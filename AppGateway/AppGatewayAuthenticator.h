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

#pragma once

#include <interfaces/IAppGateway.h>
#include <interfaces/IAppNotifications.h>
#include <interfaces/ILifecycleManagerState.h>
#include <interfaces/IRDKWindowManager.h>
#include <plugins/plugins.h>
#include "UtilsLogging.h"

namespace WPEFramework
{

    namespace Plugin
    {
        class AppGatewayAuthenticator : public Exchange::IAppGatewayAuthenticator, public Exchange::IAppGatewayRequestHandler, public Exchange::IAppNotificationHandler
        {
            struct AppLifeCycleContext
            {
                string appInstanceId;
                Exchange::ILifecycleManager::LifecycleState oldState;
                Exchange::ILifecycleManager::LifecycleState newState;
                string navigationIntent;
            };

        public:
            class AppStateChangeNotificationHandler : public Exchange::ILifecycleManagerState::INotification
            {

            public:
                explicit AppStateChangeNotificationHandler(AppGatewayAuthenticator &parent)
                    : mParent(parent)
                {
                }
                ~AppStateChangeNotificationHandler() override = default;

                void OnAppLifecycleStateChanged(const string &appId,
                                                const string &appInstanceId,
                                                const Exchange::ILifecycleManager::LifecycleState oldState,
                                                const Exchange::ILifecycleManager::LifecycleState newState,
                                                const string &navigationIntent) override
                {
                    mParent.OnAppLifecycleStateChanged(appId, appInstanceId, oldState, newState, navigationIntent);
                }

                BEGIN_INTERFACE_MAP(AppStateChangeNotificationHandler)
                INTERFACE_ENTRY(Exchange::ILifecycleManagerState::INotification)
                END_INTERFACE_MAP

            private:
                AppGatewayAuthenticator &mParent;
            };

            class WindowManagerNotification : public Exchange::IRDKWindowManager::INotification
            {
                private:
                    WindowManagerNotification(const AppGatewayAuthenticator&) = delete;
                    WindowManagerNotification& operator=(const AppGatewayAuthenticator&) = delete;

                public:
                    explicit WindowManagerNotification(AppGatewayAuthenticator& parent)
                        : _parent(parent)
                    {
                    }

                    ~WindowManagerNotification() override = default;
                    BEGIN_INTERFACE_MAP(WindowManagerNotification)
                    INTERFACE_ENTRY(Exchange::IRDKWindowManager::INotification)
                    END_INTERFACE_MAP

                    virtual void OnFocus(const std::string& appInstanceId) override;
                    virtual void OnBlur(const std::string& appInstanceId) override;

                private:
                    AppGatewayAuthenticator& _parent;
            };

            

            AppGatewayAuthenticator(PluginHost::IShell *service);
            AppGatewayAuthenticator(const AppGatewayAuthenticator &) = delete;
            AppGatewayAuthenticator &operator=(const AppGatewayAuthenticator &) = delete;

            virtual ~AppGatewayAuthenticator() override;

            // IAppGatewayAuthenticator interface
            Core::hresult Authenticate(const string &sessionId /* @in */, string &appId /* @out */) override;
            Core::hresult GetSessionId(const string &appId /* @in */, string &sessionId /* @out */) override;
            Core::hresult CheckPermissionGroup(const string &appId /* @in */,
                                               const string &permissionGroup /* @in */,
                                               bool &allowed /* @out */) override;
            

            // IAppNotificationHandler Interface
            Core::hresult HandleAppEventNotifier(const string& event /* @in */, 
                const bool& listen /* @in */, 
                bool& status /* @out */) override;
            
            // IAppGatewayRequestHandler Interface
            Core::hresult HandleAppGatewayRequest(const Exchange::GatewayContext &context /* @in */,
                                          const string& method /* @in */,
                                          const string &payload /* @in @opaque */,
                                          string& result /*@out @opaque */) override;

        private:
            mutable Core::CriticalSection mAdminLock;
            mutable uint32_t mRefCount;
            std::map<string, AppLifeCycleContext> mAppLifeCycleContextMap;
            PluginHost::IShell *mCurrentservice;
            Core::Sink<AppStateChangeNotificationHandler> mAppStateChangeNotificationHandler;
            Exchange::ILifecycleManagerState *mLifecycleManagerStateRemoteObject;
            std::map<string, bool> mAppFocusMap;
            Core::Sink<WindowManagerNotification> mWindowManagerNotification;
            Exchange::IRDKWindowManager *mWindowManagerRemoteObject;
            
            
            void OnAppLifecycleStateChanged(const string &appId,
                                            const string &appInstanceId,
                                            const Exchange::ILifecycleManager::LifecycleState oldState,
                                            const Exchange::ILifecycleManager::LifecycleState newState,
                                            const string &navigationIntent);
            Core::hresult createLifecycleManagerStateRemoteObject();
            Core::hresult createWindowManagerRemoteObject();

        public /*members*/:
            BEGIN_INTERFACE_MAP(AppGatewayAuthenticator)
            INTERFACE_ENTRY(Exchange::IAppGatewayAuthenticator)
            INTERFACE_ENTRY(Exchange::IAppGatewayRequestHandler)
            INTERFACE_ENTRY(Exchange::IAppNotificationHandler)
            END_INTERFACE_MAP
            uint32_t AddRef() const override
            {
                Core::InterlockedIncrement(mRefCount);
                return Core::ERROR_NONE;
            }
            uint32_t Release() const override
            {
                uint32_t result = Core::ERROR_NONE;
                if (Core::InterlockedDecrement(mRefCount) == 0)
                {
                    delete this;
                    result = Core::ERROR_DESTRUCTION_SUCCEEDED;
                }
                return (result);
            }

        }; // class AppGatewayAuthenticator
    }   // namespace Plugin
} /* namespace WPEFramework */
