/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
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
**/

#include <chrono>

#include "PackageManagerImplementation.h"

/* Until we don't get it from Package configuration, use size as 1MB */
#define STORAGE_MAX_SIZE 1024

namespace WPEFramework {
namespace Plugin {

    SERVICE_REGISTRATION(PackageManagerImplementation, 1, 0);

    PackageManagerImplementation::PackageManagerImplementation()
        : mDownloaderNotifications()
        , mInstallNotifications()
        , mNextDownloadId(1000)
        , mCurrentservice(nullptr)
        , mStorageManagerObject(nullptr)
    {
        LOGINFO("ctor PackageManagerImplementation: %p", this);
        mHttpClient = std::unique_ptr<HttpClient>(new HttpClient);
    }

    PackageManagerImplementation::~PackageManagerImplementation()
    {
        LOGINFO("dtor PackageManagerImplementation: %p", this);

        std::list<Exchange::IPackageInstaller::INotification*>::iterator index(mInstallNotifications.begin());
        {
            while (index != mInstallNotifications.end()) {
                (*index)->Release();
                index++;
            }
        }
        mInstallNotifications.clear();

        releaseStorageManagerObject();

        std::list<Exchange::IPackageDownloader::INotification*>::iterator itDownloader(mDownloaderNotifications.begin());
        {
            while (itDownloader != mDownloaderNotifications.end()) {
                (*itDownloader)->Release();
                itDownloader++;
            }
        }
        mDownloaderNotifications.clear();
    }

    Core::hresult PackageManagerImplementation::Register(Exchange::IPackageDownloader::INotification* notification)
    {
        LOGINFO();
        ASSERT(notification != nullptr);

        mAdminLock.Lock();
        ASSERT(std::find(mDownloaderNotifications.begin(), mDownloaderNotifications.end(), notification) == mDownloaderNotifications.end());
        if (std::find(mDownloaderNotifications.begin(), mDownloaderNotifications.end(), notification) == mDownloaderNotifications.end()) {
            mDownloaderNotifications.push_back(notification);
            notification->AddRef();
        }

        mAdminLock.Unlock();

        return Core::ERROR_NONE;
    }

    Core::hresult PackageManagerImplementation::Unregister(Exchange::IPackageDownloader::INotification* notification)
    {
        LOGINFO();
        ASSERT(notification != nullptr);
        Core::hresult result = Core::ERROR_NONE;

        done = true;
        cv.notify_one();
        mDownloadThreadPtr->join();

        mAdminLock.Lock();
        auto item = std::find(mDownloaderNotifications.begin(), mDownloaderNotifications.end(), notification);
        if (item != mDownloaderNotifications.end()) {
            notification->Release();
            mDownloaderNotifications.erase(item);
        } else {
            result = Core::ERROR_GENERAL;
        }
        mAdminLock.Unlock();

        return result;
    }

    Core::hresult PackageManagerImplementation::Initialize(PluginHost::IShell* service) {
        Core::hresult result = Core::ERROR_GENERAL;
        LOGINFO();

        if (service != nullptr) {
            mCurrentservice = service;
            mCurrentservice->AddRef();
            if (Core::ERROR_NONE != createStorageManagerObject()) {
                LOGERR("Failed to create createStorageManagerObject");
            } else {
                LOGINFO("created createStorageManagerObject");
                result = Core::ERROR_NONE;
            }

            configStr = service->ConfigLine().c_str();
            LOGINFO("ConfigLine=%s", service->ConfigLine().c_str());
            PackageManagerImplementation::Configuration config;
            config.FromString(service->ConfigLine());
            downloadDir = config.downloadDir;
            LOGINFO("downloadDir=%s", downloadDir.c_str());

            //std::filesystem::create_directories(path);        // XXX: need C++17
            int rc = mkdir(downloadDir.c_str(), 0777);
            if (rc) {
                if (errno != EEXIST) {
                    LOGERR("Failed to create dir '%s' rc: %d errno=%d", downloadDir.c_str(), rc, errno);
                }
            } else {
                LOGDBG("created dir '%s'", downloadDir.c_str());
            }

            #ifdef USE_LIBPACKAGE
            packageImpl = packagemanager::IPackageImpl::instance();
            #endif

            mDownloadThreadPtr = std::unique_ptr<std::thread>(new std::thread(&PackageManagerImplementation::downloader, this, 1));

        } else {
            LOGERR("service is null \n");
        }

        return result;
    }

