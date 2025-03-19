#include "SharedStorageImplementation.h"

namespace WPEFramework {
namespace Plugin {

    SharedStorageImplementation::SharedStorageImplementation()
        : _deviceStore2(nullptr),
          _deviceStoreCache(nullptr),
          _deviceStoreInspector(nullptr),
          _deviceStoreLimit(nullptr),
          _store2Sink(*this) {
    }

    SharedStorageImplementation::~SharedStorageImplementation() {
    }

    void UserSettingsImplementation::ValueChanged(const Exchange::IStore2::ScopeType scope, const string& ns, const string& key, const string& value)
    {
        Exchange::IStore2* store = getRemoteStoreObject(scope);

        if(store)
        {
            store->ValueChanged(scope, ns, key, value);
        }
        else
        {
            LOGERR("Not supported");
        }


    Exchange::IStore2* SharedStorage::getRemoteStoreObject(ScopeType eScope)
    {
        if( (eScope == ScopeType::DEVICE) && _psObject)
        {
            return _psObject;
        }
        else if( (eScope == ScopeType::ACCOUNT) && _csObject)
        {
            return _csObject;
        }
        else
        {
            TRACE(Trace::Error, (_T("%s: Unknown scope: %d"), __FUNCTION__, static_cast<int>(eScope)));
            return nullptr;
        }
    }

    uint32_t SharedStorageImplementation::SetValue(const IStore2::ScopeType scope, const string& ns, const string& key, const string& value, const uint32_t ttl) {
        IStore2* store = SharedStorage::getRemoteStoreObject(scope);
        if (store != nullptr) {
            return store->SetValue(scope, ns, key, value, ttl);
        }
        return Core::ERROR_UNAVAILABLE;
    }

    uint32_t SharedStorageImplementation::GetValue(const IStore2::ScopeType scope, const string& ns, const string& key, string& value, uint32_t& ttl) {
        IStore2* store = SharedStorage::getRemoteStoreObject(scope);
        if (store != nullptr) {
            return store->GetValue(scope, ns, key, value, ttl);
        }
        return Core::ERROR_UNAVAILABLE;
    }

    uint32_t SharedStorageImplementation::DeleteKey(const IStore2::ScopeType scope, const string& ns, const string& key) {
        IStore2* store = SharedStorage::getRemoteStoreObject(scope);
        if (store != nullptr) {
            return store->DeleteKey(scope, ns, key);
        }
        return Core::ERROR_UNAVAILABLE;
    }

    uint32_t SharedStorageImplementation::DeleteNamespace(const IStore2::ScopeType scope, const string& ns) {
        IStore2* store = SharedStorage::getRemoteStoreObject(scope);
        if (store != nullptr) {
            return store->DeleteNamespace(scope, ns);
        }
        return Core::ERROR_UNAVAILABLE;
    }

    uint32_t SharedStorageImplementation::FlushCache() {
        if (_deviceStoreCache != nullptr) {
            return _deviceStoreCache->FlushCache();
        }
        return Core::ERROR_UNAVAILABLE;
    }

    uint32_t SharedStorageImplementation::GetKeys(const IStore2::ScopeType scope, const string& ns, RPC::IStringIterator*& keys) {
        IStore2* store = SharedStorage::getRemoteStoreObject(scope);
        if (store != nullptr) {
            return store->GetKeys(scope, ns, keys);
        }
        return Core::ERROR_UNAVAILABLE;
    }

    uint32_t SharedStorageImplementation::GetNamespaces(const IStore2::ScopeType scope, RPC::IStringIterator*& namespaces) {
        IStore2* store = SharedStorage::getRemoteStoreObject(scope);
        if (store != nullptr) {
            return store->GetNamespaces(scope, namespaces);
        }
        return Core::ERROR_UNAVAILABLE;
    }

    uint32_t SharedStorageImplementation::GetStorageSizes(const IStore2::ScopeType scope, INamespaceSizeIterator*& storageList) {
        if (scope == IStore2::ScopeType::DEVICE) {
            return SharedStorage::getRemoteStoreObject(scope)->GetStorageSizes(scope, storageList);
        }
        return Core::ERROR_UNAVAILABLE;
    }

    uint32_t SharedStorageImplementation::SetNamespaceStorageLimit(const IStore2::ScopeType scope, const string& ns, const uint32_t size) {
        if (scope == IStore2::ScopeType::DEVICE) {
            return SharedStorage::getRemoteStoreObject(scope)->SetNamespaceStorageLimit(scope, ns, size);
        }
        return Core::ERROR_UNAVAILABLE;
    }

    uint32_t SharedStorageImplementation::GetNamespaceStorageLimit(const IStore2::ScopeType scope, const string& ns, uint32_t& size) {
        if (scope == IStore2::ScopeType::DEVICE) {
            return SharedStorage::getRemoteStoreObject(scope)->GetNamespaceStorageLimit(scope, ns, size);
        }
        return Core::ERROR_UNAVAILABLE;
    }

} // namespace Plugin
} // namespace WPEFramework
