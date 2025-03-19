#include "SharedStorageImplementation.h"
#include "UtilsLogging.h"

namespace WPEFramework {
namespace Plugin {

    SERVICE_REGISTRATION(SharedStorageImplementation, 1, 0);

    SharedStorageImplementation::SharedStorageImplementation()
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

    uint32_t SharedStorageImplementation::Register(Exchange::IStore2::INotification* client)
    {
        Core::SafeSyncType<Core::CriticalSection> lock(_clientLock);
        _clients.push_back(client);
        return Core::ERROR_NONE;
    }

    uint32_t SharedStorageImplementation::Unregister(Exchange::IStore2::INotification* client)
    {
        Core::SafeSyncType<Core::CriticalSection> lock(_clientLock);
        _clients.remove(client);
        return Core::ERROR_NONE;
    }

    SharedStorageImplementation::~SharedStorageImplementation()
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

    uint32_t SharedStorageImplementation::Configure(PluginHost::IShell* service)
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
        else
        {
            LOGERR("sharedstorage service is null \n");
        }

        return result;
    }

    void SharedStorageImplementation::ValueChanged(const Exchange::IStore2::ScopeType scope, const string& ns, const string& key, const string& value)
    {
        Core::SafeSyncType<Core::CriticalSection> lock(_clientLock);
        for (auto* client : _clients)
        {
            client->ValueChanged(scope, ns, key, value);
        }
    }


    Exchange::IStore2* SharedStorageImplementation::getRemoteStoreObject(Exchange::IStore2::ScopeType eScope)
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

    uint32_t SharedStorageImplementation::SetValue(const IStore2::ScopeType scope, const string& ns, const string& key, const string& value, const uint32_t ttl)
    {
        Exchange::IStore2* store = getRemoteStoreObject(scope);
        if (store != nullptr)
        {
            return store->SetValue(scope, ns, key, value, ttl);
        }
        return Core::ERROR_NOT_SUPPORTED;
    }

    uint32_t SharedStorageImplementation::GetValue(const IStore2::ScopeType scope, const string& ns, const string& key, string& value, uint32_t& ttl)
    {
        Exchange::IStore2* store = getRemoteStoreObject(scope);
        if (store != nullptr)
        {
            return store->GetValue(scope, ns, key, value, ttl);
        }
        return Core::ERROR_NOT_SUPPORTED;
    }

    uint32_t SharedStorageImplementation::DeleteKey(const IStore2::ScopeType scope, const string& ns, const string& key)
    {
        Exchange::IStore2* store = getRemoteStoreObject(scope);
        if (store != nullptr)
        {
            return store->DeleteKey(scope, ns, key);
        }
        return Core::ERROR_NOT_SUPPORTED;
    }

    uint32_t SharedStorageImplementation::DeleteNamespace(const IStore2::ScopeType scope, const string& ns)
    {
        Exchange::IStore2* store = getRemoteStoreObject(scope);
        if (store != nullptr)
        {
            return store->DeleteNamespace(scope, ns);
        }
        return Core::ERROR_NOT_SUPPORTED;
    }

    uint32_t SharedStorageImplementation::FlushCache()
    {
        if (_psCache != nullptr)
        {
            return _psCache->FlushCache();
        }
        return Core::ERROR_NOT_SUPPORTED;
    }

    uint32_t SharedStorageImplementation::GetKeys(const IStore2::ScopeType scope, const string& ns, RPC::IStringIterator*& keys)
    {
        ASSERT(scope == IStore2::ScopeType::DEVICE);
        if (_psInspector != nullptr)
        {
            return _psInspector->GetKeys(scope, ns, keys);
        }
        return Core::ERROR_NOT_SUPPORTED;
    }

    uint32_t SharedStorageImplementation::GetNamespaces(const IStore2::ScopeType scope, RPC::IStringIterator*& namespaces)
    {
        ASSERT(scope == IStore2::ScopeType::DEVICE);
        if (_psInspector != nullptr)
        {
            return _psInspector->GetNamespaces(scope, namespaces);
        }
        return Core::ERROR_NOT_SUPPORTED;
    }

    uint32_t SharedStorageImplementation::GetStorageSizes(const IStore2::ScopeType scope, INamespaceSizeIterator*& storageList)
    {
        ASSERT(scope == IStore2::ScopeType::DEVICE);
        if (_psInspector != nullptr)
        {
            return _psInspector->GetStorageSizes(scope, storageList);
        }
        return Core::ERROR_NOT_SUPPORTED;
    }

    uint32_t SharedStorageImplementation::SetNamespaceStorageLimit(const IStore2::ScopeType scope, const string& ns, const uint32_t size)
    {
        ASSERT(scope == IStore2::ScopeType::DEVICE);
        if (_psLimit != nullptr)
        {
            return _psLimit->SetNamespaceStorageLimit(scope, ns, size);
        }
        return Core::ERROR_NOT_SUPPORTED;
    }

    uint32_t SharedStorageImplementation::GetNamespaceStorageLimit(const IStore2::ScopeType scope, const string& ns, uint32_t& size)
    {
        ASSERT(scope == IStore2::ScopeType::DEVICE);
        if (_psLimit != nullptr)
        {
            return _psLimit->GetNamespaceStorageLimit(scope, ns, size);
        }
        return Core::ERROR_NOT_SUPPORTED;
    }

} // namespace Plugin
} // namespace WPEFramework