    Core::hresult PackageManagerImplementation::Deinitialize(PluginHost::IShell* service) {
        Core::hresult result = Core::ERROR_NONE;
        LOGINFO();
        return result;
    }

    Core::hresult PackageManagerImplementation::createStorageManagerObject()
    {
        Core::hresult status = Core::ERROR_GENERAL;

        if (nullptr == mCurrentservice) {
            LOGERR("mCurrentservice is null \n");
        } else if (nullptr == (mStorageManagerObject = mCurrentservice->QueryInterfaceByCallsign<WPEFramework::Exchange::IStorageManager>("org.rdk.StorageManager"))) {
            LOGERR("mStorageManagerObject is null \n");
        } else {
            LOGINFO("created StorageManager Object\n");
            status = Core::ERROR_NONE;
        }
        return status;
    }

    void PackageManagerImplementation::releaseStorageManagerObject()
    {
        ASSERT(nullptr != mStorageManagerObject);
        if(mStorageManagerObject) {
            mStorageManagerObject->Release();
            mStorageManagerObject = nullptr;
        }
    }

    // IPackageDownloader methods
    Core::hresult PackageManagerImplementation::Download(const string& url, const Exchange::IPackageDownloader::Options &options, string &downloadId)
    {
        Core::hresult result = Core::ERROR_NONE;

        std::lock_guard<std::mutex> lock(mMutex);

        // XXX: Check Network here ???
        // Utils::isPluginActivated(NETWORK_PLUGIN_CALLSIGN)

        DownloadInfoPtr di = DownloadInfoPtr(new DownloadInfo(url, std::to_string(++mNextDownloadId), options.retries, options.rateLimit));
        std::string filename = downloadDir + "package" + di->GetId();
        di->SetFileLocator(filename);
        if (options.priority) {
            mDownloadQueue.push_front(di);
        } else {
            mDownloadQueue.push_back(di);
        }
        cv.notify_one();

        downloadId = di->GetId();

        return result;
    }

    Core::hresult PackageManagerImplementation::Pause(const string &downloadId)
    {
        Core::hresult result = Core::ERROR_NONE;

        LOGTRACE("Pausing '%s'", downloadId.c_str());
        if (mInprogressDowload.get() != nullptr) {
            if (downloadId.compare(mInprogressDowload->GetId()) == 0) {
                mHttpClient->pause();
                LOGDBG("%s paused", downloadId.c_str());
            } else {
                result = Core::ERROR_UNKNOWN_KEY;
            }
        } else {
            LOGERR("Pause Failed, mInprogressDowload=%p", mInprogressDowload.get());
            result = Core::ERROR_GENERAL;
        }

        return result;
    }

    Core::hresult PackageManagerImplementation::Resume(const string &downloadId)
    {
        Core::hresult result = Core::ERROR_NONE;

        LOGTRACE("Resuming '%s'", downloadId.c_str());
        if (mInprogressDowload.get() != nullptr) {
            if (downloadId.compare(mInprogressDowload->GetId()) == 0) {
                mHttpClient->resume();
                LOGDBG("%s resumed", downloadId.c_str());
            } else {
                result = Core::ERROR_UNKNOWN_KEY;
            }
        } else {
            LOGERR("Resume Failed, mInprogressDowload=%p", mInprogressDowload.get());
            result = Core::ERROR_GENERAL;
        }

        return result;
    }

    Core::hresult PackageManagerImplementation::Cancel(const string &downloadId)
    {
        Core::hresult result = Core::ERROR_NONE;

        LOGTRACE("Cancelling '%s'", downloadId.c_str());
        if (mInprogressDowload.get() != nullptr) {
            if (downloadId.compare(mInprogressDowload->GetId()) == 0) {
                mInprogressDowload->Cancel();
                mHttpClient->cancel();
                LOGDBG("%s cancelled", downloadId.c_str());
            } else {
                result = Core::ERROR_UNKNOWN_KEY;
            }
        } else {
            LOGERR("Cancel Failed, mInprogressDowload=%p", mInprogressDowload.get());
            result = Core::ERROR_GENERAL;
        }

        return result;
    }

