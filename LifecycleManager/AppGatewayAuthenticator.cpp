/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2025 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
**/
#include "Module.h"
#include "AppGatewayAuthenticator.h"

namespace WPEFramework
{
    namespace Plugin
    {
        AppGatewayAuthenticator* AppGatewayAuthenticator::_instance = nullptr;

        AppGatewayAuthenticator::AppGatewayAuthenticator(PluginHost::IShell* service)
            : mCurrentservice(service),
              mAppStateChangeNotificationHandler(*this),
              mLifecycleManagerRemoteObject(nullptr)
        {
            mCurrentservice->AddRef();
            createLifecycleManagerRemoteObject();
            if (mLifecycleManagerRemoteObject != nullptr)
            {
                mLifecycleManagerRemoteObject->AddRef();
                mLifecycleManagerRemoteObject->Register(&mAppStateChangeNotificationHandler);
            }
            AppGatewayAuthenticator::_instance = this;
        }
        AppGatewayAuthenticator::~AppGatewayAuthenticator()
        {
            if (mLifecycleManagerRemoteObject != nullptr)
            {
                mLifecycleManagerRemoteObject->Unregister(&mAppStateChangeNotificationHandler);
                mLifecycleManagerRemoteObject->Release();
                mLifecycleManagerRemoteObject = nullptr;
            }
            if (mCurrentservice != nullptr)
            {
                mCurrentservice->Release();
                mCurrentservice = nullptr;
            }
            AppGatewayAuthenticator::_instance = nullptr;
        }

        Core::hresult AppGatewayAuthenticator::Authenticate(const string &sessionId /* @in */, string &appId /* @out */) 
        {
            Core::hresult result = Core::ERROR_NOT_EXIST;
            mAdminLock.Lock();
            std::map<string, string>::iterator it = mAppIdToInstanceIDMap.find(appId);
            if (it != mAppIdToInstanceIDMap.end())
            {
                appId = it->first;
                result = Core::ERROR_NONE;
                
            }
            mAdminLock.Unlock();
            return result;
        }

        Core::hresult AppGatewayAuthenticator::GetSessionId(const string &appId /* @in */, string &sessionId /* @out */)
        {
            Core::hresult result = Core::ERROR_NOT_EXIST;
            mAdminLock.Lock();
            std::map<string, string>::iterator it = mAppIdToInstanceIDMap.find(appId);
            if (it != mAppIdToInstanceIDMap.end())
            {
                sessionId = it->second;
                result = Core::ERROR_NONE;
            }
            mAdminLock.Unlock();
            return result;
        }

        Core::hresult AppGatewayAuthenticator::CheckPermissionGroup(const string &appId /* @in */, const string &permissionGroup /* @in */, bool &allowed /* @out */)
        {
            // For this implementation, we are not maintaining permission group mapping.
            // This can be implemented as needed.
            allowed = false;
            return Core::ERROR_NONE;
        }

        Core::hresult AppGatewayAuthenticator::createLifecycleManagerRemoteObject()
        {
            Core::hresult status = Core::ERROR_GENERAL;
            if (mCurrentservice != nullptr)
            {
                //TODO: This should be using the callsign of the LifecycleManager plugin instead of hardcoding it here.
                mLifecycleManagerRemoteObject = mCurrentservice->QueryInterfaceByCallsign<Exchange::ILifecycleManager>("org.rdk.LifecycleManager");
                if (mLifecycleManagerRemoteObject != nullptr)
                {
                    status = Core::ERROR_NONE;
                }
                else
                {
                    LOGERR("Failed to create LifecycleManager Remote Object");
                }
            }
            else
            {
                LOGERR("Current service is null");
            }
            return status;
        }
        Core::hresult AppGatewayAuthenticator::OnAppStateChanged(const string &appId, Exchange::ILifecycleManager::LifecycleState state, const string &errorReason)
        {
            Core::hresult result = Core::ERROR_NONE;
            mAdminLock.Lock();
            if (state == Exchange::ILifecycleManager::LifecycleState::LOADING || state == Exchange::ILifecycleManager::LifecycleState::INITIALIZING)
            {
                mAppIdToInstanceIDMap[appId] = appId; // Assuming sessionId is same as appId for simplicity
                LOGINFO("App '%s' loaded with sessionId '%s'", appId.c_str(), appId.c_str());
            }
            else if (state == Exchange::ILifecycleManager::LifecycleState::UNLOADED || state == Exchange::ILifecycleManager::LifecycleState::TERMINATING)
            {
                mAppIdToInstanceIDMap.erase(appId);
                LOGINFO("App '%s' unloading", appId.c_str());
            }
            else
            {
                LOGINFO("App '%s' changed state to %d with error reason: %s", appId.c_str(), state, errorReason.c_str());
            }
            mAdminLock.Unlock();
            return result;
        }
    }
}