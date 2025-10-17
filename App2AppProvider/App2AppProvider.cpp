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

#include "App2AppProvider.h"
#include <interfaces/IConfiguration.h>
#include <interfaces/json/JsonData_App2AppProvider.h>
#include <interfaces/json/JApp2AppProvider.h>


#define API_VERSION_NUMBER_MAJOR    APP2APPPROVIDER_MAJOR_VERSION
#define API_VERSION_NUMBER_MINOR    APP2APPPROVIDER_MINOR_VERSION
#define API_VERSION_NUMBER_PATCH    APP2APPPROVIDER_PATCH_VERSION

namespace WPEFramework {

namespace {
    static Plugin::Metadata<Plugin::App2AppProvider> metadata(
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
    SERVICE_REGISTRATION(App2AppProvider, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

    App2AppProvider::App2AppProvider(): mService(nullptr), mApp2AppProvider(nullptr), mConnectionId(0)
    {
        SYSLOG(Logging::Startup, (_T("App2AppProvider Constructor")));
    }

    App2AppProvider::~App2AppProvider()
    {
        SYSLOG(Logging::Shutdown, (string(_T("App2AppProvider Destructor"))));
    }

    /* virtual */ const string App2AppProvider::Initialize(PluginHost::IShell* service)
    {
        ASSERT(service != nullptr);
        ASSERT(mApp2AppProvider == nullptr);

        SYSLOG(Logging::Startup, (_T("App2AppProvider::Initialize: PID=%u"), getpid()));

        mService = service;
        mService->AddRef();
        mApp2AppProvider = service->Root<Exchange::IApp2AppProvider>(mConnectionId, 2000, _T("App2AppProviderImplementation"));

        if (mApp2AppProvider != nullptr) {
            auto configConnection = mApp2AppProvider->QueryInterface<Exchange::IConfiguration>();
            if (configConnection != nullptr) {
                configConnection->Configure(service);
                configConnection->Release();
            }

            //Invoking Plugin API register to wpeframework
            Exchange::JApp2AppProvider::Register(*this, mApp2AppProvider);
        }
        else
        {
            SYSLOG(Logging::Startup, (_T("App2AppProvider::Initialize: Failed to initialise App2AppProvider plugin")));
        }
        // On success return empty, to indicate there is no error text.
        return ((mApp2AppProvider != nullptr))
            ? EMPTY_STRING
            : _T("Could not retrieve the App2AppProvider interface.");
    }

    /* virtual */ void App2AppProvider::Deinitialize(PluginHost::IShell* service)
    {
        SYSLOG(Logging::Shutdown, (string(_T("App2AppProvider::Deinitialize"))));
        ASSERT(service == mService);

        if (mApp2AppProvider != nullptr) {
            Exchange::JApp2AppProvider::Unregister(*this);
            RPC::IRemoteConnection *connection(service->RemoteConnection(mConnectionId));
            VARIABLE_IS_NOT_USED uint32_t result = mApp2AppProvider->Release();
            mApp2AppProvider = nullptr;

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
        SYSLOG(Logging::Shutdown, (string(_T("App2AppProvider de-initialised"))));
    }

    void App2AppProvider::Deactivated(RPC::IRemoteConnection* connection)
    {
        if (connection->Id() == mConnectionId) {

            ASSERT(mService != nullptr);

            Core::IWorkerPool::Instance().Submit(PluginHost::IShell::Job::Create(mService, PluginHost::IShell::DEACTIVATED, PluginHost::IShell::FAILURE));
        }
    }

} // namespace Plugin
} // namespace WPEFramework