    Core::hresult PackageManagerImplementation::Delete(const string &fileLocator)
    {
        Core::hresult result = Core::ERROR_NONE;

        if ((mInprogressDowload.get() != nullptr) && (fileLocator.compare(mInprogressDowload->GetFileLocator()) == 0)) {
            LOGWARN("%s in in progress", fileLocator.c_str());
            result = Core::ERROR_GENERAL;
        } else {
            if (remove(fileLocator.c_str()) == 0) {
                LOGDBG("Deleted %s", fileLocator.c_str());
            } else {
                LOGERR("'%s' delete failed", fileLocator.c_str());
                result = Core::ERROR_GENERAL;
            }
        }

        return result;
    }

    Core::hresult PackageManagerImplementation::Progress(const string &downloadId, uint8_t &percent)
    {
        Core::hresult result = Core::ERROR_NONE;

        if (mInprogressDowload.get() != nullptr) {
            percent = mHttpClient->getProgress();
        } else {
            result = Core::ERROR_GENERAL;
        }

        return result;
    }

    Core::hresult PackageManagerImplementation::GetStorageDetails(uint32_t &quotaKB, uint32_t &usedKB)
    {
        Core::hresult result = Core::ERROR_NONE;
        return result;
    }

    Core::hresult PackageManagerImplementation::RateLimit(const string &downloadId, uint64_t &limit)
    {
        Core::hresult result = Core::ERROR_NONE;
        return result;
    }

    // IPackageInstaller methods
    Core::hresult PackageManagerImplementation::Install(const string &packageId, const string &version, IPackageInstaller::IKeyValueIterator* const& additionalMetadata, const string &fileLocator, Exchange::IPackageInstaller::FailReason &reason)
    {
        Core::hresult result = Core::ERROR_GENERAL;

        LOGTRACE("Installing '%s' ver:'%s'", packageId.c_str(), version.c_str());

        packagemanager::NameValues keyValues;
        struct IPackageInstaller::KeyValue kv;
        while (additionalMetadata->Next(kv) == true) {
            LOGTRACE("name: %s val: %s", kv.name.c_str(), kv.value.c_str());
            keyValues.push_back(std::make_pair(kv.name, kv.value));
        }

        mAdminLock.Lock();
        StateKey key { packageId, version };
        auto it = mState.find( key );
        if (it == mState.end()) {
            State state;
            mState.insert( { key, state } );
        }

        it = mState.find( key );
        if (it != mState.end()) {
            State &state = it->second;

            if (nullptr == mStorageManagerObject) {
                if (Core::ERROR_NONE != createStorageManagerObject()) {
                    LOGERR("Failed to create StorageManager");
                }
            }
            ASSERT (nullptr != mStorageManagerObject);
            if (nullptr != mStorageManagerObject) {
                string path = "";
                string errorReason = "";
                if(mStorageManagerObject->CreateStorage(packageId, STORAGE_MAX_SIZE, path, errorReason) == Core::ERROR_NONE) {
                    LOGINFO("CreateStorage path [%s]", path.c_str());
                    state.lifecycleState = LifecycleState::INSTALLING;
                    NotifyInstallStatus(packageId, version, LifecycleState::INSTALLING);
                    #ifdef USE_LIBPACKAGE
                    packagemanager::ConfigMetaData config;
                    packagemanager::Result pmResult = packageImpl->Install(packageId, version, keyValues, fileLocator, config);
                    if (pmResult == packagemanager::SUCCESS) {
                        result = Core::ERROR_NONE;
                        state.lifecycleState = LifecycleState::INSTALLED;
                        NotifyInstallStatus(packageId, version, LifecycleState::INSTALLED);
                    } else {
                        // XXX: NotifyInstallStatus(INSTALL_FAILED)
                    }
                    #endif
                } else {
                    LOGERR("CreateStorage failed with result :%d errorReason [%s]", result, errorReason.c_str());
                }
            }
        } else {
            LOGERR("Unknown package id: %s ver: %s", packageId.c_str(), version.c_str());
        }

        mAdminLock.Unlock();
        return result;
    }

