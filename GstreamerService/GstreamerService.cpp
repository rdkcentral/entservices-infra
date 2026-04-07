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

#include "GstreamerService.h"
#include <interfaces/IConfiguration.h>


#define API_VERSION_NUMBER_MAJOR    GSTREAMERSERVICE_MAJOR_VERSION
#define API_VERSION_NUMBER_MINOR    GSTREAMERSERVICE_MINOR_VERSION
#define API_VERSION_NUMBER_PATCH    GSTREAMERSERVICE_PATCH_VERSION

namespace WPEFramework {

namespace {
    static Plugin::Metadata<Plugin::GstreamerService> metadata(
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
    SERVICE_REGISTRATION(GstreamerService, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

    GstreamerService::GstreamerService()
        : _service(nullptr)
        , _gstreamerService(nullptr)
        , _connectionId(0)
        , _notification(this)
    {
        SYSLOG(Logging::Startup, (_T("GstreamerService Constructor")));
    }

    GstreamerService::~GstreamerService()
    {
        SYSLOG(Logging::Shutdown, (string(_T("GstreamerService Destructor"))));
    }

    /* virtual */ const string GstreamerService::Initialize(PluginHost::IShell* service)
    {
        ASSERT(service != nullptr);
        ASSERT(_service == nullptr);
        ASSERT(_gstreamerService == nullptr);
        ASSERT(_connectionId == 0);

        SYSLOG(Logging::Startup, (_T("GstreamerService::Initialize: PID=%u"), getpid()));

        string message;

        _service = service;
        _service->AddRef();

        // Register for COM-RPC connection/disconnection notifications
        _service->Register(&_notification);

        _gstreamerService = service->Root<Exchange::IGstreamerService>(_connectionId, 2000, _T("GstreamerServiceImplementation"));

        if (_gstreamerService != nullptr) {
            // Register for custom plugin notifications (event relay)
            _gstreamerService->Register(&_notification);

            // Configure the implementation
            Exchange::IConfiguration* configure = _gstreamerService->QueryInterface<Exchange::IConfiguration>();
            if (configure != nullptr) {
                configure->Configure(service);
                configure->Release();
            }

            // Register JSON-RPC interface
            Exchange::JGstreamerService::Register(*this, _gstreamerService);
        }
        else
        {
            message = _T("GstreamerService could not be instantiated");
            SYSLOG(Logging::Startup, (_T("GstreamerService::Initialize: Failed to initialize GstreamerService plugin")));
        }

        // On success return empty, to indicate there is no error text.
        return message;
    }

    /* virtual */ void GstreamerService::Deinitialize(PluginHost::IShell* service)
    {
        SYSLOG(Logging::Shutdown, (string(_T("GstreamerService::Deinitialize"))));
        ASSERT(service == _service);

        // Unregister from the Framework Shell (stops state change events)
        _service->Unregister(&_notification);

        if (_gstreamerService != nullptr) {
            // Unregister from the Target Plugin (stops custom events)
            _gstreamerService->Unregister(&_notification);

            // Unregister JSON-RPC interface
            Exchange::JGstreamerService::Unregister(*this);

            // Release the implementation
            VARIABLE_IS_NOT_USED uint32_t result = _gstreamerService->Release();
            _gstreamerService = nullptr;

            // It should have been the last reference we are releasing,
            // so it should end up in a DESTRUCTION_SUCCEEDED, if not we
            // are leaking...
            ASSERT(result == Core::ERROR_DESTRUCTION_SUCCEEDED);

            // If this was running in a (container) process...
            RPC::IRemoteConnection* connection = service->RemoteConnection(_connectionId);
            if (connection != nullptr)
            {
                // Lets trigger the cleanup sequence for
                // out-of-process code, which ensures that
                // unresponsive processes are terminated
                // if they do not stop gracefully.
                connection->Terminate();
                connection->Release();
            }
        }

        _connectionId = 0;

        _service->Release();
        _service = nullptr;

        SYSLOG(Logging::Shutdown, (string(_T("GstreamerService de-initialized"))));
    }

    void GstreamerService::Deactivated(RPC::IRemoteConnection* connection)
    {
        if (connection->Id() == _connectionId) {

            ASSERT(_service != nullptr);

            Core::IWorkerPool::Instance().Submit(PluginHost::IShell::Job::Create(_service, PluginHost::IShell::DEACTIVATED, PluginHost::IShell::FAILURE));
        }
    }

} // namespace Plugin
} // namespace WPEFramework
