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
#include <interfaces/ILifecycleManager.h>
#include <plugins/plugins.h>
#include "UtilsLogging.h"

namespace WPEFramework
{
    namespace Plugin
    {

        class AppGatewayAuthenticator : public Exchange::IAppGatewayAuthenticator
        {
            class AppStateChangeNotificationHandler : public Exchange::ILifecycleManager::INotification
            {

            public:
                AppStateChangeNotificationHandler(AppGatewayAuthenticator &parent)
                    : mParent(parent)
                {
                }
                ~AppStateChangeNotificationHandler() {}

                void OnAppStateChanged(const string &appId, Exchange::ILifecycleManager::LifecycleState state, const string &errorReason)
                {
                    mParent.OnAppStateChanged(appId, state, errorReason);
                }

                BEGIN_INTERFACE_MAP(AppStateChangeNotificationHandler)
                INTERFACE_ENTRY(Exchange::ILifecycleManager::INotification)
                END_INTERFACE_MAP

            private:
                AppGatewayAuthenticator &mParent;
            };

        public:
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

        private:
            mutable Core::CriticalSection mAdminLock;
            std::map<string, string> mAppIdToInstanceIDMap; // Map of appId to sessionId
            PluginHost::IShell *mCurrentservice;
            Core::Sink<AppStateChangeNotificationHandler> mAppStateChangeNotificationHandler;
            Exchange::ILifecycleManager *mLifecycleManagerRemoteObject;
            Core::hresult OnAppStateChanged(const string &appId, Exchange::ILifecycleManager::LifecycleState state, const string &errorReason);
            Core::hresult createLifecycleManagerRemoteObject();

        public /*members*/:
            static AppGatewayAuthenticator *_instance;
        }; // class AppGatewayAuthenticator
    } /* namespace Plugin */
} /* namespace WPEFramework */