    Core::hresult PackageManagerImplementation::Uninstall(const string &packageId, string &errorReason ) {
        Core::hresult result = Core::ERROR_GENERAL;
        string version = "";    // XXX: get it from mState

        LOGTRACE("Uninstalling %s", packageId.c_str());

        mAdminLock.Lock();
        auto it = mState.find( { packageId, version } );
        if (it != mState.end()) {
            auto &state = it->second;

            if (nullptr == mStorageManagerObject) {
                LOGINFO("Create StorageManager object");
                if (Core::ERROR_NONE != createStorageManagerObject()) {
                    LOGERR("Failed to create StorageManager");
                }
            }
            ASSERT (nullptr != mStorageManagerObject);
            if (nullptr != mStorageManagerObject) {
                if(mStorageManagerObject->DeleteStorage(packageId, errorReason) == Core::ERROR_NONE) {
                    LOGINFO("DeleteStorage done");
                    state.lifecycleState = LifecycleState::UNINSTALLING;
                    NotifyInstallStatus(packageId, version, LifecycleState::UNINSTALLING);
                    #ifdef USE_LIBPACKAGE
                    // XXX: what if DeleteStorage() fails, who Uninstall the package
                    packagemanager::Result pmResult = packageImpl->Uninstall(packageId);
                    if (pmResult == packagemanager::SUCCESS) {
                        result = Core::ERROR_NONE;
                    }
                    #endif
                    state.lifecycleState = LifecycleState::UNINSTALLED;
                    NotifyInstallStatus(packageId, version, LifecycleState::UNINSTALLED);
                    // XXX: remove from state/cache
                } else {
                    LOGERR("DeleteStorage failed with result :%d errorReason [%s]", result, errorReason.c_str());
                }
            }
        } else {
            LOGERR("Unknown package id: %s ver: %s", packageId.c_str(), version.c_str());
        }

        mAdminLock.Unlock();

        return result;
    }

    Core::hresult PackageManagerImplementation::ListPackages(Exchange::IPackageInstaller::IPackageIterator*& packages)
    {
        Core::hresult result = Core::ERROR_NONE;
        std::list<Exchange::IPackageInstaller::Package> packageList;

        LOGTRACE();
        #ifdef USE_LIBPACKAGE
        string list;
        packagemanager::Result pmResult = packageImpl->GetList(list);
        if (pmResult == packagemanager::SUCCESS) {
            Json::Value jv;
            Json::Reader reader;

        if (reader.parse(list.c_str(), jv) ) {
            if (jv.isArray()) {
                for (unsigned int i = 0; i < jv.size(); ++i) {
                    Json::Value val = jv[i];

                        Exchange::IPackageInstaller::Package package;
                        package.packageId = val["packageId"].asString().c_str();
                        package.version = val["version"].asString().c_str();
                        package.packageState = LifecycleState::INSTALLED;
                        package.sizeKb = 0;         // XXX: getPackageSpaceInKBytes
                        packageList.emplace_back(package);
                    }
                } else {
                    LOGERR("Invalid json response");
                }
            }
        } else {
            result = Core::ERROR_GENERAL;
        }
        #endif

        packages = (Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IPackageIterator>>::Create<Exchange::IPackageInstaller::IPackageIterator>(packageList));
        LOGTRACE();

        return result;
    }

    Core::hresult PackageManagerImplementation::Config(const string &packageId, const string &version, Exchange::RuntimeConfig& runtimeConfig)
    {
        Core::hresult result = Core::ERROR_NONE;

        LOGTRACE();
        auto it = mState.find( { packageId, version } );
        if (it != mState.end()) {
            auto &state = it->second;
            getRuntimeConfig(state.runtimeConfig, runtimeConfig);
        } else {
            LOGERR("Package: %s Version: %s Not found", packageId.c_str(), version.c_str());
            result = Core::ERROR_GENERAL;
        }
        return result;
    }

    Core::hresult PackageManagerImplementation::PackageState(const string &packageId, const string &version,
        Exchange::IPackageInstaller::PackageLifecycleState &lifecycleState)
    {
        Core::hresult result = Core::ERROR_NONE;

        LOGTRACE();
        auto it = mState.find( { packageId, version } );
        if (it != mState.end()) {
            auto &state = it->second;
            lifecycleState = state.lifecycleState;
        } else {
            LOGERR("Unknown package id: %s ver: %s", packageId.c_str(), version.c_str());
        }

        return result;
    }

