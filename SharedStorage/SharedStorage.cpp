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
        , _impl(nullptr)
        , _implementation(nullptr)
        , _cacheImpl(nullptr)
        , _limitImpl(nullptr)
        , _inspectorImpl(nullptr)
    {

    }
    SharedStorage::~SharedStorage()
    {

    }

    Exchange::IStore2* SharedStorage::Implementation::getRemoteStoreObject(Exchange::IStore2::ScopeType scope)
    {
        LOGINFO("SharedStorage getremotestoreobject called");
        if( (scope == Exchange::IStore2::ScopeType::DEVICE) && _psObject)
        {
            LOGINFO("SharedStorage getremotestoreobject store = device");
            return _psObject;
        }
        else if( (scope == Exchange::IStore2::ScopeType::ACCOUNT) && _csObject)
        {
            LOGINFO("SharedStorage getremotestoreobject store = account");
            return _csObject;
        }
        else
        {
            LOGINFO("SharedStorage getremotestoreobject store = nullptr");
            //TRACE(Trace::Error, (_T("%s: Unknown scope: %d"), __FUNCTION__, static_cast<int>(eScope)));
            return nullptr;
        }
    }

    const string SharedStorage::Initialize(PluginHost::IShell* service)
    {
        SYSLOG(Logging::Startup, (_T("SharedStorage::Initialize: PID=%u"), getpid()));
        string message;

        ASSERT(service != nullptr);
        ASSERT(nullptr == _service);

        _service = service;
        _service->AddRef();

        // m_PersistentStoreRef = service->QueryInterfaceByCallsign<PluginHost::IPlugin>("org.rdk.PersistentStore");
        // if(nullptr != m_PersistentStoreRef)
        // {
        //     // Get interface for IStore2
        //     _psObject = m_PersistentStoreRef->QueryInterface<Exchange::IStore2>();
        //     // Get interface for IStoreInspector
        //     _psInspector = m_PersistentStoreRef->QueryInterface<Exchange::IStoreInspector>();
        //     // Get interface for IStoreLimit
        //     _psLimit = m_PersistentStoreRef->QueryInterface<Exchange::IStoreLimit>();
        //     // Get interface for IStoreCache
        //     _psCache = m_PersistentStoreRef->QueryInterface<Exchange::IStoreCache>();
        //     if ( (nullptr == _psObject) || (nullptr == _psInspector) || (nullptr == _psLimit) || (nullptr == _psCache) )
        //     {
        //         message = _T("SharedStorage plugin could not be initialized.");
        //         TRACE(Trace::Error, (_T("%s: Can't get PersistentStore interface"), __FUNCTION__));
        //         m_PersistentStoreRef->Release();
        //         m_PersistentStoreRef = nullptr;
        //     }
        //     else
        //     {
        //         _psObject->Register(&_storeNotification);
        //     }
        // }
        // else
        // {
        //     message = _T("SharedStorage plugin could not be initialized.");
        //     TRACE(Trace::Error, (_T("%s: Can't get PersistentStore interface"), __FUNCTION__));
        // }

        // // Establish communication with CloudStore
        // m_CloudStoreRef = service->QueryInterfaceByCallsign<PluginHost::IPlugin>("org.rdk.CloudStore");
        // if(nullptr != m_CloudStoreRef)
        // {
        //     // Get interface for IStore2
        //     _csObject = m_CloudStoreRef->QueryInterface<Exchange::IStore2>();
        //     if (nullptr == _csObject)
        //     {
        //         //message = _T("SharedStorage plugin could not be initialized.");
        //         TRACE(Trace::Error, (_T("%s: Can't get CloudStore interface"), __FUNCTION__));
        //         m_CloudStoreRef->Release();
        //         m_CloudStoreRef = nullptr;
        //     }
        //     else
        //     {
        //         _csObject->Register(&_storeNotification);
        //     }
        // }
        // else
        // {
        //     //message = _T("SharedStorage plugin could not be initialized.");
        //     TRACE(Trace::Error, (_T("%s: Can't get CloudStore interface"), __FUNCTION__));
        // }

        _impl = Core::Service<Implementation>::Create<Implementation>(*this);

        //_implementation = Core::Service<Implementation>::Create<Exchange::IStore2>(*this);
        _implementation = (_impl->QueryInterface<Exchange::IStore2>());
        Exchange::JStore2::Register(*this, _implementation);

        //_cacheImpl = Core::Service<Implementation>::Create<Exchange::IStoreCache>(*this);
        _cacheImpl = _impl->QueryInterface<Exchange::IStoreCache>();
        Exchange::JStoreCache::Register(*this, _cacheImpl);

        //_limitImpl = Core::Service<Implementation>::Create<Exchange::IStoreLimit>(*this);
        _limitImpl = _impl->QueryInterface<Exchange::IStoreLimit>();
        Exchange::JStoreLimit::Register(*this, _limitImpl);

        //_inspectorImpl = Core::Service<Implementation>::Create<Exchange::IStoreInspector>(*this);
        _inspectorImpl = _impl->QueryInterface<Exchange::IStoreInspector>();
        Exchange::JStoreInspector::Register(*this, _inspectorImpl);

        // _storeImpl = _implementation->QueryInterface<Exchange::IStore2>();
        // if(_storeImpl)
        // {
        //     Exchange::JStore2::Register(*this, _storeImpl);
        // }

        // _cacheImpl = _implementation->QueryInterface<Exchange::IStoreCache>();
        // if(_cacheImpl)
        // {
        //     Exchange::JStoreCache::Register(*this, _cacheImpl);
        // }

        // _limitImpl = _implementation->QueryInterface<Exchange::IStoreLimit>();
        // if(_limitImpl)
        // {
        //     Exchange::JStoreLimit::Register(*this, _limitImpl);
        // }

        // _inspectorImpl = _implementation->QueryInterface<Exchange::IStoreInspector>();
        // if(_inspectorImpl)
        // {
        //     Exchange::JStoreInspector::Register(*this, _inspectorImpl);
        // }

        // Exchange::JStore2::Register(*this, _implementation);
        // Exchange::JStoreCache::Register(*this, _implementation);
        // Exchange::JStoreInspector::Register(*this, _implementation);
        // Exchange::JStoreLimit::Register(*this, _implementation);

        // Exchange::IStore2* store = _implementation->QueryInterface<Exchange::IStore2>();
        // if(store != nullptr)
        // {
        //     Exchange::JStore2::Register(*this, store);
        //     store->Release();
        // }
        // else
        // {
        //     LOGINFO("SharedStorage store is null");
        // }


        // Exchange::IStoreCache* cache = _implementation->QueryInterface<Exchange::IStoreCache>();
        // if(cache != nullptr)
        // {
        //     Exchange::JStoreCache::Register(*this, cache);
        //     cache->Release();
        // }
        // else
        // {
        //     LOGINFO("SharedStorage cache is null");
        // }

        // Exchange::IStoreInspector* inspector = _implementation->QueryInterface<Exchange::IStoreInspector>();
        // if(inspector != nullptr)
        // {
        //     Exchange::JStoreInspector::Register(*this, inspector);
        //     inspector->Release();
        // }
        // else
        // {
        //     LOGINFO("SharedStorage inspector is null");
        // }


        // Exchange::IStoreLimit* limit = _implementation->QueryInterface<Exchange::IStoreLimit>();
        // if(limit != nullptr)
        // {
        //     Exchange::JStoreLimit::Register(*this, limit);
        //     limit->Release();
        // }
        // else
        // {
        //     LOGINFO("SharedStorage limit is null");
        // }


        Exchange::IConfiguration* config = static_cast<Exchange::IConfiguration*>(_impl);
        if(config != nullptr)
        {
            LOGINFO("SharedStorage config called");
            uint32_t result = config->Configure(service);
            config->Release();

            if(result != Core::ERROR_NONE)
            {
                return _T("SharedStorage Configure failed");
            }
        }
        else
        {
            LOGINFO("SharedStorage config is null");
        }

        SYSLOG(Logging::Startup, (string(_T("SharedStorage Initialize complete"))));
        return message;
    }

    void SharedStorage::Deinitialize(PluginHost::IShell* service)
    {
        ASSERT(_service == service);
        SYSLOG(Logging::Shutdown, (string(_T("SharedStorage::Deinitialize"))));

        Exchange::JStore2::Unregister(*this);
        Exchange::JStoreCache::Unregister(*this);
        Exchange::JStoreInspector::Unregister(*this);
        Exchange::JStoreLimit::Unregister(*this);

        if(_implementation != nullptr)
        {
            _implementation->Release();
            _implementation = nullptr;
        }

        if(_service != nullptr)
        {
            _service->Release();
            _service = nullptr;
        }
        


        SYSLOG(Logging::Shutdown, (string(_T("SharedStorage Deinitialize complete"))));
    }

    string SharedStorage::Information() const
    {
        return (string());
    }

    SharedStorage::Implementation::Implementation(SharedStorage& parent)
        : _shared(parent)
        , _storeNotification(*this)
        , _psObject(nullptr)
        , _psCache(nullptr)
        , _psInspector(nullptr)
        , _psLimit(nullptr)
        , _csObject(nullptr)
        , m_PersistentStoreRef(nullptr)
        , m_CloudStoreRef(nullptr)
    {

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
        LOGINFO("SharedStorage configure called");
        uint32_t result = Core::ERROR_GENERAL;
        string message = "";

        if (service != nullptr)
        {
            LOGINFO("SharedStorage configure service is not null");
            //_service = service;
            //_service->AddRef();
            result = Core::ERROR_NONE;

            m_PersistentStoreRef = service->QueryInterfaceByCallsign<PluginHost::IPlugin>("org.rdk.PersistentStore");
            if (nullptr != m_PersistentStoreRef)
            {
                LOGINFO("SharedStorage configure m_PersistentStoreRef is not null");
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
                    LOGINFO("SharedStorage configure a ps object is null");
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
                LOGINFO("SharedStorage configure m_PersistentStoreRef is null");
                message = _T("SharedStorage plugin could not be initialized.");
                TRACE(Trace::Error, (_T("%s: Can't get PersistentStore interface"), __FUNCTION__));
            }

            // Establish communication with CloudStore
            m_CloudStoreRef = service->QueryInterfaceByCallsign<PluginHost::IPlugin>("org.rdk.CloudStore");
            if (nullptr != m_CloudStoreRef)
            {
                LOGINFO("SharedStorage configure m_CloudStoreRef is not null");
                // Get interface for IStore2
                _csObject = m_CloudStoreRef->QueryInterface<Exchange::IStore2>();
                if (nullptr == _csObject)
                {
                    LOGINFO("SharedStorage configure cs object is null");
                    // message = _T("SharedStorage plugin could not be initialized.");
                    TRACE(Trace::Error, (_T("%s: Can't get CloudStore interface"), __FUNCTION__));
                    m_CloudStoreRef->Release();
                    m_CloudStoreRef = nullptr;
                }
                else
                {
                    LOGINFO("SharedStorage configure cs object is not null");
                    _csObject->Register(&_storeNotification);
                }
            }
            else
            {
                LOGINFO("SharedStorage configure m_CloudStoreRef is null");
                // message = _T("SharedStorage plugin could not be initialized.");
                TRACE(Trace::Error, (_T("%s: Can't get CloudStore interface"), __FUNCTION__));
            }
        }

        return result;
    }


    uint32_t SharedStorage::Implementation::Register(Exchange::IStore2::INotification* notification)
    {
        uint32_t result = Core::ERROR_UNAVAILABLE;

        if(_psObject != nullptr)
        {
            result = _psObject->Register(notification);
        }

        if(_csObject != nullptr)
        {
            result = _csObject->Register(notification);
        }

        return result;
    }

    uint32_t SharedStorage::Implementation::Unregister(Exchange::IStore2::INotification* notification)
    {
        uint32_t result = Core::ERROR_UNAVAILABLE;

        if(_psObject != nullptr)
        {
            result = _psObject->Unregister(notification);
        }

        if(_csObject != nullptr)
        {
            result = _csObject->Unregister(notification);
        }

        return result;
    }


    uint32_t SharedStorage::Implementation::SetValue(const IStore2::ScopeType scope, const string& ns, const string& key, const string& value, const uint32_t ttl)
    {
        LOGINFO("SharedStorage SetValue called");
        uint32_t status = Core::ERROR_NOT_SUPPORTED;
        Exchange::IStore2* store = nullptr;

        store = getRemoteStoreObject(scope);
        if (store != nullptr)
        {
            LOGINFO("SharedStorage SetValue store set");
            status = store->SetValue(scope, ns, key, value, ttl);
        }
        else{
            LOGINFO("SharedStorage SetValue store is null");
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
        LOGINFO("SharedStorage GetKeys called");
        uint32_t status = Core::ERROR_NOT_SUPPORTED;

        if(scope != IStore2::ScopeType::DEVICE)
        {
            LOGINFO("SharedStorage getkeys scope is not device");
            return status;
        }

        if (_psInspector != nullptr)
        {
            LOGINFO("SharedStorage getkeys psinspector is not null");
            status = _psInspector->GetKeys(scope, ns, keys);
        }
        else
        {
            LOGINFO("SharedStorage getKeys psinspector is null");
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
