/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2024 RDK Management
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
*/

#pragma once

#include <map>
#include <interfaces/IRuntimeManager.h>
#include <plugins/plugins.h>
#include "IEventHandler.h"
#include "ApplicationContext.h"

namespace WPEFramework {
namespace Plugin {
    class RuntimeManagerHandler
    {
        class RuntimeManagerNotification : public Exchange::IRuntimeManager::INotification
	{
            private:
                RuntimeManagerNotification(const RuntimeManagerNotification&) = delete;
                RuntimeManagerNotification& operator=(const RuntimeManagerNotification&) = delete;

            public:
                explicit RuntimeManagerNotification(RuntimeManagerHandler& parent)
                    : _parent(parent)
                {
                }
                ~RuntimeManagerNotification() override = default;
/*
                void OnDevicePluggedIn(const USBDevice &device)
                {
                    _parent.OnDevicePluggedIn(device);
                }

                void OnDevicePluggedOut(const USBDevice &device)
                {
                    _parent.OnDevicePluggedOut(device);
                }
*/
                BEGIN_INTERFACE_MAP(RuntimeManagerNotification)
                INTERFACE_ENTRY(Exchange::IRuntimeManager::INotification)
                END_INTERFACE_MAP

            private:
                RuntimeManagerHandler& _parent;
        };

        public:
            RuntimeManagerHandler();
            ~RuntimeManagerHandler();

            // We do not allow this plugin to be copied !!
            RuntimeManagerHandler(const RuntimeManagerHandler&) = delete;
            RuntimeManagerHandler& operator=(const RuntimeManagerHandler&) = delete;

        public:
            bool initialize(PluginHost::IShell* service, IEventHandler* eventHandler);
	    void deinitialize();
            bool run(const string& appInstanceId, const string& appPath, const string& appConfig, const string& runtimeAppId, const string& runtimePath, const string& runtimeConfig, const string& environmentVars, const bool enableDebugger, const string& launchArgs, const string& xdgRuntimeDir, const string& displayName, string& errorReason);
            bool kill(const string& appInstanceId, string& errorReason);
            bool terminate(const string& appInstanceId, string& errorReason);
            bool suspend(const string& appInstanceId, string& errorReason);
            bool resume(const string& appInstanceId, string& errorReason);
            bool hibernate(const string& appInstanceId, string& errorReason);
            bool getRuntimeStats(const string& appInstanceId, string& info);

        private:
            Exchange::IRuntimeManager* mRuntimeManager;
            uint32_t mFireboltAccessPort;
            IEventHandler* mEventHandler;
    };
} // namespace Plugin
} // namespace WPEFramework
