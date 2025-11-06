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
#include "UtilsCallsign.h"
#include "StringUtils.h"
#include "UtilsLogging.h"
#include "UtilsFirebolt.h"

namespace WPEFramework
{
        
    ENUM_CONVERSION_BEGIN(Exchange::ILifecycleManager::LifecycleState)
    {Exchange::ILifecycleManager::LifecycleState::UNLOADED, _TXT("unloaded")},
    {Exchange::ILifecycleManager::LifecycleState::LOADING, _TXT("loading")},
    {Exchange::ILifecycleManager::LifecycleState::INITIALIZING, _TXT("initializing")},
    {Exchange::ILifecycleManager::LifecycleState::PAUSED, _TXT("paused")},
    {Exchange::ILifecycleManager::LifecycleState::ACTIVE, _TXT("active")},
    {Exchange::ILifecycleManager::LifecycleState::SUSPENDED, _TXT("suspended")},
    {Exchange::ILifecycleManager::LifecycleState::HIBERNATED, _TXT("hibernated")},
    {Exchange::ILifecycleManager::LifecycleState::TERMINATING, _TXT("terminating")},
    ENUM_CONVERSION_END(Exchange::ILifecycleManager::LifecycleState)
    
    namespace Plugin
    {
        const std::string LifecycleStateToString(Exchange::ILifecycleManager::LifecycleState state)
        {
            WPEFramework::Core::EnumerateType<Exchange::ILifecycleManager::LifecycleState> type(state);
            return type.Data();
        }

        const bool StringToLifecycleState(const std::string& state, Exchange::ILifecycleManager::LifecycleState& value)
        {
            WPEFramework::Core::EnumerateType<Exchange::ILifecycleManager::LifecycleState> type(state.c_str(), false);
            if (type.IsSet()) {
                value = type.Value();
                return true;
            }
            return false;
        }

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
            mAppIdInstanceMap[appId] = appInstanceId;
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
            std::string lowerMethod = StringUtils::toLower(method);
            // Route System/Device methods
            if (lowerMethod == "lifecycle2test.settargetstate")
            {
                Exchange::ILifecycleManager* lifecycleManager = mCurrentservice->QueryInterfaceByCallsign<Exchange::ILifecycleManager>("org.rdk.LifecycleManager");
                if (lifecycleManager != nullptr) {
                    // Handle the set target state request here
                    JsonObject params;
                    if (params.FromString(payload)) {
                        // Process params and call lifecycleManager methods as needed
                        std::string state = params["state"].String();
                        std::string appInstanceId = params["appInstanceId"].String();
                        std::string intent = params["intent"].String();
                        Exchange::ILifecycleManager::LifecycleState targetState;
                        if (!StringToLifecycleState(state, targetState)) {
                            ErrorUtils::CustomInternal("Invalid target state", result);
                            return Core::ERROR_GENERAL;
                        }
                        if (Core::ERROR_NONE != lifecycleManager->SetTargetAppState(appInstanceId, targetState, intent)) {
                            ErrorUtils::CustomInternal("Couldnt set target state", result);
                            return Core::ERROR_GENERAL;
                        }
                    }
                    else {
                        LOGERR("Failed to parse payload for set target state");
                    }
                }
            }
            result = "null";
            return Core::ERROR_NONE;
        }

        void AppGatewayAuthenticator::WindowManagerNotification::OnFocus(const std::string& appInstanceId) {
            _parent.mAdminLock.Lock();
            _parent.mAppFocusMap[appInstanceId] = true;
            _parent.mAdminLock.Unlock();
            _parent.HandleActivePassive(appInstanceId, true);
        }
        void AppGatewayAuthenticator::WindowManagerNotification::OnBlur(const std::string& appInstanceId) {
            _parent.mAdminLock.Lock();
            _parent.mAppFocusMap[appInstanceId] = false;
            _parent.mAdminLock.Unlock();
            _parent.HandleActivePassive(appInstanceId, false);
        }

        Core::hresult AppGatewayAuthenticator::IsAppInstanceFocused(const string& appInstanceId, bool& focussed) const
        {
            Core::hresult status = Core::ERROR_GENERAL;
            mAdminLock.Lock();
            auto it = mAppFocusMap.find(appInstanceId);
            if (it != mAppFocusMap.end() && it->second)
            {
                focussed = true;
                status = Core::ERROR_NONE;
            } else {
                focussed = false;
            }
            mAdminLock.Unlock();
            return status;
        }

