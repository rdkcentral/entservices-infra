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

namespace WPEFramework {
namespace Plugin {

    class SharedStorage : public PluginHost::IPlugin, public PluginHost::JSONRPC {
    private:
        class Implementation : public Exchange::IStore2,
                               public Exchange::IStoreCache,
                               public Exchange::IStoreInspector,
                               public Exchange::IStoreLimit,
                               public Exchange::IConfiguration {

            class Store2Notification : public Exchange::IStore2::INotification
            {
            private:
                Store2Notification(const Store2Notification &) = delete;
                Store2Notification &operator=(const Store2Notification &) = delete;

            public:
                explicit Store2Notification(Implementation &parent)
                    : _parent(parent)
                {
                }
                ~Store2Notification() override = default;

            public:
                void ValueChanged(const Exchange::IStore2::ScopeType scope, const string &ns, const string &key, const string &value) override
                {
                    // TRACE(Trace::Information, (_T("ValueChanged event")));
                    JsonData::PersistentStore::SetValueParamsData params;
                    params.Scope = JsonData::PersistentStore::ScopeType(scope);
                    params.Namespace = ns;
                    params.Key = key;
                    params.Value = value;

                    _parent._shared.event_onValueChanged(params);
                }

                BEGIN_INTERFACE_MAP(Store2Notification)
                INTERFACE_ENTRY(Exchange::IStore2::INotification)
                END_INTERFACE_MAP

            private:
                Implementation &_parent;
            };

        public:
            Implementation(const Implementation &) = delete;
            Implementation &operator=(const Implementation &) = delete;

            explicit Implementation(SharedStorage& parent);
            ~Implementation() override;

            BEGIN_INTERFACE_MAP(Implementation)
            INTERFACE_ENTRY(Exchange::IStore2)
            INTERFACE_ENTRY(Exchange::IStoreCache)
            INTERFACE_ENTRY(Exchange::IStoreInspector)
            INTERFACE_ENTRY(Exchange::IStoreLimit)
            INTERFACE_ENTRY(Exchange::IConfiguration)
            END_INTERFACE_MAP

            uint32_t Configure(PluginHost::IShell *service) override;
            uint32_t SetValue(const IStore2::ScopeType scope, const string &ns, const string &key, const string &value, const uint32_t ttl) override;
            uint32_t GetValue(const IStore2::ScopeType scope, const string &ns, const string &key, string &value, uint32_t &ttl) override;
            uint32_t DeleteKey(const IStore2::ScopeType scope, const string &ns, const string &key) override;
            uint32_t DeleteNamespace(const IStore2::ScopeType scope, const string &ns) override;
            uint32_t FlushCache() override;
            uint32_t GetKeys(const IStore2::ScopeType scope, const string &ns, RPC::IStringIterator *&keys) override;
            uint32_t GetNamespaces(const IStore2::ScopeType scope, RPC::IStringIterator *&namespaces) override;
            uint32_t GetStorageSizes(const IStore2::ScopeType scope, INamespaceSizeIterator *&storageList) override;
            uint32_t SetNamespaceStorageLimit(const IStore2::ScopeType scope, const string &ns, const uint32_t size) override;
            uint32_t GetNamespaceStorageLimit(const IStore2::ScopeType scope, const string &ns, uint32_t &size) override;
            uint32_t Register(Exchange::IStore2::INotification* notification) override;
            uint32_t Unregister(Exchange::IStore2::INotification* notification) override;

        private:
            Exchange::IStore2 *getRemoteStoreObject(Exchange::IStore2::ScopeType scope);

        private:
            // Exchange::IStore2* _store;
            SharedStorage &_shared;
            Core::Sink<Store2Notification> _storeNotification;
            Exchange::IStore2 *_psObject;
            Exchange::IStoreCache *_psCache;
            Exchange::IStoreInspector *_psInspector;
            Exchange::IStoreLimit *_psLimit;
            Exchange::IStore2 *_csObject;
            PluginHost::IPlugin *m_PersistentStoreRef;
            PluginHost::IPlugin *m_CloudStoreRef;
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
        //INTERFACE_AGGREGATE(Exchange::IStore2, _implementation)
        END_INTERFACE_MAP

    public:
        const string Initialize(PluginHost::IShell* service) override;
        void Deinitialize(PluginHost::IShell* service) override;
        string Information() const override;

    private:
        void event_onValueChanged(const JsonData::PersistentStore::SetValueParamsData& params)
        {
            Notify(_T("onValueChanged"), params);
        }

    private:
        PluginHost::IShell* _service{};
        Implementation* _impl;
        Exchange::IStore2* _implementation;
        //Exchange::IStore2* _storeImpl;
        Exchange::IStoreCache* _cacheImpl;
        Exchange::IStoreLimit* _limitImpl;
        Exchange::IStoreInspector* _inspectorImpl;
    };

} // namespace Plugin
} // namespace WPEFramework
