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

#include "SceneSet.h"
#include <interfaces/IAppManager.h>

namespace WPEFramework {
namespace Plugin {
    
    namespace {
        
        static Metadata<SceneSet>metadata(
            // Version
            1, 0, 0,
            // Preconditions
            {},
            // Terminations
            {},
            // Controls
            {}
        );
    }
    
    
    const string SceneSet::Initialize(PluginHost::IShell* service) {
        string message;
        ASSERT(service != nullptr);
        ASSERT(mService == nullptr);
        ASSERT(mConnectionId == 0);
        mService = service;
        mService->AddRef();
	LOGINFO();
        mAppManager = mService->QueryInterfaceByCallsign<Exchange::IAppManager>("org.rdk.AppManager");
        if (!mAppManager) {
            SYSLOG(Logging::Startup, ("SceneSet: Failed to get AppManager interface"));
        }
       	// Start Reference App on plugin startup
     	StartReferenceApp();

        return (message);
    }
    
    void SceneSet::Deinitialize(PluginHost::IShell* service) {
	
	LOGINFO();
        if (mService != nullptr) {
            ASSERT(mService == service);
            mService->Release();
            mService = nullptr;
            mConnectionId = 0;
            if (mAppManager) {
                mAppManager->Release();
                mAppManager = nullptr;
            }
            SYSLOG(Logging::Shutdown, (string(_T("SceneSet de-initialised"))));
        }

    }
    
    string SceneSet::Information() const {
        return (string());
    }
    
    
    void SceneSet::StartReferenceApp()
    {
        // Launch Reference App using AppManagerImplementation directly
        LOGINFO();
        if (mAppManager) {
	    LOGINFO();
            Core::hresult result = mAppManager->LaunchApp("com.rdk.cobalt", "", "");
            if (result == Core::ERROR_NONE) {
                SYSLOG(Logging::Startup, ("SceneSet: Reference App launched successfully"));
            } else {
                SYSLOG(Logging::Startup, ("SceneSet: Failed to launch Reference App"));
            }
        } else {
            SYSLOG(Logging::Startup, ("SceneSet: AppManager instance not available"));
        }
    }

    void SceneSet::MonitorReferenceAppCrash()
    {
    	LOGINFO();
    }

    void SceneSet::RestartReferenceApp()
    {
        // Listen for lifecycle state changes and restart Reference App if needed
        SYSLOG(Logging::Startup, ("SceneSet: Reference App crashed, restarting..."));
    }
    
    
} // Plugin
} // WPEFramework