        void AppGatewayAuthenticator::DispatchAppLifecycleEvent(const string& appId, const string& appInstanceId)
        {
            // Implementation of dispatching app lifecycle event
            // Get AppLifecycleContext from the map
            JsonArray eventPayload;
            Exchange::ILifecycleManager::LifecycleState oldState;
            Exchange::ILifecycleManager::LifecycleState newState;
            mAdminLock.Lock();
            auto it = mAppLifeCycleContextMap.find(appId);
            if (it != mAppLifeCycleContextMap.end()) {
                const AppLifeCycleContext& context = it->second;
                oldState = context.oldState;
                newState = context.newState;
                mAdminLock.Unlock();

                bool focussed;
                IsAppInstanceFocused(appInstanceId, focussed);

                if (focussed && oldState == Exchange::ILifecycleManager::LifecycleState::ACTIVE && newState == Exchange::ILifecycleManager::LifecycleState::PAUSED) {
                    // Handle specific state transition if needed
                    JsonObject passiveState;
                    passiveState["oldState"] = LifecycleStateToString(Exchange::ILifecycleManager::LifecycleState::ACTIVE);
                    passiveState["newState"] = LifecycleStateToString(Exchange::ILifecycleManager::LifecycleState::ACTIVE);
                    passiveState["focused"] = false;
                    eventPayload.Add(passiveState);
                }

                JsonObject stateChange;
                stateChange["oldState"] = LifecycleStateToString(oldState);
                stateChange["newState"] = LifecycleStateToString(newState);
                stateChange["focused"] = focussed;
                // Further event dispatch logic here
                eventPayload.Add(stateChange);

                string payload;
                eventPayload.ToString(payload);
                Core::IWorkerPool::Instance().Submit(DispatchLifeycleJob::Create(this, appId, payload));
                
            } else {
                mAdminLock.Unlock();
                LOGERR("AppLifeCycleContext not found for appId: %s and appInstanceId %s", appId.c_str(), appInstanceId.c_str());
            }
        }

        Core::hresult AppGatewayAuthenticator::IsAppActive(const string& appInstanceId, string& appId, bool& active) const
            {
                Core::hresult status = Core::ERROR_GENERAL;
                mAdminLock.Lock();
                for (const auto& pair : mAppLifeCycleContextMap) {
                    const AppLifeCycleContext& context = pair.second;
                    if (context.appInstanceId == appInstanceId) {
                        appId = pair.first;
                        active = (context.newState == Exchange::ILifecycleManager::LifecycleState::ACTIVE);
                        status = Core::ERROR_NONE;
                        break;
                    }
                }
                mAdminLock.Unlock();
                return status;
            }

        void AppGatewayAuthenticator::HandleActivePassive(const string& appInstanceId, bool focussed)
        {
            bool active;
            string appId;
            if (IsAppActive(appInstanceId, appId, active) == Core::ERROR_NONE) {
                if (active) {
                    JsonArray eventPayload;
                    JsonObject passiveState;
                    passiveState["oldState"] = LifecycleStateToString(Exchange::ILifecycleManager::LifecycleState::ACTIVE);
                    passiveState["newState"] = LifecycleStateToString(Exchange::ILifecycleManager::LifecycleState::ACTIVE);
                    passiveState["focused"] = focussed;
                    eventPayload.Add(passiveState);
                    string payload;
                    eventPayload.ToString(payload);
                    Core::IWorkerPool::Instance().Submit(DispatchLifeycleJob::Create(this, appId, payload));
                }
            }
        }
        void AppGatewayAuthenticator::DispatchLifecycleNotification(const string& appId, const string& payload)
        {
            Exchange::IAppNotifications* appNotifications = mCurrentservice->QueryInterfaceByCallsign<Exchange::IAppNotifications>(APP_NOTIFICATIONS_CALLSIGN);
            if (appNotifications != nullptr) {
                appNotifications->Emit("Lifecycle2.onStateChanged", payload, appId);
                appNotifications->Release();
            } else {
                LOGERR("Failed to get IAppNotifications interface for appId: %s", appId.c_str());
            }
        }
    }
}