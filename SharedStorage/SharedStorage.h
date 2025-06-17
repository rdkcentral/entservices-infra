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
#include <interfaces/json/JsonData_PersistentStore.h>
#include <interfaces/IConfiguration.h>

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
        void RegisterAll();
        void UnregisterAll();

        uint32_t endpoint_setValue(const JsonData::PersistentStore::SetValueParamsData& params, JsonData::PersistentStore::DeleteKeyResultInfo& response);
        uint32_t endpoint_getValue(const JsonData::PersistentStore::DeleteKeyParamsInfo& params, JsonData::PersistentStore::GetValueResultData& response);
        uint32_t endpoint_deleteKey(const JsonData::PersistentStore::DeleteKeyParamsInfo& params, JsonData::PersistentStore::DeleteKeyResultInfo& response);
        uint32_t endpoint_deleteNamespace(const JsonData::PersistentStore::DeleteNamespaceParamsInfo& params, JsonData::PersistentStore::DeleteKeyResultInfo& response);
        uint32_t endpoint_getKeys(const JsonData::PersistentStore::DeleteNamespaceParamsInfo& params, JsonData::PersistentStore::GetKeysResultData& response);
        uint32_t endpoint_getNamespaces(const JsonData::PersistentStore::GetNamespacesParamsInfo& params, JsonData::PersistentStore::GetNamespacesResultData& response);
        uint32_t endpoint_getStorageSize(const JsonData::PersistentStore::GetNamespacesParamsInfo& params, JsonObject& response); // Deprecated
        uint32_t endpoint_getStorageSizes(const JsonData::PersistentStore::GetNamespacesParamsInfo& params, JsonData::PersistentStore::GetStorageSizesResultData& response);
        uint32_t endpoint_flushCache(JsonData::PersistentStore::DeleteKeyResultInfo& response);
        uint32_t endpoint_getNamespaceStorageLimit(const JsonData::PersistentStore::DeleteNamespaceParamsInfo& params, JsonData::PersistentStore::GetNamespaceStorageLimitResultData& response);
        uint32_t endpoint_setNamespaceStorageLimit(const JsonData::PersistentStore::SetNamespaceStorageLimitParamsData& params);

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
