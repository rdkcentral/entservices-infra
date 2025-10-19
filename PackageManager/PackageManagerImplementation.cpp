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
#include <inttypes.h> // Required for PRIu64

#include "PackageManagerImplementation.h"

/* Until we don't get it from Package configuration, use size as 1MB */
#define STORAGE_MAX_SIZE 1024

#define DEBUG_PRINTF(fmt, ...) \
    std::printf("[DEBUG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

namespace WPEFramework {
namespace Plugin {

    SERVICE_REGISTRATION(PackageManagerImplementation, 1, 0);

    #ifdef USE_LIBPACKAGE
    #define CHECK_CACHE() { if ((packageImpl.get() == nullptr) || (!cacheInitialized)) { \
        return Core::ERROR_UNAVAILABLE; \
    }}
    #endif

    PackageManagerImplementation::PackageManagerImplementation()
        : mDownloaderNotifications()
        , mInstallNotifications()
        , mNextDownloadId(1000)
        , mCurrentservice(nullptr)
        , mStorageManagerObject(nullptr)
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
        , mTelemetryMetricsObject(nullptr)
#endif /* ENABLE_AIMANAGERS_TELEMETRY_METRICS */
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

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
            if (nullptr == (mTelemetryMetricsObject = mCurrentservice->QueryInterfaceByCallsign<WPEFramework::Exchange::ITelemetryMetrics>("org.rdk.TelemetryMetrics")))
            {
                LOGERR("mTelemetryMetricsObject is null \n");
            }
            else
            {
                LOGINFO("created TelemetryMetrics Object");
            }
#endif /* ENABLE_AIMANAGERS_TELEMETRY_METRICS */

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

        done = true;
        cv.notify_one();
        mDownloadThreadPtr->join();

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
        if (nullptr != mTelemetryMetricsObject)
        {
            LOGINFO("TelemetryMetrics object released\n");
            mTelemetryMetricsObject->Release();
            mTelemetryMetricsObject = nullptr;
        }
#endif /* ENABLE_AIMANAGERS_TELEMETRY_METRICS */

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

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
    time_t PackageManagerImplementation::getCurrentTimestamp()
    {
        timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (((time_t)ts.tv_sec * 1000) + ((time_t)ts.tv_nsec / 1000000));
    }

    void PackageManagerImplementation::recordAndPublishTelemetryData(const std::string& marker, const std::string& appId,
                                                                     time_t requestTime, PackageManagerImplementation::PackageFailureErrorCode errorCode)
    {
        std::string telemetryMetrics = "";
        JsonObject jsonParam;
        int duration = 0;
        bool shouldProcessMarker = true;
        bool publish = true;

        if (marker.empty()) {
            LOGERR("Telemetry marker is empty");
        }
        else {
            if (mTelemetryMetricsObject == nullptr) {
                LOGINFO("mTelemetryMetricsObject is null, recreate it");
                mTelemetryMetricsObject = mCurrentservice->QueryInterfaceByCallsign<WPEFramework::Exchange::ITelemetryMetrics>("org.rdk.TelemetryMetrics");

                if (mTelemetryMetricsObject == nullptr) {
                    LOGERR("mTelemetryMetricsObject is still null");
                }
            }

            if (mTelemetryMetricsObject != nullptr) {
                time_t currentTime = getCurrentTimestamp();
                duration = static_cast<int>(currentTime - requestTime);
                LOGINFO("End time for %s: %lu", marker.c_str(), currentTime);

                if (marker == TELEMETRY_MARKER_LAUNCH_TIME) {
                    jsonParam["packageManagerLockTime"] = duration;
                    publish = false;
                }
                else if (marker == TELEMETRY_MARKER_CLOSE_TIME) {
                    jsonParam["packageManagerUnlockTime"] = duration;
                    publish = false;
                }
                else if (marker == TELEMETRY_MARKER_INSTALL_TIME) {
                    jsonParam["installTime"] = duration;
                }
                else if (marker == TELEMETRY_MARKER_UNINSTALL_TIME) {
                    jsonParam["uninstallTime"] = duration;
                }
                else if (marker == TELEMETRY_MARKER_INSTALL_ERROR || marker == TELEMETRY_MARKER_UNINSTALL_ERROR) {
                    jsonParam["errorCode"] = static_cast<int>(errorCode);
                }
                else {
                    LOGERR("Unknown telemetry marker: %s", marker.c_str());
                    shouldProcessMarker = false;
                }

                if (true == shouldProcessMarker) {
                    jsonParam["appId"] = appId;

                    if (jsonParam.ToString(telemetryMetrics)) {
                        LOGINFO("Record appId %s marker %s duration %d", appId.c_str(), marker.c_str(), duration);

                        if (mTelemetryMetricsObject->Record(appId, telemetryMetrics, marker) != Core::ERROR_NONE) {
                            LOGERR("Telemetry Record Failed");
                        }

                        if (publish) {
                            LOGINFO("Publish appId %s marker %s", appId.c_str(), marker.c_str());

                            if (mTelemetryMetricsObject->Publish(appId, marker) != Core::ERROR_NONE) {
                                LOGERR("Telemetry Publish Failed");
                            }
                        }
                    } else {
                        LOGERR("Failed to serialize telemetry metrics");
                    }
                }
            }
        }

        return;
    }
#endif /* ENABLE_AIMANAGERS_TELEMETRY_METRICS */

    // IPackageDownloader methods
    Core::hresult PackageManagerImplementation::Download(const string& url,
        const Exchange::IPackageDownloader::Options &options,
        Exchange::IPackageDownloader::DownloadId &downloadId)
    {
        Core::hresult result = Core::ERROR_NONE;

        if (!mCurrentservice->SubSystems()->IsActive(PluginHost::ISubSystem::INTERNET)) {
            return Core::ERROR_UNAVAILABLE;
        }

        DEBUG_PRINTF("-----------------------DEBUG-2803------------------------");

        std::lock_guard<std::mutex> lock(mMutex);

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

        DEBUG_PRINTF("-----------------------DEBUG-2803------------------------");

        return result;
    }

    Core::hresult PackageManagerImplementation::Pause(const string &downloadId)
    {
        Core::hresult result = Core::ERROR_NONE;

        LOGDBG("Pausing '%s'", downloadId.c_str());
        if (mInprogressDownload.get() != nullptr) {
            if (downloadId.compare(mInprogressDownload->GetId()) == 0) {
                mHttpClient->pause();
                LOGDBG("%s paused", downloadId.c_str());
            } else {
                result = Core::ERROR_UNKNOWN_KEY;
            }
        } else {
            LOGERR("Pause Failed, mInprogressDownload=%p", mInprogressDownload.get());
            result = Core::ERROR_GENERAL;
        }

        return result;
    }

    Core::hresult PackageManagerImplementation::Resume(const string &downloadId)
    {
        Core::hresult result = Core::ERROR_NONE;

        LOGDBG("Resuming '%s'", downloadId.c_str());
        if (mInprogressDownload.get() != nullptr) {
            if (downloadId.compare(mInprogressDownload->GetId()) == 0) {
                mHttpClient->resume();
                LOGDBG("%s resumed", downloadId.c_str());
            } else {
                result = Core::ERROR_UNKNOWN_KEY;
            }
        } else {
            LOGERR("Resume Failed, mInprogressDownload=%p", mInprogressDownload.get());
            result = Core::ERROR_GENERAL;
        }

        return result;
    }

    Core::hresult PackageManagerImplementation::Cancel(const string &downloadId)
    {
        Core::hresult result = Core::ERROR_NONE;

        LOGDBG("Cancelling '%s'", downloadId.c_str());
        if (mInprogressDownload.get() != nullptr) {
            if (downloadId.compare(mInprogressDownload->GetId()) == 0) {
                mInprogressDownload->Cancel();
                mHttpClient->cancel();
                LOGDBG("%s cancelled", downloadId.c_str());
            } else {
                result = Core::ERROR_UNKNOWN_KEY;
            }
        } else {
            LOGERR("Cancel Failed, mInprogressDownload=%p", mInprogressDownload.get());
            result = Core::ERROR_GENERAL;
        }

        return result;
    }

    Core::hresult PackageManagerImplementation::Delete(const string &fileLocator)
    {
        Core::hresult result = Core::ERROR_NONE;

        if ((mInprogressDownload.get() != nullptr) && (fileLocator.compare(mInprogressDownload->GetFileLocator()) == 0)) {
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

    Core::hresult PackageManagerImplementation::Progress(const string &downloadId, ProgressInfo &progress)
    {
        Core::hresult result = Core::ERROR_NONE;

        if (mInprogressDownload.get() != nullptr) {
            progress.progress = mHttpClient->getProgress();
        } else {
            result = Core::ERROR_GENERAL;
        }

        return result;
    }

    Core::hresult PackageManagerImplementation::GetStorageDetails(string &quotaKB, string &usedKB)
    {
        Core::hresult result = Core::ERROR_NONE;
        return result;
    }

    Core::hresult PackageManagerImplementation::RateLimit(const string &downloadId, const uint64_t &limit)
    {
        Core::hresult result = Core::ERROR_NONE;
        LOGDBG("'%s' limit=%" PRIu64, downloadId.c_str(), limit);
        if (mInprogressDownload.get() != nullptr) {
            if (downloadId.compare(mInprogressDownload->GetId()) == 0) {
                mHttpClient->setRateLimit(limit);
            } else {
                result = Core::ERROR_UNKNOWN_KEY;
            }
        } else {
            LOGERR("set RateLimit Failed, mInprogressDownload=%p", mInprogressDownload.get());
            result = Core::ERROR_GENERAL;
        }
        return result;
    }

    // IPackageInstaller methods
    Core::hresult PackageManagerImplementation::Install(const string &packageId, const string &version, IPackageInstaller::IKeyValueIterator* const& additionalMetadata, const string &fileLocator, Exchange::IPackageInstaller::FailReason &reason)
    {
        Core::hresult result = Core::ERROR_GENERAL;
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
        PackageManagerImplementation::PackageFailureErrorCode packageFailureErrorCode = PackageManagerImplementation::PackageFailureErrorCode::ERROR_NONE;
        /* Get current timestamp at the start of Install for telemetry */
        time_t requestTime = getCurrentTimestamp();
#endif /* ENABLE_AIMANAGERS_TELEMETRY_METRICS */

        LOGDBG("Installing '%s' ver:'%s' fileLocator: '%s'", packageId.c_str(), version.c_str(), fileLocator.c_str());
        #ifdef USE_LIBPACKAGE
        CHECK_CACHE()
        #endif
        if (fileLocator.empty()) {
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
            recordAndPublishTelemetryData(TELEMETRY_MARKER_INSTALL_ERROR, packageId, requestTime, PackageManagerImplementation::PackageFailureErrorCode::ERROR_SIGNATURE_VERIFICATION_FAILURE);
#endif /* ENABLE_AIMANAGERS_TELEMETRY_METRICS */
            return Core::ERROR_INVALID_SIGNATURE;
        }

        #ifdef USE_LIBPACKAGE
        packagemanager::NameValues keyValues;
        #endif
        struct IPackageInstaller::KeyValue kv;
        while (additionalMetadata->Next(kv) == true) {
            LOGDBG("name: %s val: %s", kv.name.c_str(), kv.value.c_str());
            #ifdef USE_LIBPACKAGE
            keyValues.push_back(std::make_pair(kv.name, kv.value));
            #endif
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

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                        packageFailureErrorCode = (pmResult == packagemanager::Result::VERSION_MISMATCH) ?
                            PackageManagerImplementation::PackageFailureErrorCode::ERROR_PACKAGE_MISMATCH_FAILURE : PackageManagerImplementation::PackageFailureErrorCode::ERROR_SIGNATURE_VERIFICATION_FAILURE;
#endif /* ENABLE_AIMANAGERS_TELEMETRY_METRICS */
                    }
                    #endif
                } else {
                    LOGERR("CreateStorage failed with result :%d errorReason [%s]", result, errorReason.c_str());
                    state.failReason = FailReason::PERSISTENCE_FAILURE;
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                    packageFailureErrorCode = PackageManagerImplementation::PackageFailureErrorCode::ERROR_PERSISTENCE_FAILURE;
#endif /* ENABLE_AIMANAGERS_TELEMETRY_METRICS */
                }
                NotifyInstallStatus(packageId, version, state);
            }
        } else {
            LOGERR("Package: %s Version: %s Not found", packageId.c_str(), version.c_str());
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
            packageFailureErrorCode = PackageManagerImplementation::PackageFailureErrorCode::ERROR_VERSION_NOT_FOUND;
#endif /* ENABLE_AIMANAGERS_TELEMETRY_METRICS */
        }

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
        recordAndPublishTelemetryData(((PackageManagerImplementation::PackageFailureErrorCode::ERROR_NONE == packageFailureErrorCode) ? TELEMETRY_MARKER_INSTALL_TIME : TELEMETRY_MARKER_INSTALL_ERROR),
                                                    packageId,
                                                    requestTime,
                                                    packageFailureErrorCode);
#endif /* ENABLE_AIMANAGERS_TELEMETRY_METRICS */

        return result;
    }

    Core::hresult PackageManagerImplementation::Uninstall(const string &packageId, string &errorReason )
    {
        Core::hresult result = Core::ERROR_GENERAL;
        string version = GetVersion(packageId);

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
        PackageManagerImplementation::PackageFailureErrorCode packageFailureErrorCode = PackageManagerImplementation::PackageFailureErrorCode::ERROR_NONE;
        /* Get current timestamp at the start of Uninstall for telemetry */
        time_t requestTime = getCurrentTimestamp();
#endif /* ENABLE_AIMANAGERS_TELEMETRY_METRICS */

        LOGDBG("Uninstalling id: '%s' ver: '%s'", packageId.c_str(), version.c_str());
        #ifdef USE_LIBPACKAGE
        CHECK_CACHE()
        #endif
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
                    else{
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                        packageFailureErrorCode = (pmResult == packagemanager::Result::VERSION_MISMATCH) ?
                            PackageManagerImplementation::PackageFailureErrorCode::ERROR_PACKAGE_MISMATCH_FAILURE : PackageManagerImplementation::PackageFailureErrorCode::ERROR_SIGNATURE_VERIFICATION_FAILURE;
#endif /* ENABLE_AIMANAGERS_TELEMETRY_METRICS */
                    }
                    #endif
                    state.installState = InstallState::UNINSTALLED;
                    NotifyInstallStatus(packageId, version, state);
                } else {
                    LOGERR("DeleteStorage failed with result :%d errorReason [%s]", result, errorReason.c_str());
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                    packageFailureErrorCode = PackageManagerImplementation::PackageFailureErrorCode::ERROR_PERSISTENCE_FAILURE;
#endif /* ENABLE_AIMANAGERS_TELEMETRY_METRICS */

                }
            }
        } else {
            LOGERR("Package: %s Version: %s Not found", packageId.c_str(), version.c_str());
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
            packageFailureErrorCode = PackageManagerImplementation::PackageFailureErrorCode::ERROR_VERSION_NOT_FOUND;
#endif /* ENABLE_AIMANAGERS_TELEMETRY_METRICS */

        }

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
        recordAndPublishTelemetryData(((PackageManagerImplementation::PackageFailureErrorCode::ERROR_NONE == packageFailureErrorCode) ? TELEMETRY_MARKER_UNINSTALL_TIME : TELEMETRY_MARKER_UNINSTALL_ERROR),
                                                    packageId,
                                                    requestTime,
                                                    packageFailureErrorCode);
