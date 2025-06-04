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

    Core::hresult PackageManagerImplementation::Initialize(PluginHost::IShell* service)
    {
        Core::hresult result = Core::ERROR_GENERAL;
        LOGINFO("entry");

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

            InitializeState();
            PluginHost::ISubSystem* subSystem = service->SubSystems();
            if (subSystem != nullptr) {
                LOGDBG("ISubSystem::INTERNET is %s", subSystem->IsActive(PluginHost::ISubSystem::INTERNET)? "Active" : "Inactive");
                LOGDBG("ISubSystem::INSTALLATION is %s", subSystem->IsActive(PluginHost::ISubSystem::INSTALLATION)? "Active" : "Inactive");
            }
            mDownloadThreadPtr = std::unique_ptr<std::thread>(new std::thread(&PackageManagerImplementation::downloader, this, 1));

        } else {
            LOGERR("service is null \n");
        }

        LOGINFO("exit");
        return result;
    }

    Core::hresult PackageManagerImplementation::Deinitialize(PluginHost::IShell* service)
    {
        Core::hresult result = Core::ERROR_NONE;
        LOGINFO();

        mCurrentservice->Release();
        mCurrentservice = nullptr;

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
    Core::hresult PackageManagerImplementation::Download(const string& url,
        const Exchange::IPackageDownloader::Options &options,
        Exchange::IPackageDownloader::DownloadId &downloadId)
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

        downloadId.downloadId = di->GetId();

        return result;
    }

    Core::hresult PackageManagerImplementation::Pause(const string &downloadId)
    {
        Core::hresult result = Core::ERROR_NONE;

        LOGDBG("Pausing '%s'", downloadId.c_str());
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

        LOGDBG("Resuming '%s'", downloadId.c_str());
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

        LOGDBG("Cancelling '%s'", downloadId.c_str());
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

    Core::hresult PackageManagerImplementation::Progress(const string &downloadId, Percent &percent)
    {
        Core::hresult result = Core::ERROR_NONE;

        if (mInprogressDowload.get() != nullptr) {
            percent.percent = mHttpClient->getProgress();
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

        LOGDBG("Installing '%s' ver:'%s' fileLocator: '%s'", packageId.c_str(), version.c_str(), fileLocator.c_str());
        if (fileLocator.empty()) {
            return Core::ERROR_INVALID_SIGNATURE;
        }

        packagemanager::NameValues keyValues;
        struct IPackageInstaller::KeyValue kv;
        while (additionalMetadata->Next(kv) == true) {
            LOGTRACE("name: %s val: %s", kv.name.c_str(), kv.value.c_str());
            keyValues.push_back(std::make_pair(kv.name, kv.value));
        }

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
                    state.installState = InstallState::INSTALLING;
                    NotifyInstallStatus(packageId, version, state);
                    #ifdef USE_LIBPACKAGE
                    packagemanager::ConfigMetaData config;
                    packagemanager::Result pmResult = packageImpl->Install(packageId, version, keyValues, fileLocator, config);
                    if (pmResult == packagemanager::SUCCESS) {
                        result = Core::ERROR_NONE;
                        state.installState = InstallState::INSTALLED;
                    } else {
                        state.installState = InstallState::INSTALL_FAILURE;
                        state.failReason = (pmResult == packagemanager::Result::VERSION_MISMATCH) ?
                            FailReason::PACKAGE_MISMATCH_FAILURE : FailReason::SIGNATURE_VERIFICATION_FAILURE;
                        LOGERR("Install failed reason %s", getFailReason(state.failReason).c_str());
                    }
                    #endif
                } else {
                    LOGERR("CreateStorage failed with result :%d errorReason [%s]", result, errorReason.c_str());
                    state.failReason = FailReason::PERSISTENCE_FAILURE;
                }
                NotifyInstallStatus(packageId, version, state);
            }
        } else {
            LOGERR("Unknown package id: %s ver: %s", packageId.c_str(), version.c_str());
        }

        return result;
    }

    Core::hresult PackageManagerImplementation::Uninstall(const string &packageId, string &errorReason )
    {
        Core::hresult result = Core::ERROR_GENERAL;
        string version = GetVersion(packageId);

        LOGDBG("Uninstalling id: '%s' ver: '%s'", packageId.c_str(), version.c_str());

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
                    state.installState = InstallState::UNINSTALLING;
                    NotifyInstallStatus(packageId, version, state);
                    #ifdef USE_LIBPACKAGE
                    // XXX: what if DeleteStorage() fails, who Uninstall the package
                    packagemanager::Result pmResult = packageImpl->Uninstall(packageId);
                    if (pmResult == packagemanager::SUCCESS) {
                        result = Core::ERROR_NONE;
                    }
                    #endif
                    state.installState = InstallState::UNINSTALLED;
                    NotifyInstallStatus(packageId, version, state);
                } else {
                    LOGERR("DeleteStorage failed with result :%d errorReason [%s]", result, errorReason.c_str());
                }
            }
        } else {
            LOGERR("Unknown package id: %s ver: %s", packageId.c_str(), version.c_str());
        }

        return result;
    }

    Core::hresult PackageManagerImplementation::ListPackages(Exchange::IPackageInstaller::IPackageIterator*& packages)
    {
        LOGTRACE("entry");
        Core::hresult result = Core::ERROR_NONE;
        std::list<Exchange::IPackageInstaller::Package> packageList;

        for (auto const& [key, state] : mState) {
            Exchange::IPackageInstaller::Package package;
            package.packageId = key.first.c_str();
            package.version = key.second.c_str();
            package.state = state.installState;
            package.sizeKb = state.runtimeConfig.dataImageSize;
            packageList.emplace_back(package);
        }

        packages = (Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IPackageIterator>>::Create<Exchange::IPackageInstaller::IPackageIterator>(packageList));

        LOGTRACE("exit");

        return result;
    }

    Core::hresult PackageManagerImplementation::Config(const string &packageId, const string &version, Exchange::RuntimeConfig& runtimeConfig)
    {
        LOGDBG();
        Core::hresult result = Core::ERROR_NONE;

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
        Exchange::IPackageInstaller::InstallState &installState)
    {
        LOGDBG();
        Core::hresult result = Core::ERROR_NONE;

        auto it = mState.find( { packageId, version } );
        if (it != mState.end()) {
            auto &state = it->second;
            installState = state.installState;
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
    Core::hresult PackageManagerImplementation::Lock(const string &packageId, const string &version,
        const Exchange::IPackageHandler::LockReason &lockReason,
        uint32_t &lockId, string &unpackedPath, Exchange::RuntimeConfig& runtimeConfig,
        Exchange::IPackageHandler::ILockIterator*& appMetadata
        )
    {
        Core::hresult result = Core::ERROR_NONE;

        LOGDBG("id: %s ver: %s reason=%d", packageId.c_str(), version.c_str(), lockReason);

        auto it = mState.find( { packageId, version } );
        if (it != mState.end()) {
            auto &state = it->second;
            #ifdef USE_LIBPACKAGE
            string gatewayMetadataPath;
            bool locked = (state.mLockCount > 0);
            LOGDBG("id: %s ver: %s locked: %d", packageId.c_str(), version.c_str(), locked);
            if (locked)  {
                lockId = ++state.mLockCount;
            } else {
                packagemanager::ConfigMetaData config;
                packagemanager::NameValues locks;
                packagemanager::Result pmResult = packageImpl->Lock(packageId, version, state.unpackedPath, config, locks);
                LOGDBG("unpackedPath=%s", unpackedPath.c_str());
                // save the new config in state
                getRuntimeConfig(config, state.runtimeConfig);   // XXX: config is unnecessary in Lock ?!
                if (pmResult == packagemanager::SUCCESS) {
                    lockId = ++state.mLockCount;

                    std::list<Exchange::IPackageHandler::AdditionalLock> additionalLocks;
                    for (packagemanager::NameValue nv : locks) {
                        Exchange::IPackageHandler::AdditionalLock lock;
                        lock.packageId = nv.first;
                        lock.version = nv.second;
                        additionalLocks.emplace_back(lock);
                    }

                    appMetadata = Core::Service<RPC::IteratorType<Exchange::IPackageHandler::ILockIterator>>::Create<Exchange::IPackageHandler::ILockIterator>(additionalLocks);

                    LOGDBG("Locked id: %s ver: %s", packageId.c_str(), version.c_str());
                } else {
                    LOGERR("Lock Failed id: %s ver: %s", packageId.c_str(), version.c_str());
                    result = Core::ERROR_GENERAL;
                }
            }

            if (result == Core::ERROR_NONE) {
                getRuntimeConfig(state.runtimeConfig, runtimeConfig);
                unpackedPath = state.unpackedPath;
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
                if (--state.mLockCount == 0) {
                    packagemanager::Result pmResult = packageImpl->Unlock(packageId, version);
                    state.unpackedPath = "";
                    if (pmResult != packagemanager::SUCCESS) {
                        result = Core::ERROR_GENERAL;
                    }
                }
            } else {
                LOGERR("Never Locked (mLockCount is 0) id: %s ver: %s", packageId.c_str(), version.c_str());
                result = Core::ERROR_GENERAL;
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
            unpackedPath = state.unpackedPath;
            locked = (state.mLockCount > 0);
            LOGDBG("id: %s ver: %s lock count:%d", packageId.c_str(), version.c_str(), state.mLockCount);
        } else {
            LOGERR("Unknown package id: %s ver: %s", packageId.c_str(), version.c_str());
            result = Core::ERROR_BAD_REQUEST;
        }
        return result;
    }

    void PackageManagerImplementation::InitializeState()
    {
        LOGDBG("entry");
        #ifdef USE_LIBPACKAGE
        packagemanager::ConfigMetadataArray aConfigMetadata;
        packagemanager::Result pmResult = packageImpl->Initialize(configStr, aConfigMetadata);
        LOGDBG("aConfigMetadata.count:%ld pmResult=%d", aConfigMetadata.size(), pmResult);
        for (auto it = aConfigMetadata.begin(); it != aConfigMetadata.end(); ++it ) {
            StateKey key = it->first;
            State state(it->second);
            state.installState = InstallState::INSTALLED;
            mState.insert( { key, state } );
        }
        #endif
        LOGDBG("exit");
    }

    void PackageManagerImplementation::downloader(int n)
    {
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
                    LOGDBG("Downloading id=%s url=%s file=%s rateLimit=%ld",
                        di->GetId().c_str(), di->GetUrl().c_str(), di->GetFileLocator().c_str(), di->GetRateLimit());
                    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
                    status = mHttpClient->downloadFile(di->GetUrl(), di->GetFileLocator(), di->GetRateLimit());
                    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
                    if (elapsed) {
                        LOGDBG("Download status=%d code=%ld time=%ld ms", status,
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
        if (reason != DownloadReason::NONE)
            obj["failReason"] = getDownloadReason(reason);
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

    void PackageManagerImplementation::NotifyInstallStatus(const string& id, const string& version, const State &state)
    {
        JsonArray list = JsonArray();
        JsonObject obj;
        obj["packageId"] = id;
        obj["version"] = version;
        obj["state"] = getInstallState(state.installState);
        if (!((state.installState != InstallState::INSTALLED) || (state.installState != InstallState::UNINSTALLED) ||
            (state.installState != InstallState::INSTALLING) || (state.installState != InstallState::UNINSTALLING))) {
            obj["failReason"] = getFailReason(state.failReason);
        }
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