    Core::hresult PackageManagerImplementation::Register(Exchange::IPackageInstaller::INotification *notification)
    {
        Core::hresult result = Core::ERROR_NONE;

        LOGINFO();
        ASSERT(notification != nullptr);
        mAdminLock.Lock();
        ASSERT(std::find(mInstallNotifications.begin(), mInstallNotifications.end(), notification) == mInstallNotifications.end());
        if (std::find(mInstallNotifications.begin(), mInstallNotifications.end(), notification) == mInstallNotifications.end()) {
            mInstallNotifications.push_back(notification);
            notification->AddRef();
        }
        mAdminLock.Unlock();

        return result;
    }

    Core::hresult PackageManagerImplementation::Unregister(Exchange::IPackageInstaller::INotification *notification) {
        Core::hresult result = Core::ERROR_NONE;

        LOGINFO();
        mAdminLock.Lock();
        auto item = std::find(mInstallNotifications.begin(), mInstallNotifications.end(), notification);
        if (item != mInstallNotifications.end()) {
            notification->Release();
            mInstallNotifications.erase(item);
        }
        else {
            result = Core::ERROR_GENERAL;
        }
        mAdminLock.Unlock();

        return result;
    }

    // IPackageHandler methods
    Core::hresult PackageManagerImplementation::Lock(const string &packageId, const string &version, const Exchange::IPackageHandler::LockReason &lockReason,
        uint32_t &lockId, string &unpackedPath, Exchange::RuntimeConfig& runtimeConfig, string& appMetadata
        )
    {
        Core::hresult result = Core::ERROR_NONE;

        LOGDBG("id: %s ver: %s reason=%d", packageId.c_str(), version.c_str(), lockReason);

        auto it = mState.find( { packageId, version } );
        if (it != mState.end()) {
            auto &state = it->second;
            #ifdef USE_LIBPACKAGE
            bool locked = false;
            string gatewayMetadataPath;
            if (GetLockedInfo(packageId, version, unpackedPath, runtimeConfig, gatewayMetadataPath, locked) == Core::ERROR_NONE) {
                LOGDBG("id: %s ver: %s locked: %d", packageId.c_str(), version.c_str(), locked);
                getRuntimeConfig(state.runtimeConfig, runtimeConfig);
                if (locked)  {
                    lockId = ++state.mLockCount;
                } else {
                    packagemanager::ConfigMetaData config;
                    packagemanager::Result pmResult = packageImpl->Lock(packageId, version, unpackedPath, config);
                    if (pmResult == packagemanager::SUCCESS) {
                        lockId = ++state.mLockCount;
                        LOGDBG("Locked id: %s ver: %s", packageId.c_str(), version.c_str());
                    } else {
                        LOGERR("Lock Failed id: %s ver: %s", packageId.c_str(), version.c_str());
                        result = Core::ERROR_GENERAL;
                    }
                }
            }
            #endif

            LOGDBG("id: %s ver: %s lock count:%d", packageId.c_str(), version.c_str(), state.mLockCount);
        } else {
            LOGERR("Unknown package id: %s ver: %s", packageId.c_str(), version.c_str());
            result = Core::ERROR_BAD_REQUEST;
        }

        return result;
    }

    // XXX: right way to do this is via copy ctor, when we move Thunder 5.2 and have commone struct RuntimeConfig
    void PackageManagerImplementation::getRuntimeConfig(const Exchange::RuntimeConfig &config, Exchange::RuntimeConfig &runtimeConfig)
    {
        runtimeConfig.dial = config.dial;
        runtimeConfig.wanLanAccess = config.wanLanAccess;
        runtimeConfig.thunder = config.thunder;
        runtimeConfig.systemMemoryLimit = config.systemMemoryLimit;
        runtimeConfig.gpuMemoryLimit = config.gpuMemoryLimit;

        runtimeConfig.userId = config.userId;
        runtimeConfig.groupId = config.groupId;
        runtimeConfig.dataImageSize = config.dataImageSize;

        runtimeConfig.fkpsFiles = config.fkpsFiles;
        runtimeConfig.appType = config.appType;
        runtimeConfig.appPath = config.appPath;
        runtimeConfig.command= config.command;
        runtimeConfig.runtimePath = config.runtimePath;
    }

