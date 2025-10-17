/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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

#include "FbSettings.h"
#include <interfaces/IConfiguration.h>
#include <interfaces/json/JsonData_FbSettings.h>
#include <interfaces/json/JFbSettings.h>


#define API_VERSION_NUMBER_MAJOR    FBSETTINGS_MAJOR_VERSION
#define API_VERSION_NUMBER_MINOR    FBSETTINGS_MINOR_VERSION
#define API_VERSION_NUMBER_PATCH    FBSETTINGS_PATCH_VERSION

namespace WPEFramework {

namespace {
    static Plugin::Metadata<Plugin::FbSettings> metadata(
        // Version (Major, Minor, Patch)
        API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH,
        // Preconditions
        {},
        // Terminations
        {},
        // Controls
        {}
    );
}

namespace Plugin {
    SERVICE_REGISTRATION(FbSettings, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

    FbSettings::FbSettings(): mService(nullptr), mFbSettings(nullptr), mConnectionId(0)
    {
        SYSLOG(Logging::Startup, (_T("FbSettings Constructor")));
    }

    FbSettings::~FbSettings()
    {
        SYSLOG(Logging::Shutdown, (string(_T("FbSettings Destructor"))));
    }

    /* virtual */ const string FbSettings::Initialize(PluginHost::IShell* service)
    {
        ASSERT(service != nullptr);
        ASSERT(mFbSettings == nullptr);

        SYSLOG(Logging::Startup, (_T("FbSettings::Initialize: PID=%u"), getpid()));

        mService = service;
        mService->AddRef();
        mFbSettings = service->Root<Exchange::IFbSettings>(mConnectionId, 2000, _T("FbSettingsImplementation"));

        if (mFbSettings != nullptr) {
            auto configConnection = mFbSettings->QueryInterface<Exchange::IConfiguration>();
            if (configConnection != nullptr) {
                configConnection->Configure(service);
                configConnection->Release();
            }

            //Invoking Plugin API register to wpeframework
            Exchange::JFbSettings::Register(*this, mFbSettings);
        }
        else
        {
            SYSLOG(Logging::Startup, (_T("FbSettings::Initialize: Failed to initialise FbSettings plugin")));
        }
        // On success return empty, to indicate there is no error text.
        return ((mFbSettings != nullptr))
            ? EMPTY_STRING
            : _T("Could not retrieve the FbSettings interface.");
    }

    /* virtual */ void FbSettings::Deinitialize(PluginHost::IShell* service)
    {
        SYSLOG(Logging::Shutdown, (string(_T("FbSettings::Deinitialize"))));
        ASSERT(service == mService);

        if (mFbSettings != nullptr) {
            Exchange::JFbSettings::Unregister(*this);
            RPC::IRemoteConnection *connection(service->RemoteConnection(mConnectionId));
            VARIABLE_IS_NOT_USED uint32_t result = mFbSettings->Release();
            mFbSettings = nullptr;

            // It should have been the last reference we are releasing,
            // so it should end up in a DESCRUCTION_SUCCEEDED, if not we
            // are leaking...
            ASSERT(result == Core::ERROR_DESTRUCTION_SUCCEEDED);

            // If this was running in a (container) process...
            if (connection != nullptr)
            {
                // Lets trigger a cleanup sequence for
                // out-of-process code. Which will guard
                // that unwilling processes, get shot if
                // not stopped friendly :~)
                connection->Terminate();
                connection->Release();
            }
        }

        mConnectionId = 0;
        mService->Release();
        mService = nullptr;
        SYSLOG(Logging::Shutdown, (string(_T("FbSettings de-initialised"))));
    }

    void FbSettings::Deactivated(RPC::IRemoteConnection* connection)
    {
        if (connection->Id() == mConnectionId) {

            ASSERT(mService != nullptr);

            Core::IWorkerPool::Instance().Submit(PluginHost::IShell::Job::Create(mService, PluginHost::IShell::DEACTIVATED, PluginHost::IShell::FAILURE));
        }
    }

} // namespace Plugin
} // namespace WPEFramework