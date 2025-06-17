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
        uint32_t status = Core::ERROR_NOT_SUPPORTED;
        Exchange::IStore2* store = nullptr;

        store = getRemoteStoreObject(scope);
        if (store != nullptr)
        {
            status = store->SetValue(scope, ns, key, value, ttl);
        }

        return status;
    }

    uint32_t SharedStorageImplementation::GetValue(const IStore2::ScopeType scope, const string& ns, const string& key, string& value, uint32_t& ttl)
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

    uint32_t SharedStorageImplementation::DeleteKey(const IStore2::ScopeType scope, const string& ns, const string& key)
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

    uint32_t SharedStorageImplementation::DeleteNamespace(const IStore2::ScopeType scope, const string& ns)
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

    uint32_t SharedStorageImplementation::FlushCache()
    {
        uint32_t status = Core::ERROR_NOT_SUPPORTED;

        if (_psCache != nullptr)
        {
            status = _psCache->FlushCache();
        }

        return status;
    }

    uint32_t SharedStorageImplementation::GetKeys(const IStore2::ScopeType scope, const string& ns, RPC::IStringIterator*& keys)
    {
        uint32_t status = Core::ERROR_NOT_SUPPORTED;

        if(scope != IStore2::ScopeType::DEVICE)
        {
            return status;
        }

        if (_psInspector != nullptr)
        {
            status = _psInspector->GetKeys(scope, ns, keys);
        }

        return status;
    }

    uint32_t SharedStorageImplementation::GetNamespaces(const IStore2::ScopeType scope, RPC::IStringIterator*& namespaces)
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

    uint32_t SharedStorageImplementation::GetStorageSizes(const IStore2::ScopeType scope, INamespaceSizeIterator*& storageList)
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

    uint32_t SharedStorageImplementation::SetNamespaceStorageLimit(const IStore2::ScopeType scope, const string& ns, const uint32_t size)
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

    uint32_t SharedStorageImplementation::GetNamespaceStorageLimit(const IStore2::ScopeType scope, const string& ns, uint32_t& size)
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