    void PackageManagerImplementation::getRuntimeConfig(const packagemanager::ConfigMetaData &config, Exchange::RuntimeConfig &runtimeConfig)
    {
        runtimeConfig.dial = config.dial;
        runtimeConfig.wanLanAccess = config.wanLanAccess;
        runtimeConfig.thunder = config.thunder;
        runtimeConfig.systemMemoryLimit = config.systemMemoryLimit;
        runtimeConfig.gpuMemoryLimit = config.gpuMemoryLimit;

        runtimeConfig.userId = config.userId;
        runtimeConfig.groupId = config.groupId;
        runtimeConfig.dataImageSize = config.dataImageSize;

        JsonArray list = JsonArray();
        for (const std::string &fkpsFile : config.fkpsFiles) {
            list.Add(fkpsFile);
        }

        if (!list.ToString(runtimeConfig.fkpsFiles)) {
            LOGERR("Failed to  stringify fkpsFiles to JsonArray");
        }
        runtimeConfig.appType = config.appType == packagemanager::ApplicationType::SYSTEM ? "SYSTEM" : "INTERACTIVE";
        runtimeConfig.appPath = config.appPath;
        runtimeConfig.command= config.command;
        runtimeConfig.runtimePath = config.runtimePath;
    }

    Core::hresult PackageManagerImplementation::Unlock(const string &packageId, const string &version)
    {
        Core::hresult result = Core::ERROR_NONE;

        LOGDBG("id: %s ver: %s", packageId.c_str(), version.c_str());

        auto it = mState.find( { packageId, version } );
        if (it != mState.end()) {
            auto &state = it->second;
            #ifdef USE_LIBPACKAGE
            if (state.mLockCount) {
                packagemanager::Result pmResult = packageImpl->Unlock(packageId, version);
                if (pmResult != packagemanager::SUCCESS) {
                    result = Core::ERROR_GENERAL;
                }
                --state.mLockCount;
            } else {
                LOGERR("Never Locked (mLockCount is 0) id: %s ver: %s", packageId.c_str(), version.c_str());
            }
            #endif
            LOGDBG("id: %s ver: %s lock count:%d", packageId.c_str(), version.c_str(), state.mLockCount);
        } else {
            LOGERR("Unknown package id: %s ver: %s", packageId.c_str(), version.c_str());
            result = Core::ERROR_BAD_REQUEST;
        }

        return result;
    }

    Core::hresult PackageManagerImplementation::GetLockedInfo(const string &packageId, const string &version,
        string &unpackedPath, Exchange::RuntimeConfig& runtimeConfig, string& gatewayMetadataPath, bool &locked)
    {

        Core::hresult result = Core::ERROR_NONE;

        LOGDBG("id: %s ver: %s", packageId.c_str(), version.c_str());
        auto it = mState.find( { packageId, version } );
        if (it != mState.end()) {
            auto &state = it->second;
            getRuntimeConfig(state.runtimeConfig, runtimeConfig);

            #ifdef USE_LIBPACKAGE
            packagemanager::Result pmResult = packageImpl->GetLockInfo(packageId, version, unpackedPath, locked);
            if (pmResult != packagemanager::SUCCESS) {
                result = Core::ERROR_GENERAL;
            }
            #endif
        } else {
            LOGERR("Unknown package id: %s ver: %s", packageId.c_str(), version.c_str());
            result = Core::ERROR_BAD_REQUEST;
        }
        return result;
    }

    void PackageManagerImplementation::InitializeState()
    {
        LOGTRACE();
        #ifdef USE_LIBPACKAGE
        packagemanager::ConfigMetadataArray aConfigMetadata;
        packagemanager::Result pmResult = packageImpl->Initialize(configStr, aConfigMetadata);
        LOGDBG("aConfigMetadata.count:%ld pmResult=%d", aConfigMetadata.size(), pmResult);
        for (auto it = aConfigMetadata.begin(); it != aConfigMetadata.end(); ++it ) {
            StateKey key = it->first;
            State state(it->second);
            mState.insert( { key, state } );
        }
        #endif
        LOGTRACE();
    }

