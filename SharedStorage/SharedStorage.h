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

#pragma once

#include "Module.h"
#include "UtilsLogging.h"
#include <interfaces/IStore2.h>
#include <interfaces/IStoreCache.h>
#include <interfaces/json/JStore2.h>
#include <interfaces/json/JStoreLimit.h>
#include <interfaces/json/JStoreInspector.h>
#include <interfaces/json/JStoreCache.h>
#include <interfaces/json/JsonData_PersistentStore.h>
#include <interfaces/IConfiguration.h>
#include <com/com.h>
#include <core/core.h>

namespace WPEFramework {
namespace Plugin {

    class SharedStorage : public PluginHost::IPlugin, public PluginHost::JSONRPC
    {

    private:
        class Notification : public RPC::IRemoteConnection::INotification,
                             public Exchange::IStore2::INotification {
        private:
            Notification() = delete;
            Notification(const Notification&) = delete;
            Notification& operator=(const Notification&) = delete;

        public:
        explicit Notification(SharedStorage* parent)
            : _parent(*parent)
            {
                ASSERT(parent != nullptr);
                if(parent == nullptr)
                {
                    LOGERR("parent is null");
                }
            }

            virtual ~Notification()
            {
            }

            BEGIN_INTERFACE_MAP(Notification)
            INTERFACE_ENTRY(Exchange::IStore2::INotification)
            INTERFACE_ENTRY(RPC::IRemoteConnection::INotification)
            END_INTERFACE_MAP

            void Activated(RPC::IRemoteConnection*) override
            {

            }

            void Deactivated(RPC::IRemoteConnection* connection) override
            {
                _parent.Deactivated(connection);
            }

            void ValueChanged(const Exchange::IStore2::ScopeType scope, const string& ns, const string& key, const string& value)
            {
                LOGINFO("ValueChanged ns:%s key:%s value:%s", ns.c_str(), key.c_str(), value.c_str());
                Exchange::JStore2::Event::ValueChanged(_parent, scope, ns, key, value);
            }


        private:
            SharedStorage& _parent;
        };


        class Implementation : public Exchange::IStore2,
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
                explicit Store2Notification(Implementation& parent)
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
                INTERFACE_ENTRY(Exchange::IStore2::INotification)
                END_INTERFACE_MAP

            private:
                Implementation& _parent;
            };

        public:
            uint32_t Register(Exchange::IStore2::INotification* client) override;
            uint32_t Unregister(Exchange::IStore2::INotification* client) override;

        public:
            Implementation();
            ~Implementation() override;

            Implementation(const Implementation&) = delete;
            Implementation& operator=(const Implementation&) = delete;

            BEGIN_INTERFACE_MAP(Implementation)
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

    private:
        SharedStorage(const SharedStorage&) = delete;
        SharedStorage& operator=(const SharedStorage&) = delete;

    public:
        SharedStorage();
        ~SharedStorage() override;

        BEGIN_INTERFACE_MAP(SharedStorage)
        INTERFACE_ENTRY(PluginHost::IPlugin)
        INTERFACE_ENTRY(PluginHost::IDispatcher)
        INTERFACE_AGGREGATE(Exchange::IStore2, _sharedStorage)
        INTERFACE_AGGREGATE(Exchange::IStoreLimit, _sharedStorage)
        INTERFACE_AGGREGATE(Exchange::IStoreInspector, _sharedStorage)
        INTERFACE_AGGREGATE(Exchange::IStoreCache, _sharedStorage)
        END_INTERFACE_MAP

    public:
        const string Initialize(PluginHost::IShell* service) override;
        void Deinitialize(PluginHost::IShell* service) override;
        string Information() const override;

    private:
        void Deactivated(RPC::IRemoteConnection* connection);

    private:
        void event_onValueChanged(const JsonData::PersistentStore::SetValueParamsData& params)
        {
            Notify(_T("onValueChanged"), params);
        }
        Exchange::IStore2* getRemoteStoreObject(JsonData::PersistentStore::ScopeType eScope);

    private:
        PluginHost::IShell* _service{};
        uint32_t _connectionId;
        Exchange::IStore2* _sharedStorage;
        Exchange::IStoreCache* _sharedCache;
        Exchange::IStoreInspector* _sharedInspector;
        Exchange::IStoreLimit* _sharedLimit;
        Core::Sink<Notification> _sharedStorageNotification;
        Exchange::IConfiguration* configure;
    };

} // namespace Plugin
} // namespace WPEFramework
