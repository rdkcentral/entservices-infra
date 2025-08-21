/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
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
*/

#pragma once

#include "Module.h"
#include <interfaces/IAppManager.h>
#include "UtilsJsonRpc.h"
#include <string>
#include <iostream>


namespace WPEFramework {
namespace Plugin {
    
    class SceneSet : public PluginHost::IPlugin {
    public:
        SceneSet(const SceneSet&) = delete;
        SceneSet& operator=(const SceneSet&) = delete;
        SceneSet(SceneSet&&) = delete;
        SceneSet& operator=(SceneSet&&) = delete;
        
        SceneSet()
            : PluginHost::IPlugin()
            , mConnectionId(0)
            , mService(nullptr)
            , mAppManager(nullptr)
            , mReferenceAppId("rdk-reference-app") // Reference App ID
        {
        }
        
        ~SceneSet() override = default;
        
    public:
        // IPlugin Methods
        const string Initialize(PluginHost::IShell* service) override;
        void Deinitialize(PluginHost::IShell* service) override;
        string Information() const override;
        
        
        BEGIN_INTERFACE_MAP(SceneSet)
            INTERFACE_ENTRY(PluginHost::IPlugin)
        END_INTERFACE_MAP
        
    private:
        
       uint32_t mConnectionId;
            PluginHost::IShell* mService;

            // App Manager interface pointer
            Exchange::IAppManager* mAppManager;

            // Reference App ID
            std::string mReferenceAppId;

            // Handle App Manager lifecycle state changes
            //void OnAppLifecycleStateChanged(const std::string& appId, Exchange::IAppManager::AppLifecycleState newState, Exchange::IAppManager::AppErrorReason errorReason);

            // Placeholder for starting reference app
            void StartReferenceApp();

            // Placeholder for monitoring Reference App crashes
            void MonitorReferenceAppCrash();

            // Placeholder for restarting Reference App
            void RestartReferenceApp(); 
        
    };
} // Plugin
} // WPEFramework