    void PackageManagerImplementation::downloader(int n)
    {
        InitializeState();

        while(!done) {
            auto di = getNext();
            if (di == nullptr) {
                LOGTRACE("Waiting ... ");
                std::unique_lock<std::mutex> lock(mMutex);
                cv.wait(lock);
            } else {
                HttpClient::Status status = HttpClient::Status::Success;
                int waitTime = 1;
                for (int i = 0; i < di->GetRetries(); i++) {
                    if (i) {
                        waitTime = nextRetryDuration(waitTime);
                        LOGDBG("waitTime=%d retry %d/%d", waitTime, i, di->GetRetries());
                        std::this_thread::sleep_for(std::chrono::seconds(waitTime));
                        // XXX: retrying because of server error, Cancel ?!
                        if (di->Cancelled()) {
                            break;
                        }
                    }
                    LOGTRACE("Downloading id=%s url=%s file=%s rateLimit=%ld",
                        di->GetId().c_str(), di->GetUrl().c_str(), di->GetFileLocator().c_str(), di->GetRateLimit());
                    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
                    status = mHttpClient->downloadFile(di->GetUrl(), di->GetFileLocator(), di->GetRateLimit());
                    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
                    if (elapsed) {
                        LOGTRACE("Download status=%d code=%ld time=%ld ms", status,
                            mHttpClient->getStatusCode(),
                            elapsed);
                    }
                    if ( status == HttpClient::Status::Success || mHttpClient->getStatusCode() == 404) {  // XXX: other status codes
                        break;
                    }
                }

                if (mHttpClient->getStatusCode() == 404) {
                    status = HttpClient::Status::HttpError;
                }
                DownloadReason reason = DownloadReason::NONE;
                switch (status) {
                    case HttpClient::Status::DiskError: reason = DownloadReason::DISK_PERSISTENCE_FAILURE; break;
                    case HttpClient::Status::HttpError: reason = DownloadReason::DOWNLOAD_FAILURE; break;
                    default: break; /* Do nothing */
                }
                NotifyDownloadStatus(di->GetId(), di->GetFileLocator(), reason);
                mInprogressDowload.reset();
            }
        }
    }

    void PackageManagerImplementation::NotifyDownloadStatus(const string& id, const string& locator, const DownloadReason reason)
    {
        JsonArray list = JsonArray();
        JsonObject obj;
        obj["downloadId"] = id;
        obj["fileLocator"] = locator;
        obj["reason"] = getDownloadReason(reason);
        list.Add(obj);
        std::string jsonstr;
        if (!list.ToString(jsonstr)) {
            LOGERR("Failed to  stringify JsonArray");
        }

        mAdminLock.Lock();
        for (auto notification: mDownloaderNotifications) {
            notification->OnAppDownloadStatus(jsonstr);
            LOGTRACE();
        }
        mAdminLock.Unlock();
    }

    void PackageManagerImplementation::NotifyInstallStatus(const string& id, const string& version, const LifecycleState state)
    {
        JsonArray list = JsonArray();
        JsonObject obj;
        obj["packageId"] = id;
        obj["version"] = version;
        obj["reason"] = getInstallReason(state);
        list.Add(obj);
        std::string jsonstr;
        if (!list.ToString(jsonstr)) {
            LOGERR("Failed to  stringify JsonArray");
        }

        mAdminLock.Lock();
        for (auto notification: mInstallNotifications) {
            notification->OnAppInstallationStatus(jsonstr);
            LOGTRACE();
        }
        mAdminLock.Unlock();
    }

    PackageManagerImplementation::DownloadInfoPtr PackageManagerImplementation::getNext()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        LOGTRACE("mDownloadQueue.size = %ld\n", mDownloadQueue.size());
        if (!mDownloadQueue.empty() && mInprogressDowload == nullptr) {
            mInprogressDowload = mDownloadQueue.front();
            mDownloadQueue.pop_front();
        }
        return mInprogressDowload;
    }

} // namespace Plugin
} // namespace WPEFramework
