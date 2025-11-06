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
        AppGatewayAuthenticator::AppGatewayAuthenticator(PluginHost::IShell *service)
            : mCurrentservice(service),
              mAppStateChangeNotificationHandler(*this),
              mLifecycleManagerStateRemoteObject(nullptr),
              mWindowManagerNotification(*this),
              mWindowManagerRemoteObject(nullptr)
        {
            mCurrentservice->AddRef();
            if (createLifecycleManagerStateRemoteObject() == Core::ERROR_NONE)
            {
                mLifecycleManagerStateRemoteObject->AddRef();
                mLifecycleManagerStateRemoteObject->Register(&mAppStateChangeNotificationHandler);
            }
            if (createWindowManagerRemoteObject() == Core::ERROR_NONE)
            {
                mWindowManagerRemoteObject->AddRef();
                mWindowManagerRemoteObject->Register(&mWindowManagerNotification);
            }
        }
        AppGatewayAuthenticator::~AppGatewayAuthenticator()
        {
            if (mLifecycleManagerStateRemoteObject != nullptr)
            {
                mLifecycleManagerStateRemoteObject->Unregister(&mAppStateChangeNotificationHandler);
                mLifecycleManagerStateRemoteObject->Release();
                mLifecycleManagerStateRemoteObject = nullptr;
            }
            if (mWindowManagerRemoteObject != nullptr)
            {
                mWindowManagerRemoteObject->Unregister(&mWindowManagerNotification);
                mWindowManagerRemoteObject->Release();
                mWindowManagerRemoteObject = nullptr;
            }
            if (mCurrentservice != nullptr)
            {
                mCurrentservice->Release();
                mCurrentservice = nullptr;
            }
        }

        Core::hresult AppGatewayAuthenticator::Authenticate(const string &sessionId /* @in */, string &appId /* @out */)
        {
            Core::hresult result = Core::ERROR_NOT_EXIST;
            mAdminLock.Lock();
            std::map<string, AppLifeCycleContext>::iterator it = mAppLifeCycleContextMap.find(appId);
            if (it != mAppLifeCycleContextMap.end())
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
            std::map<string, AppLifeCycleContext>::iterator it = mAppLifeCycleContextMap.find(appId);
            if (it != mAppLifeCycleContextMap.end())
            {
                sessionId = it->second.appInstanceId;
                result = Core::ERROR_NONE;
            }
            mAdminLock.Unlock();
            return result;
        }

        Core::hresult AppGatewayAuthenticator::CheckPermissionGroup(const string &appId /* @in */, const string &permissionGroup /* @in */, bool &allowed /* @out */)
        {
            // For this implementation, we are not maintaining permission group mapping.
            // For the time being keeping it as allowed for all apps.
            allowed = true;
            return Core::ERROR_NONE;
        }

        Core::hresult AppGatewayAuthenticator::createLifecycleManagerStateRemoteObject()
        {
            Core::hresult status = Core::ERROR_GENERAL;
            if (mCurrentservice != nullptr)
            {
                // TODO: This should be using the callsign of the LifecycleManager plugin instead of hardcoding it here.
                mLifecycleManagerStateRemoteObject = mCurrentservice->QueryInterfaceByCallsign<Exchange::ILifecycleManagerState>("org.rdk.LifecycleManager");
                if (mLifecycleManagerStateRemoteObject != nullptr)
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

        Core::hresult AppGatewayAuthenticator::createWindowManagerRemoteObject()
        {
            Core::hresult status = Core::ERROR_GENERAL;
            if (mCurrentservice != nullptr)
            {
                mWindowManagerRemoteObject = mCurrentservice->QueryInterfaceByCallsign<Exchange::IRDKWindowManager>("org.rdk.RDKWindowManager");
                if (mWindowManagerRemoteObject != nullptr)
                {
                    status = Core::ERROR_NONE;
                }
                else
                {
                    LOGERR("Failed to create WindowManager Remote Object");
                }
            }
            else
            {
                LOGERR("Current service is null");
            }
            return status;
        }


        void AppGatewayAuthenticator::OnAppLifecycleStateChanged(const string &appId,
                                                                 const string &appInstanceId,
                                                                 const Exchange::ILifecycleManager::LifecycleState oldState,
                                                                 const Exchange::ILifecycleManager::LifecycleState newState,
                                                                 const string &navigationIntent)
        {
            LOGINFO("AppLifecycleStateChanged: AppId=%s, AppInstanceId=%s, OldState=%d, NewState=%d, NavigationIntent=%s",
                    appId.c_str(), appInstanceId.c_str(), oldState, newState, navigationIntent.c_str());
            mAdminLock.Lock();
            // If the app is in terminating state, remove it from the map
            if (newState == Exchange::ILifecycleManager::TERMINATING)
            {
                std::map<string, AppLifeCycleContext>::iterator it = mAppLifeCycleContextMap.find(appId);
                if (it != mAppLifeCycleContextMap.end())
                {
                    mAppLifeCycleContextMap.erase(it);
                }
                mAdminLock.Unlock();
                return;
            }
            AppLifeCycleContext context;
            context.appInstanceId = appInstanceId;
            context.oldState = oldState;
            context.newState = newState;
            context.navigationIntent = navigationIntent;
            mAppLifeCycleContextMap[appId] = context;
            mAdminLock.Unlock();
            
        }

        Core::hresult AppGatewayAuthenticator::HandleAppEventNotifier(const string& event /* @in */, 
            const bool& listen /* @in */, 
            bool& status /* @out */)
        {
            // Implementation of event notifier handling
            status = true;
            return Core::ERROR_NONE;
        }

        Core::hresult AppGatewayAuthenticator::HandleAppGatewayRequest(const Exchange::GatewayContext &context /* @in */,
            const string& method /* @in */,
            const string &payload /* @in @opaque */,
            string& result /*@out @opaque */)
        {
            // Implementation of app gateway request handling
            result = "{}";
            return Core::ERROR_NONE;
        }

        void AppGatewayAuthenticator::WindowManagerNotification::OnFocus(const std::string& appInstanceId) {

        }
        void AppGatewayAuthenticator::WindowManagerNotification::OnBlur(const std::string& appInstanceId) {

        }
    }
}
