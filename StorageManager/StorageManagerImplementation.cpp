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
#include <sys/statvfs.h>

#define MAX_NUM_OF_FILE_DESCRIPTORS     256
#define DEFAULT_STORAGE_DEV_BLOCK_SIZE  512
#define STORAGE_DIR_PERMISSION          0755
#define DEFAULT_APP_STORAGE_PATH        "/opt/persistent/storageManager"

namespace WPEFramework {
namespace Plugin {

    SERVICE_REGISTRATION(StorageManagerImplementation, 1, 0);
    static StorageManagerImplementation::StorageSize gStorageSize;
    std::mutex StorageManagerImplementation::mStorageSizeLock;

    StorageManagerImplementation::StorageManagerImplementation()
    : mStorageManagerImplLock()
    , mCurrentservice(nullptr)
    {
        LOGINFO("Create StorageManagerImplementation Instance");
    }

    StorageManagerImplementation::~StorageManagerImplementation()
    {
        LOGINFO("Delete StorageManagerImplementation Instance");

        if (nullptr != mCurrentservice)
        {
            mCurrentservice->Release();
            mCurrentservice = nullptr;
        }
    }

    uint32_t StorageManagerImplementation::Configure(PluginHost::IShell* service)
    {
        uint32_t result = Core::ERROR_NONE;

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

                Core::SystemInfo::SetEnvironment(PATH_ENV, mBaseStoragePath.c_str());
                LOGINFO("Base Storage Path Set: %s", mBaseStoragePath.c_str());
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
     * @brief : Adds file sizes to calculate total storage usage.
     */
    int StorageManagerImplementation::GetSize(const char *path, const struct stat *statPtr, int currentFlag, struct FTW *internalFtwUsage )
    {
        if (currentFlag == FTW_F && statPtr != nullptr)
        {
            std::lock_guard<std::mutex> storageSizelock(mStorageSizeLock);
            uint64_t usedBytesForFile = ((uint64_t)statPtr->st_size > ((uint64_t)statPtr->st_blocks * (uint64_t)gStorageSize.blockSize)) ?
                                        ((uint64_t)statPtr->st_blocks *(uint64_t)gStorageSize.blockSize) :
                                        (uint64_t)statPtr->st_size;
            gStorageSize.usedBytes += usedBytesForFile;
            LOGINFO("path: %s usedBytes: %llu", path, gStorageSize.usedBytes);
        }
        (void)internalFtwUsage;
        return 0;
    }

    /**
     * @brief : Get the directory size traversing through the given directory path
     */
    uint64_t StorageManagerImplementation::GetDirectorySizeInBytes(const std::string &path)
    {
        const int flags = FTW_DEPTH | FTW_MOUNT | FTW_PHYS;

        std::unique_lock<std::mutex> storageSizelock(mStorageSizeLock);
        gStorageSize.usedBytes = 0;
        storageSizelock.unlock();
        const int result = nftw(path.c_str(), GetSize, MAX_NUM_OF_FILE_DESCRIPTORS, flags);

        if (result == -1)
        {
            LOGERR("nftw returned with [%d]", result);
        }
        storageSizelock.lock();
        return gStorageSize.usedBytes;
    }

    /**
     * @brief : Create or Update App storageInfo map entry for the given appId
     */
    bool StorageManagerImplementation::CreateAppStorageInfoByAppID(const std::string& appId, StorageAppInfo &storageInfo)
    {
        bool result = false;

        if (storageInfo.path.empty())
        {
            LOGWARN("AppId[%s] storage path is empty!", appId.c_str());
        }
        else
        {
            /* Check if the appId exists */
            auto it = mStorageAppInfo.find(appId);
            if (it != mStorageAppInfo.end())
            {
                /* Update existing entry */
                it->second.path     = storageInfo.path;
                it->second.uid      = storageInfo.uid;
                it->second.gid      = storageInfo.gid;
                it->second.quotaKB  = storageInfo.quotaKB;
                LOGINFO("Existing storage entry updated for appId: %s " \
                        "userId: %d groupId: %d quotaKB: %u usedKB: %u path: %s",
                        appId.c_str(), it->second.uid, it->second.gid, it->second.quotaKB, it->second.usedKB, it->second.path.c_str());
            }
            else
            {
                /* Create new entry */
                mStorageAppInfo[appId].path    = storageInfo.path;
                mStorageAppInfo[appId].uid     = storageInfo.uid;
                mStorageAppInfo[appId].gid     = storageInfo.gid;
                mStorageAppInfo[appId].quotaKB = storageInfo.quotaKB;
                mStorageAppInfo[appId].usedKB  = storageInfo.usedKB;
                LOGINFO("Created new storage entry for appId: %s " \
                        "userId: %d groupId: %d quotaKB: %u usedKB: %u path: %s",
                        appId.c_str(), storageInfo.uid, storageInfo.gid, storageInfo.quotaKB, storageInfo.usedKB, storageInfo.path.c_str());
            }
            result = true;
        }
        return result;
    }

    /**
     * @brief : c App StorageInfo for the given appId
     *
     * @return : True if App StorageInfo is available, false otherwise.
     */
    bool StorageManagerImplementation::RetrieveAppStorageInfoByAppID(const string &appId, StorageAppInfo &storageInfo)
    {
        bool result = false;

        /* Check if the appId exists */
        auto it = mStorageAppInfo.find(appId);
        if (it != mStorageAppInfo.end())
        {
            /* Check if the existing storage directory is accessible */
            if (access(it->second.path.c_str(), F_OK) == 0)
            {
                /* Updating the usedKB based on the directory size */
                it->second.usedKB = static_cast<uint32_t>(GetDirectorySizeInBytes(it->second.path) / 1024);
                storageInfo.path    = it->second.path;
                storageInfo.uid     = it->second.uid;
                storageInfo.gid     = it->second.gid;
                storageInfo.quotaKB = it->second.quotaKB;
                storageInfo.usedKB  = it->second.usedKB;
                LOGINFO("App storage entry found for appId: %s " \
                        "userId: %d groupId: %d quotaKB: %u usedKB: %u path: %s",
                        appId.c_str(), storageInfo.uid, storageInfo.gid, storageInfo.quotaKB, storageInfo.usedKB, storageInfo.path.c_str());
                result = true;
            }
            else
            {
                /* Not accessible, need to remove existing storage entry forcibly, recreate it */
                if (RemoveAppStorageInfoByAppID(appId))
                {
                    LOGWARN("Storage path for appID[%s] not accessible - forcibly removed!", it->second.path.c_str());
                }
                else
                {
                    LOGERR("Failed to remove app storage entry forcibly: %s", it->second.path.c_str());
                }
            }
        }
        else
        {
            LOGWARN("AppId[%s] storage entry not found/created", appId.c_str());
        }
        return result;
    }

    /**
     * @brief : Remove App StorageInfo for the given appId
     *
     * @return : True if App StorageInfo map is available and erased, false otherwise.
     */
    bool StorageManagerImplementation::RemoveAppStorageInfoByAppID(const string &appId)
    {
        bool result = false;

        /* Check if appId StorageInfo is found, erase it */
        auto it = mStorageAppInfo.find(appId);
        if (it != mStorageAppInfo.end())
        {
            LOGINFO("App storage entry erased for appId: %s " \
                    "userId: %d groupId: %d quotaKB: %u usedKB: %u path: %s",
                    appId.c_str(), it->second.uid, it->second.gid, it->second.quotaKB, it->second.usedKB, it->second.path.c_str());
            mStorageAppInfo.erase(appId);
            result = true;
        } 
        else
        {
            LOGWARN("AppId[%s] StorageInfo not found", appId.c_str());
        }
        return result;
    }

    /**
     * @brief : Check whether enough storag space is to create a new app Directory.
     *
     * @return : True if enough space is available, false otherwise.
     */
    bool StorageManagerImplementation::HasEnoughStorageFreeSpace(const std::string& baseDir, uint32_t requiredSpaceKB)
    {
        bool hasEnoughSpace = false;
        struct statvfs statFs;
        uint64_t curStorageFreeSpaceKB = 0;
        uint64_t existingAppsReservationSpaceKB = 0;
        uint64_t availableSizeKB = 0;

        if (baseDir.empty())
        {
            LOGERR("Invalid baseDir: path cannot be empty");
        }
        else
        {
            if (statvfs(baseDir.c_str(), &statFs) == 0)
            {
                /* Store the current storage dev block size */
                {
                    std::lock_guard<std::mutex> storageSizelock(mStorageSizeLock);
                    if (0 != statFs.f_frsize)
                    {
                        gStorageSize.blockSize = statFs.f_frsize;
                    }
                    else
                    {
                        gStorageSize.blockSize = DEFAULT_STORAGE_DEV_BLOCK_SIZE; /* Fallback to default block size */
                    }
                }

                /* Calculate the current available storage in KB */
                curStorageFreeSpaceKB = (static_cast<uint64_t>(statFs.f_bfree) * statFs.f_frsize) / 1024;

                /* Compute total reserved space for existing applications */
                for (auto& entry : mStorageAppInfo)
                {
                    entry.second.usedKB = static_cast<uint32_t>(GetDirectorySizeInBytes(entry.second.path) / 1024);

                    /* Ensure applications do not exceed allocated space */
                    if (entry.second.usedKB > entry.second.quotaKB)
                    {
                        LOGERR("Application storage usage exceeded allocation: %s (Allocated: %u KB, Used: %u KB)",
                               entry.second.path.c_str(), entry.second.quotaKB, entry.second.usedKB);
                    }
                    else
                    {
                        /* quotaKB is total allocated space for the app, and usedKB is the current usage */
                        existingAppsReservationSpaceKB += (entry.second.quotaKB - entry.second.usedKB);
                    }
                }

                /* Calculate available space for new apps */
                availableSizeKB = curStorageFreeSpaceKB - existingAppsReservationSpaceKB;

                /* Determine if required space is available */
                if (availableSizeKB >= static_cast<uint64_t>(requiredSpaceKB))
                {
                    LOGINFO("Enough space available. Required: %u KB, Available: %llu KB",
                            requiredSpaceKB, availableSizeKB);
                    hasEnoughSpace = true;
                }
                else
                {
                    LOGERR("Not enough space available. Required: %u KB, Available: %llu KB",
                            requiredSpaceKB, availableSizeKB);
                }
            }
            else
            {
                LOGERR("Failed to get filesystem stats for path: %s, Error: %s",
                        baseDir.c_str(), strerror(errno));
                std::lock_guard<std::mutex> storageSizelock(mStorageSizeLock);
                gStorageSize.blockSize = DEFAULT_STORAGE_DEV_BLOCK_SIZE; /* Fallback to default block size */
            }
        }

        return hasEnoughSpace;
    }

    /**
     * @brief : Creates storage for a given app id and returns the storage path
     */
    Core::hresult StorageManagerImplementation::CreateStorage(const std::string& appId, const int32_t& userId, const int32_t& groupId, const uint32_t& size, std::string& path, std::string& errorReason)
    {
        Core::hresult status = Core::ERROR_GENERAL;
        std::string appDir = "";
        StorageAppInfo storageInfo;

        LOGINFO("Entered CreateStorage Implementation appId: %s userId: %d groupId: %d", appId.c_str(), userId, groupId);
        if (appId.empty())
        {
            LOGERR("Invalid App ID");
            errorReason = "appId cannot be empty";
        }
        else
        {
            std::unique_lock<std::mutex> lock(mStorageManagerImplLock);

            /* Check if the storage mount directory exists or can be created */
            if (mkdir(mBaseStoragePath.c_str(), STORAGE_DIR_PERMISSION) != 0)
            {
                /* Check if the error is not directory already exists */
                if (errno != EEXIST)
                {
                    errorReason = "Failed to create base storage directory: " + appDir;
                    LOGERR("Error creating base storage directory %s", appDir.c_str());
                    goto ret_fail;
                }
            }

            /* Check if the appId storageInfo already exists or can be created */
            if (RetrieveAppStorageInfoByAppID(appId, storageInfo))
            {
                errorReason = "";
                if (storageInfo.uid == userId && storageInfo.gid == groupId && storageInfo.quotaKB == size)
                {
                    /* App storage already exist, no need of map entry update */
                    path = storageInfo.path;
                    status = Core::ERROR_NONE;
                    goto ret_success;
                }
                else 
                {
                    /* Check if the given storage quota size matches the existing one, but if either the UID or GID does not match */
                    if ((storageInfo.quotaKB == size) && \
                        (storageInfo.uid != userId || storageInfo.gid != groupId))
                    {
                        /* In this case, only the map entry for UID and GID needs to be updated */
                        LOGINFO("App storageInfo appId: %s Update{userId: %d groupId: %d}",
                               appId.c_str(), userId, groupId );
                        goto update_storage;
                    }
                    else
                    {
                        /* App storage quote size is different now, remove the existing map entry and new entry can be created */
                        if (!RemoveAppStorageInfoByAppID(appId))
                        {
                            LOGERR("Failed to remove storage appId: %s userId: %d groupId: %d",
                                    appId.c_str(), userId, groupId );
                            errorReason = "Failed to remove storage for appId: " + appId;
                            path = "";
                            goto ret_fail;
                        }
                    }
                }
            }

            if (!HasEnoughStorageFreeSpace(mBaseStoragePath, size))
            {
                LOGERR("Insufficient storage space for app [%s]. Requested: %u KB", appId.c_str(), size);
                errorReason = "Insufficient storage space";
                goto ret_fail;
            }
            else
            {
update_storage:
                /* Check if the app storage directory exists or can be created */
                appDir = mBaseStoragePath + "/" + appId;

                if (mkdir(appDir.c_str(), STORAGE_DIR_PERMISSION) != 0)
                {
                    /* Check if the error is not directory already exists */
                    if (errno != EEXIST)
                    {
                        errorReason = "Failed to create app storage directory: " + appDir;
                        LOGERR("Error creating app storage directory %s", appDir.c_str());
                        goto ret_fail;
                    }
                }

                if (chown(appDir.c_str(), userId, groupId) != 0)
                {
                    LOGERR("Failed to set ownership: %s", strerror(errno));
                    errorReason = "Failed to set ownership: " + appDir;
                    status = Core::ERROR_GENERAL;
                    goto ret_fail;
                }

                /* Create app storage info and add it to the map */
                storageInfo.path    = appDir;
                storageInfo.uid     = userId;
                storageInfo.gid     = groupId;
                storageInfo.quotaKB = size;
                storageInfo.usedKB  = 0; /* Initially, no space is used */
                if (CreateAppStorageInfoByAppID(appId, storageInfo))
                {
                    LOGINFO("Created storage at appDir: %s", appDir.c_str());
                    path = std::move(appDir);
                    status = Core::ERROR_NONE;
                    errorReason = "";
                }
                else
                {
                    LOGERR("Failed to create storage for appId: %s", appDir.c_str());
                    errorReason = "Failed to create storage for appId: " + appId;
                    path = "";
                }
            }
        }

    ret_success:
    ret_fail:
        return status;
    }

    /**
     * @brief : Returns the storage information and location for a given app id
     */
    Core::hresult StorageManagerImplementation::GetStorage(const string& appId, string& path, int32_t& userId, int32_t& groupId, uint32_t& size, uint32_t& used)
    {
        Core::hresult status = Core::ERROR_GENERAL;
        StorageAppInfo storageInfo;

        std::unique_lock<std::mutex> lock(mStorageManagerImplLock);
        LOGINFO("Entered GetStorage Implementation");
        if(RetrieveAppStorageInfoByAppID(appId,storageInfo))
        {
            path     = storageInfo.path;
            userId   = storageInfo.uid;
            groupId  = storageInfo.gid;
            size     = storageInfo.quotaKB;
            used     = storageInfo.usedKB;
            if (storageInfo.usedKB > storageInfo.quotaKB)
            {
                LOGWARN("Application storage usage exceeded allocation: %s (Allocated: %u KB, Used: %u KB)",storageInfo.path.c_str(), storageInfo.quotaKB, storageInfo.usedKB);
            }
            LOGINFO("GetStorage Information path = %s, userId = %d, groupId = %d, size = %u, used = %u ",path.c_str(),userId,groupId,size,used);
            status = Core::ERROR_NONE;
        }
        else
        {
            LOGERR("Failed to get the Information of the appId");
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
            /* Lock the map to ensure thread safety */
            std::unique_lock<std::mutex> lock(mStorageManagerImplLock);
            /* Check if the App Folder exists in the map */
            auto it = mStorageAppInfo.find(appId);
            if (it == mStorageAppInfo.end())
            {
                errorReason = "AppId not found in storage info";
                LOGERR("AppId not found in storage info");
            }
            else
            {
                const std::string path = it->second.path;
                LOGINFO("App Folder exists, attempting to delete: %s", path.c_str());
                if (deleteDirectoryEntries(appId, errorReason) == Core::ERROR_NONE)
                {
                    if (0 == rmdir(path.c_str()))
                    {
                        LOGINFO("App Folder removed successfully");
                        status = Core::ERROR_NONE;
                        LOGINFO("Erasing the App Folder from the database");
                        mStorageAppInfo.erase(it);
                    }
                    else
                    {
                        errorReason = "Error deleting the empty App Folder: " + std::string(strerror(errno));
                        LOGERR("Error deleting the empty App Folder:  reason=%s, Path: %s", strerror(errno), path.c_str());
                    }
                }
            }
        }
        return status;
    }

    /**
     * @brief : Callback function used by nftw to delete files and directories
     *
     * Deletes files and symbolic links using unlink
     * and directories using rmdir
     */
    int deleteCallback(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
    {
        int status = 0;

        if (ftwbuf->level == 0)
        {
            LOGINFO("Skip deleting root directory: %s", path);
        }
        else
        {
            switch (typeflag)
            {
                case FTW_DP:
                    if (rmdir(path) != 0)
                    {
                        LOGERR("Failed to delete directory: %s, Error: %s", path, strerror(errno));
                        status = -1;
                    }
                    break;

                case FTW_F:
                case FTW_SL:
                    if (unlink(path) != 0)
                    {
                        LOGERR("Failed to delete file: %s, Error: %s", path, strerror(errno));
                        status = -1;
                    }
                    break;

                default:
                    LOGERR("Unhandled type: %d for path: %s", typeflag, path);
                    status = -1;
                    break;
            }
        }
        return status;
    }

    /**
     * @brief : Deletes all entries within the specified directory
     */
    Core::hresult StorageManagerImplementation::deleteDirectoryEntries(const string& appId, string& errorReason)
    {
        Core::hresult status = Core::ERROR_GENERAL;
        std::unique_lock<std::mutex> appLock;

        if (!lockAppStorageInfo(appId, appLock))
        {
            errorReason = "Storage not found for appId: " + appId;
        }
        else
        {
        auto it = mStorageAppInfo.find(appId);
        if(it != mStorageAppInfo.end())
        {
            const std::string path = it->second.path;
            LOGINFO("Clearing App storage path: %s", path.c_str());
            if (nftw(path.c_str(), deleteCallback, MAX_NUM_OF_FILE_DESCRIPTORS, FTW_DEPTH | FTW_PHYS) != 0)
            {
                LOGERR("Failed to clear App storage path: %s",path.c_str());
                errorReason = "Failed to clear App storage path: " + path;
            }
            else
            {
                /* Successfully cleared app storage path */
                it->second.usedKB = 0;
                errorReason = "";
                status = Core::ERROR_NONE;
            }
        }
        }

        return status;
    }

    /**
     * @brief Locks the storage information for a specific application.
     *
     * This function searches for the application ID in the mStorageAppInfo map.
     * If found, it acquires a lock on the associated storage mutex to ensure
     * thread-safe access to the application's storage information.
     */
    bool StorageManagerImplementation::lockAppStorageInfo(const std::string& appId, std::unique_lock<std::mutex>& appLock)
    {
        bool status = false;

        LOGINFO("lockAppStorageInfo");

        auto it = mStorageAppInfo.find(appId);
        if (it == mStorageAppInfo.end())
        {
            LOGERR("App ID %s not found in storage info", appId.c_str());
        }
        else
        {
            appLock = std::unique_lock<std::mutex>(it->second.storageLock);
            status = true;
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
        std::unique_lock<std::mutex> lock(mStorageManagerImplLock);

        if (appId.empty())
        {
            errorReason = "Clear called with no appId";
        }
        else
        {
            status = deleteDirectoryEntries(appId, errorReason);
        }
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
        std::unique_lock<std::mutex> lock(mStorageManagerImplLock);

        parameters.FromString(exemptionAppIds);
        const JsonArray exemptedIdsjson = parameters.HasLabel("exemptionAppIds") ? parameters["exemptionAppIds"].Array() : JsonArray();
        std::list<std::string> exemptedIdsStrList;
        for (unsigned int i = 0; i < exemptedIdsjson.Length(); i++)
        {
            exemptedIdsStrList.push_back(exemptedIdsjson[i].String());
        }

        DIR* dir = opendir(mBaseStoragePath.c_str());
        if (!dir)
        {
            errorReason = "Failed to open storage directory: " + mBaseStoragePath;
        }
        else
        {
            bool deletionFailed = false;
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr)
            {
                if (entry->d_type != DT_DIR || strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                {
                    continue;
                }

                string appDirName = entry->d_name;
                if (std::find(exemptedIdsStrList.begin(), exemptedIdsStrList.end(), appDirName) != exemptedIdsStrList.end())
                {
                    LOGINFO("Skipping exempted app directory: %s", appDirName.c_str());
                    continue;
                }

                Core::hresult deleteStatus = deleteDirectoryEntries(appDirName, errorReason);
                if (deleteStatus != Core::ERROR_NONE)
                {
                    LOGERR("Error deleting directory entries for: %s", appDirName.c_str());
                    deletionFailed = true;
                }
            }
            closedir(dir);

            if (!deletionFailed)
            {
                status = Core::ERROR_NONE;
            }
        }
        return status;
    }
} /* namespace Plugin */
} /* namespace WPEFramework */