#endif /* ENABLE_AIMANAGERS_TELEMETRY_METRICS */
        return result;
    }

    Core::hresult PackageManagerImplementation::ListPackages(Exchange::IPackageInstaller::IPackageIterator*& packages)
    {
        #ifdef USE_LIBPACKAGE
        CHECK_CACHE()
        #endif
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
        #ifdef USE_LIBPACKAGE
        CHECK_CACHE()
        #endif
        LOGDBG();
        Core::hresult result = Core::ERROR_NONE;

        DEBUG_PRINTF("-----------------------DEBUG-2803------------------------");
        auto it = mState.find( { packageId, version } );
        if (it != mState.end()) {
            DEBUG_PRINTF("-----------------------DEBUG-2803------------------------");
            auto &state = it->second;
            DEBUG_PRINTF("-----------------------DEBUG-2803------------------------");
            getRuntimeConfig(state.runtimeConfig, runtimeConfig);
        } else {
            LOGERR("Package: %s Version: %s Not found", packageId.c_str(), version.c_str());
            result = Core::ERROR_GENERAL;
        }
        DEBUG_PRINTF("-----------------------DEBUG-2803------------------------");
        return result;
    }

    Core::hresult PackageManagerImplementation::PackageState(const string &packageId, const string &version,
        Exchange::IPackageInstaller::InstallState &installState)
    {
        #ifdef USE_LIBPACKAGE
        CHECK_CACHE()
        #endif
        LOGDBG();
        Core::hresult result = Core::ERROR_NONE;

        auto it = mState.find( { packageId, version } );
        if (it != mState.end()) {
            auto &state = it->second;
            installState = state.installState;
        } else {
            LOGERR("Package: %s Version: %s Not found", packageId.c_str(), version.c_str());
            result = Core::ERROR_GENERAL;
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
            DEBUG_PRINTF("-----------------------DEBUG-2803------------------------");
            notification->AddRef();
        }
        DEBUG_PRINTF("-----------------------DEBUG-2803------------------------");
        mAdminLock.Unlock();
        DEBUG_PRINTF("-----------------------DEBUG-2803------------------------");
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

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
        /* Get current timestamp at the start of Lock for telemetry */
        time_t requestTime = getCurrentTimestamp();
#endif /* ENABLE_AIMANAGERS_TELEMETRY_METRICS */

        LOGDBG("id: %s ver: %s reason=%d", packageId.c_str(), version.c_str(), lockReason);
        #ifdef USE_LIBPACKAGE
        CHECK_CACHE()
        #endif
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

                    state.additionalLocks.clear();
                    for (packagemanager::NameValue nv : locks) {
                        Exchange::IPackageHandler::AdditionalLock lock;
                        lock.packageId = nv.first;
                        lock.version = nv.second;
                        state.additionalLocks.emplace_back(lock);
                    }
                    LOGDBG("Locked id: %s ver: %s additionalLocks=%zu", packageId.c_str(), version.c_str(), state.additionalLocks.size());
                } else {
                    LOGERR("Lock Failed id: %s ver: %s", packageId.c_str(), version.c_str());
                    result = Core::ERROR_GENERAL;
                }
            }

            if (result == Core::ERROR_NONE) {
                getRuntimeConfig(state.runtimeConfig, runtimeConfig);
                unpackedPath = state.unpackedPath;
                appMetadata = Core::Service<RPC::IteratorType<Exchange::IPackageHandler::ILockIterator>>::Create<Exchange::IPackageHandler::ILockIterator>(state.additionalLocks);

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                recordAndPublishTelemetryData(TELEMETRY_MARKER_LAUNCH_TIME,
                                                            packageId,
                                                            requestTime,
                                                            PackageManagerImplementation::PackageFailureErrorCode::ERROR_NONE);
#endif /* ENABLE_AIMANAGERS_TELEMETRY_METRICS */

            }
            #endif

            LOGDBG("id: %s ver: %s lock count:%d", packageId.c_str(), version.c_str(), state.mLockCount);
        } else {
            LOGERR("Package: %s Version: %s Not found", packageId.c_str(), version.c_str());
            result = Core::ERROR_BAD_REQUEST;
        }

        return result;
    }

    // XXX: right way to do this is via copy ctor, when we move to Thunder 5.2 and have commone struct RuntimeConfig
    void PackageManagerImplementation::getRuntimeConfig(const Exchange::RuntimeConfig &config, Exchange::RuntimeConfig &runtimeConfig)
    {
        DEBUG_PRINTF("-----------------------DEBUG-2803------------------------");
        runtimeConfig.dial = config.dial;
        runtimeConfig.wanLanAccess = config.wanLanAccess;
        runtimeConfig.thunder = config.thunder;
        runtimeConfig.systemMemoryLimit = config.systemMemoryLimit;
        runtimeConfig.gpuMemoryLimit = config.gpuMemoryLimit;
        runtimeConfig.envVariables = config.envVariables;

        runtimeConfig.userId = config.userId;
        runtimeConfig.groupId = config.groupId;
        runtimeConfig.dataImageSize = config.dataImageSize;

        runtimeConfig.fkpsFiles = config.fkpsFiles;
        runtimeConfig.appType = config.appType;
        runtimeConfig.appPath = config.appPath;
        runtimeConfig.command = config.command;
        runtimeConfig.runtimePath = config.runtimePath;
        DEBUG_PRINTF("-----------------------DEBUG-2803------------------------");
    }

    #ifdef USE_LIBPACKAGE
    void PackageManagerImplementation::getRuntimeConfig(const packagemanager::ConfigMetaData &config, Exchange::RuntimeConfig &runtimeConfig)
    {
        runtimeConfig.dial = config.dial;
        runtimeConfig.wanLanAccess = config.wanLanAccess;
        runtimeConfig.thunder = config.thunder;
        runtimeConfig.systemMemoryLimit = config.systemMemoryLimit;
        runtimeConfig.gpuMemoryLimit = config.gpuMemoryLimit;

        JsonArray vars = JsonArray();
        for (auto str: config.envVars) {
            vars.Add(str);
        }
        vars.ToString(runtimeConfig.envVariables);

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
        runtimeConfig.command = config.command;
        runtimeConfig.runtimePath = config.runtimePath;
    }
    #endif

    Core::hresult PackageManagerImplementation::Unlock(const string &packageId, const string &version)
    {
        Core::hresult result = Core::ERROR_NONE;

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
        /* Get current timestamp at the start of Lock for telemetry */
        time_t requestTime = getCurrentTimestamp();
#endif /* ENABLE_AIMANAGERS_TELEMETRY_METRICS */

        LOGDBG("id: %s ver: %s", packageId.c_str(), version.c_str());
        #ifdef USE_LIBPACKAGE
        CHECK_CACHE()
        #endif
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
            LOGERR("Package: %s Version: %s Not found", packageId.c_str(), version.c_str());
            result = Core::ERROR_BAD_REQUEST;
        }

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
        if (Core::ERROR_NONE == result)
        {
            recordAndPublishTelemetryData(TELEMETRY_MARKER_CLOSE_TIME,
                                                       packageId,
                                                       requestTime,
                                                       PackageManagerImplementation::PackageFailureErrorCode::ERROR_NONE);
        }
