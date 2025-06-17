/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 RDK Management
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

#include "SharedStorage.h"

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 0

namespace WPEFramework {

namespace {

    static Plugin::Metadata<Plugin::SharedStorage> metadata(
        // Version (Major, Minor, Patch)
        API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH,
        // Preconditions
        {},
        // Terminations
        {},
        // Controls
        {});
}

namespace Plugin {
    using namespace JsonData::PersistentStore;

    SERVICE_REGISTRATION(SharedStorage, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

    SharedStorage::SharedStorage()
        : PluginHost::JSONRPC()
        , _service(nullptr)
        , _connectionId(0)
        , _sharedStorage(nullptr)
        , _sharedStorageNotification(this)
    {
        RegisterAll();
        SYSLOG(Logging::Startup, (_T("SharedStorage Constructor")));
    }
    SharedStorage::~SharedStorage()
    {
        UnregisterAll();
        SYSLOG(Logging::Shutdown, (string(_T("SharedStorage Destructor"))));
    }

    const string SharedStorage::Initialize(PluginHost::IShell* service)
    {
        SYSLOG(Logging::Startup, (_T("SharedStorage::Initialize: PID=%u"), getpid()));
        string message = "";

        ASSERT(service != nullptr);
        ASSERT(nullptr == _service);
        ASSERT(nullptr == _sharedStorage);
        ASSERT(0 == _connectionId);

        _service = service;
        _service->AddRef();
        _service->Register(&_sharedStorageNotification);
        _sharedStorage = _service->Root<Exchange::IStore2>(_connectionId, 5000, _T("SharedStorageImplementation"));

        if (nullptr != _sharedStorage)
        {
            configure = _sharedStorage->QueryInterface<Exchange::IConfiguration>();
            if (configure != nullptr)
            {
                uint32_t result = configure->Configure(_service);
                if (result != Core::ERROR_NONE)
                {
                    message = _T("SharedStorage could not be configured");
                }
            }
            else
            {
                message = _T("SharedStorage implementation did not provide a configuration interface");
            }

            _sharedCache = _sharedStorage->QueryInterface<Exchange::IStoreCache>();
            _sharedInspector = _sharedStorage->QueryInterface<Exchange::IStoreInspector>();
            _sharedLimit = _sharedStorage->QueryInterface<Exchange::IStoreLimit>();

            ASSERT(_sharedCache != nullptr);
            ASSERT(_sharedInspector != nullptr);
            ASSERT(_sharedLimit != nullptr);

            // Register for notifications
            _sharedStorage->Register(&_sharedStorageNotification);
        }
        else
        {
            SYSLOG(Logging::Startup, (_T("SharedStorage::Initialize: Failed to initialise SharedStorage plugin")));
            message = _T("SharedStorage plugin could not be initialised");
        }

        SYSLOG(Logging::Startup, (string(_T("SharedStorage Initialize complete"))));
        return message;
    }

    void SharedStorage::Deinitialize(PluginHost::IShell* service)
    {
        ASSERT(_service == service);
        SYSLOG(Logging::Shutdown, (string(_T("SharedStorage::Deinitialize"))));

        _service->Unregister(&_sharedStorageNotification);

        if (nullptr != _sharedStorage)
        {
            _sharedStorage->Unregister(&_sharedStorageNotification);

            if(_sharedCache)
            {
                _sharedCache->Release();
                _sharedCache = nullptr;
            }

            if(_sharedInspector)
            {
                _sharedInspector->Release();
                _sharedInspector = nullptr;
            }

            if(_sharedLimit)
            {
                _sharedLimit->Release();
                _sharedLimit = nullptr;
            }

            if(nullptr != configure)
            {
                configure->Release();
                configure = nullptr;
            }

            // Stop processing:
            RPC::IRemoteConnection* connection = service->RemoteConnection(_connectionId);
            VARIABLE_IS_NOT_USED uint32_t result = _sharedStorage->Release();

            _sharedStorage = nullptr;

            ASSERT(result == Core::ERROR_DESTRUCTION_SUCCEEDED);

            if (nullptr != connection)
            {
                connection->Terminate();
                connection->Release();
            }
        }

        _connectionId = 0;
        _service->Release();
        _service = nullptr;
        SYSLOG(Logging::Shutdown, (string(_T("SharedStorage Deinitialize complete"))));
    }

    string SharedStorage::Information() const
    {
        return (string());
    }

    void SharedStorage::Deactivated(RPC::IRemoteConnection* connection)
    {
        if (connection->Id() == _connectionId)
        {
            ASSERT(nullptr != _service);
            LOGINFO("SharedStorage Notification Deactivated");
            Core::IWorkerPool::Instance().Submit(PluginHost::IShell::Job::Create(_service, PluginHost::IShell::DEACTIVATED, PluginHost::IShell::FAILURE));
        }
    }

} // namespace Plugin
} // namespace WPEFramework
