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
#include "SharedStorageImplementation.h"

namespace WPEFramework {
namespace Plugin {

    using namespace JsonData::PersistentStore;

    void SharedStorage::RegisterAll()
    {
        Register<SetValueParamsData, DeleteKeyResultInfo>(_T("setValue"), &SharedStorage::endpoint_setValue, this);
        Register<DeleteKeyParamsInfo, GetValueResultData>(_T("getValue"), &SharedStorage::endpoint_getValue, this);
        Register<DeleteKeyParamsInfo, DeleteKeyResultInfo>(_T("deleteKey"), &SharedStorage::endpoint_deleteKey, this);
        Register<DeleteNamespaceParamsInfo, DeleteKeyResultInfo>(_T("deleteNamespace"), &SharedStorage::endpoint_deleteNamespace, this);
        Register<DeleteNamespaceParamsInfo, GetKeysResultData>(_T("getKeys"), &SharedStorage::endpoint_getKeys, this);
        Register<GetNamespacesParamsInfo, GetNamespacesResultData>(_T("getNamespaces"), &SharedStorage::endpoint_getNamespaces, this);
        Register<GetNamespacesParamsInfo, JsonObject>(_T("getStorageSize"), &SharedStorage::endpoint_getStorageSize, this); // Deprecated
        Register<GetNamespacesParamsInfo, GetStorageSizesResultData>(_T("getStorageSizes"), &SharedStorage::endpoint_getStorageSizes, this);
        Register<void, DeleteKeyResultInfo>(_T("flushCache"), &SharedStorage::endpoint_flushCache, this);
        Register<DeleteNamespaceParamsInfo, GetNamespaceStorageLimitResultData>(_T("getNamespaceStorageLimit"), &SharedStorage::endpoint_getNamespaceStorageLimit, this);
        Register<SetNamespaceStorageLimitParamsData, void>(_T("setNamespaceStorageLimit"), &SharedStorage::endpoint_setNamespaceStorageLimit, this);
    }

    void SharedStorage::UnregisterAll()
    {
        Unregister(_T("setValue"));
        Unregister(_T("getValue"));
        Unregister(_T("deleteKey"));
        Unregister(_T("deleteNamespace"));
        Unregister(_T("getKeys"));
        Unregister(_T("getNamespaces"));
        Unregister(_T("getStorageSize"));
        Unregister(_T("getStorageSizes"));
        Unregister(_T("flushCache"));
        Unregister(_T("getNamespaceStorageLimit"));
        Unregister(_T("setNamespaceStorageLimit"));
    }

    uint32_t SharedStorage::endpoint_setValue(const SetValueParamsData& params, DeleteKeyResultInfo& response)
    {
        uint32_t status = Core::ERROR_NOT_SUPPORTED;

        if (nullptr != _sharedStorage)
        {
            status = _sharedStorage->SetValue(
                        Exchange::IStore2::ScopeType(params.Scope.Value()),
                        params.Namespace.Value(),
                        params.Key.Value(),
                        params.Value.Value(),
                        params.Ttl.Value() );
            if (status == Core::ERROR_NONE) {
                response.Success = true;
            }
        }
        return status;
    }

    uint32_t SharedStorage::endpoint_getValue(const DeleteKeyParamsInfo& params, GetValueResultData& response)
    {
        uint32_t status = Core::ERROR_NOT_SUPPORTED;
        string value;
        uint32_t ttl = 0;

        if (nullptr != _sharedStorage)
        {
            status = _sharedStorage->GetValue(
                        Exchange::IStore2::ScopeType(params.Scope.Value()),
                        params.Namespace.Value(),
                        params.Key.Value(),
                        value,
                        ttl );
            if (status == Core::ERROR_NONE) {
                response.Value = value;
                if (ttl > 0) {
                    response.Ttl = ttl;
                }
                response.Success = true;
            }
            else {
                TRACE(Trace::Error, (_T("%s: Status: %d"), __FUNCTION__, status));
            }
        }
        return status;
    }

    uint32_t SharedStorage::endpoint_deleteKey(const DeleteKeyParamsInfo& params, DeleteKeyResultInfo& response)
    {
        uint32_t status = Core::ERROR_NOT_SUPPORTED;
        
        if (nullptr != _sharedStorage)
        {
            status = _sharedStorage->DeleteKey(
                        Exchange::IStore2::ScopeType(params.Scope.Value()),
                        params.Namespace.Value(),
                        params.Key.Value() );
            if (status == Core::ERROR_NONE) {
                response.Success = true;
            }
            else {
                TRACE(Trace::Error, (_T("%s: Status: %d"), __FUNCTION__, status));
            }
        }
        return status;
    }

    uint32_t SharedStorage::endpoint_deleteNamespace(const DeleteNamespaceParamsInfo& params, DeleteKeyResultInfo& response)
    {
        uint32_t status = Core::ERROR_NOT_SUPPORTED;
        
        if (nullptr != _sharedStorage)
        {
            status = _sharedStorage->DeleteNamespace(
                        Exchange::IStore2::ScopeType(params.Scope.Value()),
                        params.Namespace.Value() );
            if (status == Core::ERROR_NONE) {
                response.Success = true;
            }
            else {
                TRACE(Trace::Error, (_T("%s: Status: %d"), __FUNCTION__, status));
            }
        }
        return status;
    }

