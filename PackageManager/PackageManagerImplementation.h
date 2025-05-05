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

#pragma once

#include <string>
#include <vector>
#include <thread>
#include <memory>
#include <mutex>
#include <condition_variable>

#ifdef USE_LIBPACKAGE
#include <IPackageImpl.h>
#endif
#include <json/json.h>

#include "Module.h"
#include "UtilsLogging.h"
#include <interfaces/IAppPackageManager.h>
#include <interfaces/IStorageManager.h>

#include "HttpClient.h"

namespace WPEFramework {
namespace Plugin {
    typedef Exchange::IPackageDownloader::Reason DownloadReason;
    typedef Exchange::IPackageInstaller::PackageLifecycleState InstallState;

    class PackageManagerImplementation
    : public Exchange::IPackageDownloader
    , public Exchange::IPackageInstaller
    , public Exchange::IPackageHandler
    {

        class Configuration : public Core::JSON::Container {
            public:
                Configuration()
                    : Core::JSON::Container()
                    , downloadDir()
                {
                    Add(_T("downloadDir"), &downloadDir); //
                }
                ~Configuration() = default;

                Configuration(Configuration&&) = delete;
                Configuration(const Configuration&) = delete;
                Configuration& operator=(Configuration&&) = delete;
                Configuration& operator=(const Configuration&) = delete;

            public:
                Core::JSON::String downloadDir;
            };

        class DownloadInfo {
            const unsigned MIN_RETRIES = 2;
            public:
                DownloadInfo(const string& url, const string& id, uint8_t retries, long limit)
                : id(id)
                , url(url)
                , priority(false)
                , retries(retries ? retries : MIN_RETRIES)
                , rateLimit(limit)
                {
                }

                string GetId() { return id; }
                string GetUrl() { return url; }
                bool GetPriority() { return priority; }
                uint8_t GetRetries() { return retries; }
                long GetRateLimit() { return rateLimit; }
                string GetFileLocator() { return fileLocator; }
                void SetFileLocator(string &locator) { fileLocator = locator; }

            private:
                string id;
                string url;
                bool priority;
                uint8_t retries;
                long rateLimit;
                string fileLocator;
        };

        typedef std::shared_ptr<DownloadInfo> DownloadInfoPtr;
        typedef std::list<DownloadInfoPtr> DownloadQueue;

    public:
        PackageManagerImplementation();
        virtual ~PackageManagerImplementation();

        // IPackageDownloader methods
        Core::hresult Download(const string& url, const bool priority, const uint32_t retries, const uint64_t rateLimit, string &downloadId) override;
        Core::hresult Pause(const string &downloadId) override;
        Core::hresult Resume(const string &downloadId) override;
        Core::hresult Cancel(const string &downloadId) override;
        Core::hresult Delete(const string &fileLocator) override;
        Core::hresult Progress(const string &downloadId, uint8_t &percent);
        Core::hresult GetStorageDetails(uint32_t &quotaKB, uint32_t &usedKB);
        Core::hresult RateLimit(const string &downloadId, uint64_t &limit);

        Core::hresult Register(Exchange::IPackageDownloader::INotification* notification) override;
        Core::hresult Unregister(Exchange::IPackageDownloader::INotification* notification) override;

        Core::hresult Initialize(PluginHost::IShell* service) override;
        Core::hresult Deinitialize(PluginHost::IShell* service) override;

        // IPackageInstaller methods
        Core::hresult Install(const string &packageId, const string &version, IPackageInstaller::IKeyValueIterator* const& additionalMetadata, const string &fileLocator, Exchange::IPackageInstaller::FailReason &reason) override;
        Core::hresult Uninstall(const string &packageId, string &errorReason ) override;
        Core::hresult ListPackages(Exchange::IPackageInstaller::IPackageIterator*& packages);
        Core::hresult Config(const string &packageId, const string &version, Exchange::RuntimeConfig& configMetadata) override;
        Core::hresult PackageState(const string &packageId, const string &version, Exchange::IPackageInstaller::PackageLifecycleState &state) override;

        Core::hresult Register(Exchange::IPackageInstaller::INotification *sink) override;
        Core::hresult Unregister(Exchange::IPackageInstaller::INotification *sink) override;

        // IPackageHandler methods
        Core::hresult Lock(const string &packageId, const string &version, const Exchange::IPackageHandler::LockReason &lockReason,
            uint32_t &lockId, string &unpackedPath, Exchange::RuntimeConfig& configMetadata, string& appMetadata
        ) override;

        Core::hresult Unlock(const string &packageId, const string &version) override;
        Core::hresult GetLockedInfo(const string &packageId, const string &version, string &unpackedPath, Exchange::RuntimeConfig& configMetadata,
            string& gatewayMetadataPath, bool &locked) override;


        BEGIN_INTERFACE_MAP(PackageManagerImplementation)
            INTERFACE_ENTRY(Exchange::IPackageDownloader)
            INTERFACE_ENTRY(Exchange::IPackageInstaller)
            INTERFACE_ENTRY(Exchange::IPackageHandler)
        END_INTERFACE_MAP

    private:
        void downloader(int n);
        void NotifyDownloadStatus(const string& id, const string& locator, const DownloadReason status);
        void NotifyInstallStatus(const string& id, const string& version, const InstallState state);

        DownloadInfoPtr getNext();
        int nextRetryDuration(int n) {
            double nxt = n * (1 + sqrt(5)) / 2.0;
            return round(nxt);
        }

        std::string getDownloadReason(DownloadReason reason) {
            switch (reason) {
                case DownloadReason::DOWNLOAD_FAILURE: return "DOWNLOAD_FAILURE";
                case DownloadReason::DISK_PERSISTENCE_FAILURE: return "DISK_PERSISTENCE_FAILURE";
                default: return "NONE";
            }
        }

    std::string getInstallReason(InstallState state) {
        switch (state) {
            case InstallState::INSTALLING : return "INSTALLING";
            case InstallState::INSTALLATION_BLOCKED : return "INSTALLATION_BLOCKED";
            case InstallState::INSTALLED : return "INSTALLED";
            case InstallState::UNINSTALLING : return "UNINSTALLING";
            case InstallState::UNINSTALLED : return "UNINSTALLED";
            default: return "Unknown";
        }
    }
    Core::hresult createStorageManagerObject();
    void releaseStorageManagerObject();

    private:
        mutable Core::CriticalSection mAdminLock;
        std::list<Exchange::IPackageDownloader::INotification*> mDownloaderNotifications;
        std::list<Exchange::IPackageInstaller::INotification*> mInstallNotifications;
        std::unique_ptr<HttpClient> mHttpClient;

        mutable std::mutex mMutex;
        std::condition_variable cv;
        std::unique_ptr<std::thread> mDownloadThreadPtr;
        bool done = false;
        DownloadInfoPtr mInprogressDowload;

        uint32_t mNextDownloadId;
        DownloadQueue  mDownloadQueue;
        std::map<std::string, int>  mLockCount;
        std::string downloadDir = "/opt/CDL/";

        #ifdef USE_LIBPACKAGE
        std::shared_ptr<packagemanager::IPackageImpl> packageImpl;
        #endif
        PluginHost::IShell* mCurrentservice;
        Exchange::IStorageManager* mStorageManagerObject;
    };
} // namespace Plugin
} // namespace WPEFramework
