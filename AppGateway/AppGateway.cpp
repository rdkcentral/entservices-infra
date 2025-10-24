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

#include "AppGateway.h"
#include <interfaces/IConfiguration.h>
#include <interfaces/json/JsonData_AppGateway.h>
#include <interfaces/json/JAppGateway.h>
#include "UtilsLogging.h"


#define API_VERSION_NUMBER_MAJOR    APPGATEWAY_MAJOR_VERSION
#define API_VERSION_NUMBER_MINOR    APPGATEWAY_MINOR_VERSION
#define API_VERSION_NUMBER_PATCH    APPGATEWAY_PATCH_VERSION


namespace WPEFramework {

namespace {
    static Plugin::Metadata<Plugin::AppGateway> metadata(
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
    SERVICE_REGISTRATION(AppGateway, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

    AppGateway::AppGateway()
            : PluginHost::JSONRPC(), mService(nullptr), mAppGateway(nullptr), mConnectionId(0)
        {

        LOGINFO("AppGateway Constructor");
    }

    AppGateway::~AppGateway()
    {
    }

    /* virtual */ const string AppGateway::Initialize(PluginHost::IShell* service)
    {
        ASSERT(service != nullptr);
        ASSERT(mAppGateway == nullptr);

        LOGINFO("AppGateway::Initialize: PID=%u", getpid());

        mService = service;
        mService->AddRef();
        mAppGateway = service->Root<Exchange::IAppGateway>(mConnectionId, 2000, _T("AppGatewayImplementation"));

        if (mAppGateway != nullptr) {
            auto configConnection = mAppGateway->QueryInterface<Exchange::IConfiguration>();
            if (configConnection != nullptr) {
                configConnection->Configure(service);
                configConnection->Release();
            }

            //Invoking Plugin API register to wpeframework
            Exchange::JAppGateway::Register(*this, mAppGateway);
        }
        else
        {
            LOGERR("Failed to initialise AppGateway plugin!");
        }
   
            
        // On success return empty, to indicate there is no error text.
        return ((mAppGateway != nullptr))
            ? EMPTY_STRING
            : _T("Could not retrieve the AppGateway interface.");
    }

    /* virtual */ void AppGateway::Deinitialize(PluginHost::IShell* service)
    {
        ASSERT(service == mService);

        if (mAppGateway != nullptr) {
            Exchange::JAppGateway::Unregister(*this);
            RPC::IRemoteConnection *connection(service->RemoteConnection(mConnectionId));
            VARIABLE_IS_NOT_USED uint32_t result = mAppGateway->Release();
            mAppGateway = nullptr;

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
    }

    void AppGateway::Deactivated(RPC::IRemoteConnection* connection)
    {
        if (connection->Id() == mConnectionId) {

            ASSERT(mService != nullptr);

            Core::IWorkerPool::Instance().Submit(PluginHost::IShell::Job::Create(mService, PluginHost::IShell::DEACTIVATED, PluginHost::IShell::FAILURE));
        }
    }

} // namespace Plugin
} // namespace WPEFramework

