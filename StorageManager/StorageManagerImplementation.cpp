/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2024 RDK Management
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


#include "StorageManagerImplementation.h"
#include "UtilsLogging.h"
#include "RequestHandler.h"
#include <sys/statvfs.h>

#define DEFAULT_APP_STORAGE_PATH        "/opt/persistent/storageManager"

namespace WPEFramework {
namespace Plugin {

    SERVICE_REGISTRATION(StorageManagerImplementation, 1, 0);
    RequestHandler* handler = RequestHandler::getInstance();

    StorageManagerImplementation::StorageManagerImplementation()
    : mCurrentservice(nullptr)
    {
        LOGINFO("Create StorageManagerImplementation Instance");
    }

    StorageManagerImplementation::~StorageManagerImplementation()
    {
        LOGINFO("Delete StorageManagerImplementation Instance");
        handler->releasePersistentStoreRemoteStoreObject();

        if (nullptr != mCurrentservice)
        {
            mCurrentservice->Release();
            mCurrentservice = nullptr;
        }
    }

    uint32_t StorageManagerImplementation::Configure(PluginHost::IShell* service)
    {
        uint32_t result = Core::ERROR_NONE;
        Core::hresult status = Core::ERROR_GENERAL;

        if (service != nullptr)
        {
            mCurrentservice = service;
            if (nullptr != mCurrentservice)
            {
                mCurrentservice->AddRef();

                auto configLine = mCurrentservice->ConfigLine();

                _config.FromString(configLine);
                mBaseStoragePath = _config.Path.Value();
                if (mBaseStoragePath.empty())
                {
                    LOGWARN("Base storage path is empty. Setting default path: %s", DEFAULT_APP_STORAGE_PATH);
                    mBaseStoragePath = DEFAULT_APP_STORAGE_PATH;  // Fallback path
                }
                handler->setCurrentService(mCurrentservice);
                if (Core::ERROR_NONE != handler->createPersistentStoreRemoteStoreObject())
                {
                    LOGERR("Failed to create createPersistentStoreRemoteStoreObject");
                }
                else
                {
                    LOGINFO("created createPersistentStoreRemoteStoreObject");
                }

                Core::SystemInfo::SetEnvironment(PATH_ENV, mBaseStoragePath.c_str());
                LOGINFO("Base Storage Path Set: %s", mBaseStoragePath.c_str());
                handler->SetBaseStoragePath(mBaseStoragePath);
                status = handler->populateAppInfoCacheFromStoragePath();
                if (Core::ERROR_NONE != status)
                {
                    LOGERR("populateAppInfoCacheFromStoragePath Failed!!!");
                }
                else
                {
                    LOGINFO("populateAppInfoCacheFromStoragePath Success!!");
                }
            }
        }
        else
        {
            LOGERR("service is null \n");
            result = Core::ERROR_GENERAL;
        }

        return result;
    }

    /**
     * @brief : Creates storage for a given app id and returns the storage path
     */
    Core::hresult StorageManagerImplementation::CreateStorage(const std::string& appId, const uint32_t& size, std::string& path, std::string& errorReason)
    {
        Core::hresult status = Core::ERROR_GENERAL;
        LOGINFO("Entered CreateStorage Implementation appId: %s", appId.c_str());
        if (appId.empty())
        {
            LOGERR("Invalid App ID");
            errorReason = "appId cannot be empty";
        }
        else
        {
            handler->SetBaseStoragePath(mBaseStoragePath);
            status = handler->CreateStorage(appId, size, path, errorReason);
            if (status == Core::ERROR_NONE)
            {
                LOGINFO("Storage created successfully for appId: %s", appId.c_str());
                return status;
            }

        }
        return status;
    }

    /**
     * @brief : Returns the storage information and location for a given app id
     */
    Core::hresult StorageManagerImplementation::GetStorage(const string& appId, const int32_t& userId, const int32_t& groupId, string& path, uint32_t& size, uint32_t& used)
    {
        Core::hresult status = Core::ERROR_GENERAL;

        LOGINFO("Entered GetStorage Implementation");

        status = handler->GetStorage(appId, userId, groupId, path, size, used);
        if (status == Core::ERROR_NONE)
        {
            LOGINFO("Storage information retrieved successfully for appId: %s", appId.c_str());
            return status;
        }

        return status;
    }

    /**
     * @brief : Deletes storage for a given app id
     */
    Core::hresult StorageManagerImplementation::DeleteStorage(const string& appId, string& errorReason)
    {
        Core::hresult status = Core::ERROR_GENERAL;
        LOGINFO("Entered DeleteStorage Implementation");

        if (appId.empty())
        {
            errorReason = "AppId is empty";
            LOGERR("AppId is empty");
        }
        else
        {
            status = handler->DeleteStorage(appId, errorReason);
            if (status == Core::ERROR_NONE)
            {
                LOGINFO("Storage deleted successfully for appId: %s", appId.c_str());
                return status;
            }
        }
        return status;
    }

    /**
     * @brief : Clears storage for a given app id
     */
    Core::hresult StorageManagerImplementation::Clear(const string& appId, string& errorReason)
    {
        Core::hresult status = Core::ERROR_GENERAL;
        LOGINFO("Entered Clear Implementation");
        status = handler->Clear(appId, errorReason);
        return status;
    }

    /**
     * @brief : Clears all app data except for the exempt app ids
     */
    Core::hresult StorageManagerImplementation::ClearAll(const string& exemptionAppIds, string& errorReason)
    {
        Core::hresult status = Core::ERROR_GENERAL;
        JsonObject parameters;
        LOGINFO("Entered ClearAll Implementation");
        status = RequestHandler::getInstance()->ClearAll(exemptionAppIds, errorReason);
        return status;
    }
} /* namespace Plugin */
} /* namespace WPEFramework */