    uint32_t SharedStorage::endpoint_getKeys(const DeleteNamespaceParamsInfo& params, GetKeysResultData& response)
    {
        uint32_t status = Core::ERROR_NOT_SUPPORTED;

        if(params.Scope.Value() != ScopeType::DEVICE)
        {
            return status;
        }

        if (nullptr != _sharedInspector)
        {
            RPC::IStringIterator* it;
            status = _sharedInspector->GetKeys(
                        Exchange::IStoreInspector::ScopeType(params.Scope.Value()),
                        params.Namespace.Value(),
                        it);
            if (status == Core::ERROR_NONE) {
                string element;
                while (it->Next(element) == true) {
                    response.Keys.Add() = element;
                }
                it->Release();
                response.Success = true;
            }
            else {
                TRACE(Trace::Error, (_T("%s: Status: %d"), __FUNCTION__, status));
            }
        }
        return status;
    }

    uint32_t SharedStorage::endpoint_getNamespaces(const GetNamespacesParamsInfo& params, GetNamespacesResultData& response)
    {
        uint32_t status = Core::ERROR_NOT_SUPPORTED;

        if(params.Scope.Value() != ScopeType::DEVICE)
        {
            return status;
        }

        if (nullptr != _sharedInspector)
        {
            RPC::IStringIterator* it;
            status = _sharedInspector->GetNamespaces(
                        Exchange::IStoreInspector::ScopeType(params.Scope.Value()),
                        it);
            if (status == Core::ERROR_NONE) {
                string element;
                while (it->Next(element) == true) {
                    response.Namespaces.Add() = element;
                }
                it->Release();
                response.Success = true;
            }
            else {
                TRACE(Trace::Error, (_T("%s: Status: %d"), __FUNCTION__, status));
            }
        }

        return status;
    }

    // Deprecated
    uint32_t SharedStorage::endpoint_getStorageSize(const GetNamespacesParamsInfo& params, JsonObject& response)
    {
        uint32_t status = Core::ERROR_NOT_SUPPORTED;
        
        if(params.Scope.Value() != ScopeType::DEVICE)
        {
            return status;
        }

        if (nullptr != _sharedInspector)
        {
            Exchange::IStoreInspector::INamespaceSizeIterator* it;
            status = _sharedInspector->GetStorageSizes(
                        Exchange::IStoreInspector::ScopeType(params.Scope.Value()),
                        it);
            if (status == Core::ERROR_NONE) {
                JsonObject jsonObject;
                Exchange::IStoreInspector::NamespaceSize element;
                while (it->Next(element) == true) {
                    jsonObject[element.ns.c_str()] = element.size;
                }
                it->Release();
                response["namespaceSizes"] = jsonObject;
                response["success"] = true;
            }
            else {
                TRACE(Trace::Error, (_T("%s: Status: %d"), __FUNCTION__, status));
            }
        }
        return status;
    }

    uint32_t SharedStorage::endpoint_getStorageSizes(const GetNamespacesParamsInfo& params, GetStorageSizesResultData& response)
    {
        uint32_t status = Core::ERROR_NOT_SUPPORTED;

        if(params.Scope.Value() != ScopeType::DEVICE)
        {
            return status;
        }

        if (nullptr != _sharedInspector)
        {
            Exchange::IStoreInspector::INamespaceSizeIterator* it;
            status = _sharedInspector->GetStorageSizes(
                        Exchange::IStoreInspector::ScopeType(params.Scope.Value()),
                        it);
            if (status == Core::ERROR_NONE) {
                Exchange::IStoreInspector::NamespaceSize element;
                while (it->Next(element) == true) {
                    auto& item = response.StorageList.Add();
                    item.Namespace = element.ns;
                    item.Size = element.size;
                }
                it->Release();
            }
            else {
                TRACE(Trace::Error, (_T("%s: Status: %d"), __FUNCTION__, status));
            }
        }
        return status;
    }

    uint32_t SharedStorage::endpoint_flushCache(DeleteKeyResultInfo& response)
    {
        uint32_t status = Core::ERROR_NOT_SUPPORTED;

        if (nullptr != _sharedCache)
        {
            status = _sharedCache->FlushCache();
            if (status == Core::ERROR_NONE) {
                response.Success = true;
            }
            else {
                TRACE(Trace::Error, (_T("%s: Status: %d"), __FUNCTION__, status));
            }
        }
        return status;
    }

    uint32_t SharedStorage::endpoint_getNamespaceStorageLimit(const DeleteNamespaceParamsInfo& params, GetNamespaceStorageLimitResultData& response)
    {
        uint32_t status = Core::ERROR_NOT_SUPPORTED;

        if(params.Scope.Value() != ScopeType::DEVICE)
        {
            return status;
        }

        if (nullptr != _sharedLimit)
        {
            uint32_t size;
            status = _sharedLimit->GetNamespaceStorageLimit(
                        Exchange::IStoreInspector::ScopeType(params.Scope.Value()),
                        params.Namespace.Value(),
                        size);
            if (status == Core::ERROR_NONE) {
                response.StorageLimit = size;
            }
            else {
                TRACE(Trace::Error, (_T("%s: Status: %d"), __FUNCTION__, status));
            }
        }
        return status;
    }

    uint32_t SharedStorage::endpoint_setNamespaceStorageLimit(const SetNamespaceStorageLimitParamsData& params)
    {
        uint32_t status = Core::ERROR_NOT_SUPPORTED;

        if(params.Scope.Value() != ScopeType::DEVICE)
        {
            return status;
        }

        if (nullptr != _sharedLimit)
        {
            status = _sharedLimit->SetNamespaceStorageLimit(
                        Exchange::IStoreInspector::ScopeType(params.Scope.Value()),
                        params.Namespace.Value(),
                        params.StorageLimit.Value());
        }
        if(status != Core::ERROR_NONE) {
            TRACE(Trace::Error, (_T("%s: Status: %d"), __FUNCTION__, status));
        }
        return status;
    }

} // namespace Plugin
} // namespace WPEFramework
