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
#include "UtilsLogging.h"

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
        SYSLOG(Logging::Startup, (_T("SharedStorage Constructor")));
    }
    SharedStorage::~SharedStorage()
    {
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
        _sharedStorage = Core::Service<Implementation>::Create<Exchange::IStore2>();

        if (nullptr != _sharedStorage)
        {
            Exchange::JStore2::Register(*this, _sharedStorage);

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
            LOGINFO("SharedStorage GetKeys sharedInspector: %p", _sharedInspector);
            _sharedLimit = _sharedStorage->QueryInterface<Exchange::IStoreLimit>();

            ASSERT(_sharedCache != nullptr);
            ASSERT(_sharedInspector != nullptr);
            ASSERT(_sharedLimit != nullptr);

            if(_sharedCache != nullptr)
            {
                Exchange::JStoreCache::Register(*this, _sharedCache);
            }
            else
            {
                message = _T("SharedStorage implementation did not provide a IStoreCache interface");
            }

            if(_sharedInspector != nullptr)
            {
                Exchange::JStoreInspector::Register(*this, _sharedInspector);
            }
            else
            {
                message = _T("SharedStorage implementation did not provide a IStoreInspector interface");
            }

            if(_sharedLimit != nullptr)
            {
                Exchange::JStoreLimit::Register(*this, _sharedLimit);
            }
            else
            {
                message = _T("SharedStorage implementation did not provide a IStoreLimit interface");
            }

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

    SharedStorage::Implementation::Implementation()
        : m_PersistentStoreRef(nullptr),
          m_CloudStoreRef(nullptr),
          _psCache(nullptr),
          _psInspector(nullptr),
          _psLimit(nullptr),
          _storeNotification(*this),
          _psObject(nullptr),
          _csObject(nullptr),
          _service(nullptr)
    {

    }

    uint32_t SharedStorage::Implementation::Register(Exchange::IStore2::INotification* client)
    {
        Core::SafeSyncType<Core::CriticalSection> lock(_clientLock);
        _clients.push_back(client);
        return Core::ERROR_NONE;
    }

    uint32_t SharedStorage::Implementation::Unregister(Exchange::IStore2::INotification* client)
    {
        Core::SafeSyncType<Core::CriticalSection> lock(_clientLock);
        _clients.remove(client);
        return Core::ERROR_NONE;
    }

    SharedStorage::Implementation::~Implementation()
    {
        
        if (nullptr != m_PersistentStoreRef)
        {
            m_PersistentStoreRef->Release();
            m_PersistentStoreRef = nullptr;
        }
        // Disconnect from the interface
        if(_psObject)
        {
            _psObject->Unregister(&_storeNotification);
            _psObject->Release();
            _psObject = nullptr;
        }
        if(_psInspector)
        {
            _psInspector->Release();
            _psInspector = nullptr;
        }
        if(_psLimit)
        {
            _psLimit->Release();
            _psLimit = nullptr;
        }
        // Disconnect from the COM-RPC socket
        if (nullptr != m_CloudStoreRef)
        {
            m_CloudStoreRef->Release();
            m_CloudStoreRef = nullptr;
        }
        if(_psCache)
        {
            _psCache->Release();
            _psCache = nullptr;
        }
        if(_csObject)
        {
            _csObject->Unregister(&_storeNotification);
            _csObject->Release();
            _csObject = nullptr;
        }
    }

    uint32_t SharedStorage::Implementation::Configure(PluginHost::IShell* service)
    {
        uint32_t result = Core::ERROR_GENERAL;
        string message = "";

        if (service != nullptr)
        {
            _service = service;
            _service->AddRef();
            result = Core::ERROR_NONE;

            m_PersistentStoreRef = service->QueryInterfaceByCallsign<PluginHost::IPlugin>("org.rdk.PersistentStore");
            if (nullptr != m_PersistentStoreRef)
            {
                // Get interface for IStore2
                _psObject = m_PersistentStoreRef->QueryInterface<Exchange::IStore2>();
                // Get interface for IStoreInspector
                _psInspector = m_PersistentStoreRef->QueryInterface<Exchange::IStoreInspector>();
                LOGINFO("SharedStorage Configure psInspector: %p", _psInspector);
                // Get interface for IStoreLimit
                _psLimit = m_PersistentStoreRef->QueryInterface<Exchange::IStoreLimit>();
                // Get interface for IStoreCache
                _psCache = m_PersistentStoreRef->QueryInterface<Exchange::IStoreCache>();
                if ((nullptr == _psObject) || (nullptr == _psInspector) || (nullptr == _psLimit) || (nullptr == _psCache))
                {
                    message = _T("SharedStorage plugin could not be initialized.");
                    TRACE(Trace::Error, (_T("%s: Can't get PersistentStore interface"), __FUNCTION__));
                    m_PersistentStoreRef->Release();
                    m_PersistentStoreRef = nullptr;
                }
                else
                {
                    _psObject->Register(&_storeNotification);
                }
            }
            else
            {
                message = _T("SharedStorage plugin could not be initialized.");
                TRACE(Trace::Error, (_T("%s: Can't get PersistentStore interface"), __FUNCTION__));
            }

            // Establish communication with CloudStore
            m_CloudStoreRef = service->QueryInterfaceByCallsign<PluginHost::IPlugin>("org.rdk.CloudStore");
            if (nullptr != m_CloudStoreRef)
            {
                // Get interface for IStore2
                _csObject = m_CloudStoreRef->QueryInterface<Exchange::IStore2>();
                if (nullptr == _csObject)
                {
                    // message = _T("SharedStorage plugin could not be initialized.");
                    TRACE(Trace::Error, (_T("%s: Can't get CloudStore interface"), __FUNCTION__));
                    m_CloudStoreRef->Release();
                    m_CloudStoreRef = nullptr;
                }
                else
                {
                    _csObject->Register(&_storeNotification);
                }
            }
            else
            {
                // message = _T("SharedStorage plugin could not be initialized.");
                TRACE(Trace::Error, (_T("%s: Can't get CloudStore interface"), __FUNCTION__));
            }
        }

        return result;
    }

    void SharedStorage::Implementation::ValueChanged(const Exchange::IStore2::ScopeType scope, const string& ns, const string& key, const string& value)
    {
        Core::SafeSyncType<Core::CriticalSection> lock(_clientLock);
        for (auto* client : _clients)
        {
            client->ValueChanged(scope, ns, key, value);
        }
    }


    Exchange::IStore2* SharedStorage::Implementation::getRemoteStoreObject(Exchange::IStore2::ScopeType eScope)
    {
        if( (eScope == Exchange::IStore2::ScopeType::DEVICE) && _psObject)
        {
            return _psObject;
        }
        else if( (eScope == Exchange::IStore2::ScopeType::ACCOUNT) && _csObject)
        {
            return _csObject;
        }
        else
        {
            TRACE(Trace::Error, (_T("%s: Unknown scope: %d"), __FUNCTION__, static_cast<int>(eScope)));
            return nullptr;
        }
    }

    uint32_t SharedStorage::Implementation::SetValue(const IStore2::ScopeType scope, const string& ns, const string& key, const string& value, const uint32_t ttl)
    {
        uint32_t status = Core::ERROR_NOT_SUPPORTED;
        Exchange::IStore2* store = nullptr;

        store = getRemoteStoreObject(scope);
        if (store != nullptr)
        {
            status = store->SetValue(scope, ns, key, value, ttl);
        }

        return status;
    }

    uint32_t SharedStorage::Implementation::GetValue(const IStore2::ScopeType scope, const string& ns, const string& key, string& value, uint32_t& ttl)
    {
        uint32_t status = Core::ERROR_NOT_SUPPORTED;
        Exchange::IStore2* store = nullptr;

        store = getRemoteStoreObject(scope);
        if (store != nullptr)
        {
            status = store->GetValue(scope, ns, key, value, ttl);
        }

        return status;
    }

    uint32_t SharedStorage::Implementation::DeleteKey(const IStore2::ScopeType scope, const string& ns, const string& key)
    {
        uint32_t status = Core::ERROR_NOT_SUPPORTED;
        Exchange::IStore2* store = nullptr;

        store = getRemoteStoreObject(scope);
        if (store != nullptr)
        {
            status = store->DeleteKey(scope, ns, key);
        }

        return status;
    }

    uint32_t SharedStorage::Implementation::DeleteNamespace(const IStore2::ScopeType scope, const string& ns)
    {
        uint32_t status = Core::ERROR_NOT_SUPPORTED;
        Exchange::IStore2* store = nullptr;

        store = getRemoteStoreObject(scope);
        if (store != nullptr)
        {
            status = store->DeleteNamespace(scope, ns);
        }

        return status;
    }

    uint32_t SharedStorage::Implementation::FlushCache()
    {
        uint32_t status = Core::ERROR_NOT_SUPPORTED;

        if (_psCache != nullptr)
        {
            status = _psCache->FlushCache();
        }

        return status;
    }

    uint32_t SharedStorage::Implementation::GetKeys(const IStore2::ScopeType scope, const string& ns, RPC::IStringIterator*& keys)
    {
        uint32_t status = Core::ERROR_NOT_SUPPORTED;
        LOGINFO("SharedStorage GetKeys called");

        if(scope != IStore2::ScopeType::DEVICE)
        {
            LOGINFO("SharedStorage GetKeys Scope does not equal device");
            return status;
        }

        LOGINFO("SharedStorage GetKeys psObject: %p", _psObject);

        if (_psObject != nullptr)
        {
            Exchange::IStoreInspector* _inspector = _psObject->QueryInterface<Exchange::IStoreInspector>();

            if(_inspector != nullptr)
            {
                status = _inspector->GetKeys(scope, ns, keys);
                LOGINFO("SharedStorage GetKeys _inspector getkeys called, inspector: %p (status: %d, keys: %p)", _inspector, status, keys);
                _inspector->Release();
            }
            else
            {
                LOGINFO("SharedStorage getkeys queryinterface to IStoreInspector failed");
            }
        }
        else
        {
            LOGINFO("SharedStorage GetKeys psObject is null");
        }

        return status;
    }

    uint32_t SharedStorage::Implementation::GetNamespaces(const IStore2::ScopeType scope, RPC::IStringIterator*& namespaces)
    {
        uint32_t status = Core::ERROR_NOT_SUPPORTED;

        if(scope != IStore2::ScopeType::DEVICE)
        {
            return status;
        }

        if (_psInspector != nullptr)
        {
            status = _psInspector->GetNamespaces(scope, namespaces);
        }

        return status;
    }

    uint32_t SharedStorage::Implementation::GetStorageSizes(const IStore2::ScopeType scope, INamespaceSizeIterator*& storageList)
    {
        uint32_t status = Core::ERROR_NOT_SUPPORTED;

        if(scope != IStore2::ScopeType::DEVICE)
        {
            return status;
        }

        if (_psInspector != nullptr)
        {
            status = _psInspector->GetStorageSizes(scope, storageList);
        }
        return status;
    }

    uint32_t SharedStorage::Implementation::SetNamespaceStorageLimit(const IStore2::ScopeType scope, const string& ns, const uint32_t size)
    {
        uint32_t status = Core::ERROR_NOT_SUPPORTED;

        if(scope != IStore2::ScopeType::DEVICE)
        {
            return status;
        }

        if (_psLimit != nullptr)
        {
            status = _psLimit->SetNamespaceStorageLimit(scope, ns, size);
        }
        return status;
    }

    uint32_t SharedStorage::Implementation::GetNamespaceStorageLimit(const IStore2::ScopeType scope, const string& ns, uint32_t& size)
    {
        uint32_t status = Core::ERROR_NOT_SUPPORTED;

        if(scope != IStore2::ScopeType::DEVICE)
        {
            return status;
        }

        if (_psLimit != nullptr)
        {
            status = _psLimit->GetNamespaceStorageLimit(scope, ns, size);
        }
        return status;
    }

} // namespace Plugin
} // namespace WPEFramework
