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

namespace WPEFramework {
namespace Plugin {

    SERVICE_REGISTRATION(StorageManagerImplementation, 1, 0);

    StorageManagerImplementation::StorageManagerImplementation()
    : mStorageManagerImplLock()
    {
        LOGINFO("Create StorageManagerImplementation Instance");
    }

    StorageManagerImplementation::~StorageManagerImplementation()
    {
        LOGINFO("Delete StorageManagerImplementation Instance");
    }

/*
 * @brief : Creates storage for a given app id and returns the storage path
 */
    Core::hresult StorageManagerImplementation::CreateStorage(const string& appId, const int32_t& userId, const int32_t& groupId, const uint32_t& size, string& path, string& errorReason)
    {
        Core::hresult status = Core::ERROR_NONE;

        LOGINFO("Entered CreateStorage Implementation");

        return status;
    }

/*
 * @brief : Returns the storage information and location for a given app id
 */
    Core::hresult StorageManagerImplementation::GetStorage(const string& appId, string& path, int32_t& userId, int32_t& groupId, uint32_t& size, uint32_t& used)
    {
        Core::hresult status = Core::ERROR_NONE;

        LOGINFO("Entered GetStorage Implementation");

        return status;
    }

/*
 * @brief : Deletes storage for a given app id
 */
    Core::hresult StorageManagerImplementation::DeleteStorage(const string& appId, string& errorReason)
    {
        Core::hresult status = Core::ERROR_NONE;

        LOGINFO("Entered DeleteStorage Implementation");

        return status;
    }

/*
 * @brief : Clears storage for a given app id
 */
    Core::hresult StorageManagerImplementation::Clear(const string& appId, string& errorReason)
    {
        Core::hresult status = Core::ERROR_NONE;

        LOGINFO("Entered Clear Implementation");

        return status;
    }

/*
 * @brief : Clears all app data except for the exempt app ids
 */
    Core::hresult StorageManagerImplementation::ClearAll(const string& exemptionAppIds, string& errorReason)
    {
        Core::hresult status = Core::ERROR_NONE;

        LOGINFO("Entered ClearAll Implementation");

        return status;
    }

} /* namespace Plugin */
} /* namespace WPEFramework */