#endif /* ENABLE_AIMANAGERS_TELEMETRY_METRICS */

        return result;
    }

    Core::hresult PackageManagerImplementation::GetLockedInfo(const string &packageId, const string &version,
        string &unpackedPath, Exchange::RuntimeConfig& runtimeConfig, string& gatewayMetadataPath, bool &locked)
    {
        #ifdef USE_LIBPACKAGE
        CHECK_CACHE()
        #endif
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
            LOGERR("Package: %s Version: %s Not found", packageId.c_str(), version.c_str());
            result = Core::ERROR_BAD_REQUEST;
        }
        return result;
    }

    Core::hresult PackageManagerImplementation::GetConfigForPackage(const string &fileLocator, string& id, string &version, Exchange::RuntimeConfig& config)
    {
        #ifdef USE_LIBPACKAGE
        CHECK_CACHE()
        #endif
        Core::hresult result = Core::ERROR_GENERAL;

        if (fileLocator.empty())
        {
            return Core::ERROR_INVALID_SIGNATURE;
        }

        #ifdef USE_LIBPACKAGE
        packagemanager::ConfigMetaData metadata;
        packagemanager::Result pmResult = packageImpl->GetFileMetadata(fileLocator, id, version, metadata);
        if (pmResult == packagemanager::SUCCESS)
        {
            getRuntimeConfig(metadata, config);
            result = Core::ERROR_NONE;
        }
        #endif
        return result;
    }

    void PackageManagerImplementation::InitializeState()
    {
        LOGDBG("entry");
	#ifdef USE_THUNDER_R443
        PluginHost::ISubSystem* subSystem = mCurrentservice->SubSystems();
        if (subSystem != nullptr) {
            subSystem->Set(PluginHost::ISubSystem::NOT_INSTALLATION, nullptr);
        }
        #endif

        #ifdef USE_LIBPACKAGE
        packageImpl = packagemanager::IPackageImpl::instance();

        packagemanager::ConfigMetadataArray aConfigMetadata;
        packagemanager::Result pmResult = packageImpl->Initialize(configStr, aConfigMetadata);
        LOGDBG("aConfigMetadata.count:%zu pmResult=%d", aConfigMetadata.size(), pmResult);
        for (auto it = aConfigMetadata.begin(); it != aConfigMetadata.end(); ++it ) {
            StateKey key = it->first;
            State state(it->second);
            state.installState = InstallState::INSTALLED;
            mState.insert( { key, state } );
        }
        #endif

        #ifdef USE_THUNDER_R443
        if (subSystem != nullptr) {
            subSystem->Set(PluginHost::ISubSystem::INSTALLATION, nullptr);
        }
        #endif
        
        cacheInitialized = true;
        LOGDBG("exit");
    }

    void PackageManagerImplementation::downloader(int n)
    {
        InitializeState();
        while(!done) {
            auto di = getNext();
            if (di == nullptr) {
                LOGTRACE("Waiting ... ");
                std::unique_lock<std::mutex> lock(mMutex);
                cv.wait(lock, [this] { return done || (mDownloadQueue.size() > 0); });
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
                        /*LOGDBG("Download status=%d code=%ld time=%ld ms", status,
                            mHttpClient->getStatusCode(),
                            elapsed);*/
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
                mInprogressDownload.reset();
            }
        }
    }

    void PackageManagerImplementation::NotifyDownloadStatus(const string& id, const string& locator, const DownloadReason reason)
    {

        std::list<Exchange::IPackageDownloader::PackageInfo> packageInfoList;
        Exchange::IPackageDownloader::PackageInfo packageInfo;
        packageInfo.downloadId = id;
        packageInfo.fileLocator = locator;
        packageInfo.reason = reason;
        packageInfoList.emplace_back(packageInfo);

        Exchange::IPackageDownloader::IPackageInfoIterator* packageInfoIterator = Core::Service<RPC::IteratorType<Exchange::IPackageDownloader::IPackageInfoIterator>>::Create<Exchange::IPackageDownloader::IPackageInfoIterator>(packageInfoList);

        mAdminLock.Lock();
        for (auto notification: mDownloaderNotifications) {
            notification->OnAppDownloadStatus(packageInfoIterator);
            LOGTRACE();
        }
        mAdminLock.Unlock();
    }

    void PackageManagerImplementation::NotifyInstallStatus(const string& id, const string& version, const State &state)
    {
        DEBUG_PRINTF("-----------------------DEBUG-2803------------------------");
        JsonArray list = JsonArray();
        JsonObject obj;
        obj["packageId"] = id;
        obj["version"] = version;
        obj["state"] = getInstallState(state.installState);
        if (!((state.installState == InstallState::INSTALLED) || (state.installState == InstallState::UNINSTALLED) ||
            (state.installState == InstallState::INSTALLING) || (state.installState == InstallState::UNINSTALLING))) {
            DEBUG_PRINTF("-----------------------DEBUG-2803------------------------");
            obj["failReason"] = getFailReason(state.failReason);
        }
        list.Add(obj);
        std::string jsonstr;
        if (!list.ToString(jsonstr)) {
            LOGERR("Failed to  stringify JsonArray");
        }
        DEBUG_PRINTF("-----------------------DEBUG-2803------------------------");
        mAdminLock.Lock();
        for (auto notification: mInstallNotifications) {
            DEBUG_PRINTF("-----------------------DEBUG-2803------------------------");
            notification->OnAppInstallationStatus(jsonstr);
            LOGTRACE();
        }
        DEBUG_PRINTF("-----------------------DEBUG-2803------------------------");
        mAdminLock.Unlock();
    }

    PackageManagerImplementation::DownloadInfoPtr PackageManagerImplementation::getNext()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        LOGTRACE("mDownloadQueue.size = %ld\n", mDownloadQueue.size());
        if (!mDownloadQueue.empty() && mInprogressDownload == nullptr) {
            mInprogressDownload = mDownloadQueue.front();
            mDownloadQueue.pop_front();
        }
        return mInprogressDownload;
    }

} // namespace Plugin
} // namespace WPEFramework
