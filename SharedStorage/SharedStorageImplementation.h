
#pragma once

#include "Module.h"
#include <interfaces/IStore2.h>
#include <interfaces/IStoreCache.h>
#include <interfaces/IConfiguration.h>
#include <interfaces/json/JsonData_PersistentStore.h>
#include "UtilsLogging.h"
#include <com/com.h>
#include <core/core.h>

namespace WPEFramework {
namespace Plugin {

    class SharedStorageImplementation : public Exchange::IStore2,
                                        public Exchange::IStoreCache,
                                        public Exchange::IStoreInspector,
                                        public Exchange::IStoreLimit,
                                        public Exchange::IConfiguration {

    private:
        class Store2Notification : public Exchange::IStore2::INotification {
        private:
            Store2Notification(const Store2Notification&) = delete;
            Store2Notification& operator=(const Store2Notification&) = delete;

        public:
            explicit Store2Notification(SharedStorageImplementation& parent)
                : _parent(parent)
            { 
            }
            ~Store2Notification() override = default;

        public:
            void ValueChanged(const Exchange::IStore2::ScopeType scope, const string& ns, const string& key, const string& value) override
            {
                _parent.ValueChanged(scope, ns, key, value);
            }

            BEGIN_INTERFACE_MAP(Store2Notification)
            INTERFACE_ENTRY(IStore2::INotification)
            END_INTERFACE_MAP

        private:
            SharedStorageImplementation& _parent;
        };

    public:
        uint32_t Register(Exchange::IStore2::INotification* client) override;
        uint32_t Unregister(Exchange::IStore2::INotification* client) override;

    public:
        SharedStorageImplementation();
        ~SharedStorageImplementation() override;

        SharedStorageImplementation(const SharedStorageImplementation&) = delete;
        SharedStorageImplementation& operator=(const SharedStorageImplementation&) = delete;

        BEGIN_INTERFACE_MAP(SharedStorageImplementation)
        INTERFACE_ENTRY(Exchange::IStore2)
        INTERFACE_ENTRY(Exchange::IStoreCache)
        INTERFACE_ENTRY(Exchange::IStoreInspector)
        INTERFACE_ENTRY(Exchange::IStoreLimit)
        INTERFACE_ENTRY(Exchange::IConfiguration)
        END_INTERFACE_MAP

    public:
        Exchange::IStore2* getRemoteStoreObject(Exchange::IStore2::ScopeType eScope);
        void ValueChanged(const Exchange::IStore2::ScopeType scope, const string& ns, const string& key, const string& value);
        uint32_t SetValue(const IStore2::ScopeType scope, const string& ns, const string& key, const string& value, const uint32_t ttl) override;
        uint32_t GetValue(const IStore2::ScopeType scope, const string& ns, const string& key, string& value, uint32_t& ttl) override;
        uint32_t DeleteKey(const IStore2::ScopeType scope, const string& ns, const string& key) override;
        uint32_t DeleteNamespace(const IStore2::ScopeType scope, const string& ns) override;
        uint32_t FlushCache() override;
        uint32_t GetKeys(const IStore2::ScopeType scope, const string& ns, RPC::IStringIterator*& keys) override;
        uint32_t GetNamespaces(const IStore2::ScopeType scope, RPC::IStringIterator*& namespaces) override;
        uint32_t GetStorageSizes(const IStore2::ScopeType scope, INamespaceSizeIterator*& storageList) override;
        uint32_t SetNamespaceStorageLimit(const IStore2::ScopeType scope, const string& ns, const uint32_t size) override;
        uint32_t GetNamespaceStorageLimit(const IStore2::ScopeType scope, const string& ns, uint32_t& size) override;

        //IConfiguration methods
        uint32_t Configure(PluginHost::IShell* service) override;
    
    private:
        PluginHost::IPlugin *m_PersistentStoreRef;
        PluginHost::IPlugin *m_CloudStoreRef;
        Exchange::IStoreCache* _psCache;
        Exchange::IStoreInspector* _psInspector;
        Exchange::IStoreLimit* _psLimit;
        Core::Sink<Store2Notification> _storeNotification;
        Exchange::IStore2* _psObject;
        Exchange::IStore2* _csObject;
        std::list<Exchange::IStore2::INotification*> _clients;
        Core::CriticalSection _clientLock;
        PluginHost::IShell* _service;
    };

} // namespace Plugin
} // namespace WPEFramework
