
#pragma once

#include "Module.h"
#include <interfaces/IStore2.h>
#include <interfaces/IStoreCache.h>
#include <com/com.h>
#include <core/core.h>
#include <plugins/plugins.h>

namespace WPEFramework {
namespace Plugin {

    class SharedStorageImplementation : public Exchange::IStore2,
                                        public Exchange::IStoreCache,
                                        public Exchange::IStoreInspector,
                                        public Exchange::IStoreLimit {
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

    private:
        SharedStorageImplementation(const SharedStorageImplementation&) = delete;
        SharedStorageImplementation& operator=(const SharedStorageImplementation&) = delete;

    public:
        SharedStorageImplementation();
        ~SharedStorageImplementation() override;

        BEGIN_INTERFACE_MAP(SharedStorageImplementation)
        INTERFACE_ENTRY(IStore2)
        INTERFACE_ENTRY(IStoreCache)
        INTERFACE_ENTRY(IStoreInspector)
        INTERFACE_ENTRY(IStoreLimit)
        END_INTERFACE_MAP

    public:
        class EXTERNAL Job : public Core::IDispatch {
        protected:
             Job(SharedStorageImplementation *sharedStorageImplementation, Event event, JsonValue &params)
                : _sharedStorageImplementation(sharedStorageImplementation)
                , _event(event)
                , _params(params) {
                if (_sharedStorageImplementation != nullptr) {
                    _sharedStorageImplementation->AddRef();
                }
            }

       public:
            Job() = delete;
            Job(const Job&) = delete;
            Job& operator=(const Job&) = delete;
            ~Job() {
                if (_sharedStorageImplementation != nullptr) {
                    _sharedStorageImplementation->Release();
                }
            }

       public:
            static Core::ProxyType<Core::IDispatch> Create(SharedStorageImplementation *sharedStorageImplementation, Event event, JsonValue params) {
#ifndef USE_THUNDER_R4
                return (Core::proxy_cast<Core::IDispatch>(Core::ProxyType<Job>::Create(sharedStorageImplementation, event, params)));
#else
                return (Core::ProxyType<Core::IDispatch>(Core::ProxyType<Job>::Create(sharedStorageImplementation, event, params)));
#endif
            }

            virtual void Dispatch() {
                _sharedStorageImplementation->Dispatch(_event, _params);
            }
        private:
            SharedStorageImplementation *_sharedStorageImplementation;
            const Event _event;
            const JsonValue _params;
        };

    public:
        Exchange::IStore2* getRemoteStoreObject(Exchange::IStore2::ScopeType eScope);
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
    
    private:
        IStore2* _deviceStore2;
        IStoreCache* _deviceStoreCache;
        IStoreInspector* _deviceStoreInspector;
        IStoreLimit* _deviceStoreLimit;
        Core::Sink<Store2Notification> _store2Sink;
    };

} // namespace Plugin
} // namespace WPEFramework
