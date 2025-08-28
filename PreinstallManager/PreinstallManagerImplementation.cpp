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

#include "PreinstallManagerImplementation.h"

#define AI_PREINSTALL_DIRECTORY "/opt/preinstall" //temporary directory for preinstall packages

namespace WPEFramework
{
    namespace Plugin
    {

    SERVICE_REGISTRATION(PreinstallManagerImplementation, 1, 0);
    PreinstallManagerImplementation *PreinstallManagerImplementation::_instance = nullptr;

    PreinstallManagerImplementation::PreinstallManagerImplementation()
        : mAdminLock(), mPreinstallManagerNotifications(), mCurrentservice(nullptr),
          mPackageManagerInstallerObject(nullptr)
    {
        LOGINFO("Create PreinstallManagerImplementation Instance");
        if (nullptr == PreinstallManagerImplementation::_instance)
        {
            PreinstallManagerImplementation::_instance = this;
        }
    }

    PreinstallManagerImplementation *PreinstallManagerImplementation::getInstance()
    {
        return _instance;
    }

    PreinstallManagerImplementation::~PreinstallManagerImplementation()
    {
        LOGINFO("Delete PreinstallManagerImplementation Instance");
        _instance = nullptr;
        if (nullptr != mCurrentservice)
        {
            mCurrentservice->Release();
            mCurrentservice = nullptr;
        }
        if (nullptr != mPackageManagerInstallerObject)
        {
            releasePackageManagerObject();
        }
    }

    /**
     * Register a notification callback
     */
    Core::hresult PreinstallManagerImplementation::Register(Exchange::IPreinstallManager::INotification *notification)
    {
        ASSERT(nullptr != notification);

        mAdminLock.Lock();

        if (std::find(mPreinstallManagerNotifications.begin(), mPreinstallManagerNotifications.end(), notification) == mPreinstallManagerNotifications.end())
        {
            LOGINFO("Register notification");
            mPreinstallManagerNotifications.push_back(notification);
            notification->AddRef();
        }

        mAdminLock.Unlock();

        return Core::ERROR_NONE;
    }

    /**
     * Unregister a notification callback
     */
    Core::hresult PreinstallManagerImplementation::Unregister(Exchange::IPreinstallManager::INotification *notification)
    {
        Core::hresult status = Core::ERROR_GENERAL;

        ASSERT(nullptr != notification);

        mAdminLock.Lock();

        auto itr = std::find(mPreinstallManagerNotifications.begin(), mPreinstallManagerNotifications.end(), notification);
        if (itr != mPreinstallManagerNotifications.end())
        {
            (*itr)->Release();
            LOGINFO("Unregister notification");
            mPreinstallManagerNotifications.erase(itr);
            status = Core::ERROR_NONE;
        }
        else
        {
            LOGERR("notification not found");
        }

        mAdminLock.Unlock();

        return status;
    }

    uint32_t PreinstallManagerImplementation::Configure(PluginHost::IShell* service)
    {
        uint32_t result = Core::ERROR_GENERAL;
        if (service != nullptr)
        {
            mCurrentservice = service;
            mCurrentservice->AddRef();
            result = Core::ERROR_NONE;
            LOGINFO("PreinstallManagerImplementation service configured successfully");
        }
        else
        {
            LOGERR("service is null \n");
        }

        return result;
    }

    void PreinstallManagerImplementation::dispatchEvent(EventNames event, const JsonObject &params)
    {
        Core::IWorkerPool::Instance().Submit(Job::Create(this, event, params));
    }

    void PreinstallManagerImplementation::Dispatch(EventNames event, const JsonObject params)
    {
        switch (event)
        {
        case PREINSTALL_MANAGER_APP_INSTALLATION_STATUS:
        {
            LOGINFO("Preinstall Manager App Installation Status Dispatch "); //remove

            // handle dispatchEvent() here if necessary
            break;
        }
        default:
            LOGERR("Unknown event: %d", static_cast<int>(event));
            break;
        }
    }

    void PreinstallManagerImplementation::handleOnAppInstallationStatus(std::list<AppInstallInfo> &appInstallInfoList)
    {
        JsonArray appInfoJsonArray;
        for (const auto& appInstallInfo : appInstallInfoList)
        {
            JsonObject appInfoJson;
            appInfoJson["packageId"] = appInstallInfo.packageId;
            appInfoJson["version"] = appInstallInfo.version;
            appInfoJson["PreinstallState"] = static_cast<int>(appInstallInfo.state);
            appInfoJson["PreinstallFailReason"] = static_cast<int>(appInstallInfo.failReason);
            appInfoJsonArray.Add(appInfoJson);
        }
        std::string appInstallInfoArrayString;
        appInfoJsonArray.ToString(appInstallInfoArrayString); //converting back to string

        mAdminLock.Lock();
        for (auto notification: mPreinstallManagerNotifications) {
            notification->OnAppInstallationStatus(appInstallInfoArrayString); //move to dispatchEvent() if necessary?
            LOGTRACE();
        }
        mAdminLock.Unlock();

    }

    Core::hresult PreinstallManagerImplementation::createPackageManagerObject()
    {
        Core::hresult status = Core::ERROR_GENERAL;

        if (nullptr == mCurrentservice)
        {
            LOGERR("mCurrentservice is null \n");
        }
        else if (nullptr == (mPackageManagerInstallerObject = mCurrentservice->QueryInterfaceByCallsign<WPEFramework::Exchange::IPackageInstaller>("org.rdk.PackageManagerRDKEMS")))
        {
            LOGERR("mPackageManagerInstallerObject is null \n");
        }
        else
        {
            LOGINFO("created PackageInstaller Object\n");
            mPackageManagerInstallerObject->AddRef();
            // mPackageManagerInstallerObject->Register(&mPackageManagerNotification);
            status = Core::ERROR_NONE;
        }
        return status;
    }

    void PreinstallManagerImplementation::releasePackageManagerObject()
    {
        ASSERT(nullptr != mPackageManagerInstallerObject);
        if (mPackageManagerInstallerObject)
        {
            // mPackageManagerInstallerObject->Unregister(&mPackageManagerNotification);
            mPackageManagerInstallerObject->Release();
            mPackageManagerInstallerObject = nullptr;
        }
    }

    // helper to validate version after stripping any pre-release or build-metadata specifiers
    // currently expects x.y.z where x,y,z are numerical values
    bool isValidSemVer(const std::string &version)
    {
        int maj = -1, min = -1, patch = -1;
        if (std::sscanf(version.c_str(), "%d.%d.%d", &maj, &min, &patch) != 3)
        {
            return false;
        }
        for (char c : version)
        {
            if (!(std::isdigit(c) || c == '.'))
            {
                return false;
            }
        }
        return maj >= 0 && min >= 0 && patch >= 0;
    }

    bool isNewerVersion(const std::string &v1, const std::string &v2)
    {
        // Strip at first '-' or '+'
        std::string::size_type pos1 = std::min(v1.find('-'), v1.find('+'));
        std::string::size_type pos2 = std::min(v2.find('-'), v2.find('+'));

        std::string base1 = (pos1 == std::string::npos) ? v1 : v1.substr(0, pos1);
        std::string base2 = (pos2 == std::string::npos) ? v2 : v2.substr(0, pos2);

        if (!isValidSemVer(base1) || !isValidSemVer(base2))
        {
            return false; // invalid versions are treated as not newer
        }

        int maj1 = 0, min1 = 0, patch1 = 0;
        int maj2 = 0, min2 = 0, patch2 = 0;
        std::sscanf(base1.c_str(), "%d.%d.%d", &maj1, &min1, &patch1);
        std::sscanf(base2.c_str(), "%d.%d.%d", &maj2, &min2, &patch2);

        if (maj1 != maj2)
            return maj1 > maj2;
        if (min1 != min2)
            return min1 > min2;
        if (patch1 != patch2)
            return patch1 > patch2;

        return false; // equal
    }

    bool PreinstallManagerImplementation::readPreinstallDirectory(std::list<PackageInfo> &packages)
    {
        ASSERT(nullptr != mPackageManagerInstallerObject);
        std::string preinstallDir = AI_PREINSTALL_DIRECTORY;
        DIR *dir = opendir(preinstallDir.c_str());
        if (!dir)
        {
            LOGINFO("Failed to open directory: %s", preinstallDir.c_str());
            return false;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {
            std::string filename(entry->d_name);

            // Skip "." and ".."
            if (filename == "." || filename == "..")
                continue;

            std::string filepath = preinstallDir + "/" + filename;

            PackageInfo packageInfo;
            packageInfo.fileLocator = filepath;
            //debug logs
            LOGDBG("Found package file: %s", filepath.c_str());
            LOGDBG(" Ping getconfig yet to be implemented");
            // API yet to be added
            // if (mPackageManagerInstallerObject->GetConfigForPackage(packageInfo.fileLocator, packageInfo.packageId, packageInfo.version, packageInfo.configMetadata) == Core::ERROR_NONE)
            // {
            //     LOGINFO("Found package: %s, version: %s", packageInfo.packageId.c_str(), packageInfo.version.c_str());
            // }
            // else
            // {
            //     LOGINFO("Skipping invalid package file: %s", filename.c_str());
            //     continue;
            // }
            packages.push_back(packageInfo);
        }

        closedir(dir);
        return true;
    }

    /*
     * @brief Checks the preinstall directory for packages to be preinstalled and installs them as needed.
     * @Params[in]  : bool forceInstall
     * @Params[out] : None
     * @return      : Core::hresult
     */
    Core::hresult PreinstallManagerImplementation::StartPreinstall(bool forceInstall)
    {
        Core::hresult result = Core::ERROR_GENERAL;

        if (nullptr == mPackageManagerInstallerObject)
        {
            LOGINFO("Create PackageManager Remote store object");
            if (Core::ERROR_NONE != createPackageManagerObject())
            {
                LOGERR("Failed to create PackageManagerObject");
                return result;
            }
        }
        ASSERT(nullptr != mPackageManagerInstallerObject);

        // read the preinstall directory and populate packages
        std::list<PackageInfo> preinstallPackages; // all apps in preinstall directory
        if (!readPreinstallDirectory(preinstallPackages))
        {
            LOGERR("Failed to read preinstall directory");
            return result;
        }

        if (!forceInstall)  // if false, we need to check installed packages
        {

            std::list<WPEFramework::Exchange::IPackageInstaller::Package> existingApps;
            Exchange::IPackageInstaller::IPackageIterator *packageList = nullptr;

            // fetch installed packages
            if (mPackageManagerInstallerObject->ListPackages(packageList) != Core::ERROR_NONE && packageList != nullptr)
            {
                LOGERR("ListPackage is returning Error or Packages is nullptr");
                return result;
                // goto End;
            }
            WPEFramework::Exchange::IPackageInstaller::Package package;
            while (packageList->Next(package))
            {
                existingApps.push_back(package);
            }
            // filter to-be-installed apps and removes ones not to be installed based on version
            // check if each to-be-installed app exists already in existingApps
            // if not installed , need to install anyway
            // if installed already, install only if higher version
            for (auto toBeInstalledApp = preinstallPackages.begin(); toBeInstalledApp != preinstallPackages.end(); /* skip */)
            {
                // check if app is already installed
                auto found = std::find_if(
                    existingApps.begin(), existingApps.end(),
                    [&](const WPEFramework::Exchange::IPackageInstaller::Package &installed)
                    {
                        return installed.packageId == toBeInstalledApp->packageId;
                    });

                bool remove = false;
                if (found != existingApps.end())
                {
                    // Found a matching installed package
                    // check if newer version
                    // remove from to-be-installed list if newer version already exists in existingApps
                    if (!isNewerVersion(toBeInstalledApp->version, found->version)) // if to-be-installed app is newer than existing
                    {
                        // older than existing
                        remove = true;
                    }
                }

                if (remove)
                {
                    toBeInstalledApp = preinstallPackages.erase(toBeInstalledApp); // advances to next
                }
                else
                {
                    ++toBeInstalledApp;
                }
            }
        }

        // install the apps
        bool installError = false;
        int  failedApps   = 0;
        int  totalApps    = preinstallPackages.size();
        int  skippedApps  = 0;

        for (const auto &pkg : preinstallPackages)
        {
            if((pkg.packageId.empty() || pkg.version.empty() || pkg.fileLocator.empty()) /*&& !forceInstall */) // force install anyway
            {
                LOGERR("Skipping invalid package with empty fields: %s", pkg.fileLocator.empty() ? "NULL" : pkg.fileLocator.c_str());
                skippedApps++;
                //continue;  todo removed for testing
            }

            // todo multi threading ??
            LOGINFO("Installing package: %s, version: %s", pkg.packageId.c_str(), pkg.version.c_str());
            Exchange::IPackageInstaller::FailReason failReason;
            Core::hresult installResult = mPackageManagerInstallerObject->Install(pkg.packageId, pkg.version, nullptr, pkg.fileLocator, failReason); // todo additionalMetadata null
            if (installResult != Core::ERROR_NONE)
            {
                LOGERR("Failed to install package: %s, version: %s, failReason: %d", pkg.packageId.c_str(), pkg.version.c_str(), failReason);
                installError = true;
                failedApps++;
                continue;
            }
            else
            {
                LOGINFO("Successfully installed package: %s, version: %s, fileLocator: %s", pkg.packageId.c_str(), pkg.version.c_str(), pkg.fileLocator.c_str());
            }
        }

        LOGINFO("Installation summary: %d/%d packages installed successfully. %d apps failed. %d apps skipped.", totalApps - failedApps - skippedApps, totalApps, failedApps, skippedApps);
        // LOGINFO("Installation summary: Skipped packages: %d", skippedApps);

        //cleanup
        // releasePackageManagerObject();

        if(!installError)
        {
            result = Core::ERROR_NONE; // return error if any app install fails todo required??
        }


        return result;
    }














    // // Implementation for starting the pre-install process
    // LOGINFO("Starting pre-install process with forceInstall=%s", forceInstall ? "true" : "false");
    // result = Core::ERROR_NONE; // fix after implementation

    // //dummy code to be removed after implementation
    // std::list<AppInstallInfo> appInstallInfoList;
    // AppInstallInfo info_a,info_b;
    // info_a.packageId = "com.example.app";
    // info_a.version = "1.0.0";
    // info_a.state = PreinstallState::PREINSTALL_STATE_INSTALLED;
    // info_a.failReason = PreinstallFailReason::PREINSTALL_FAIL_REASON_NONE;
    // appInstallInfoList.push_back(info_a);
    // info_b.packageId = "com.example.app2";
    // info_b.version = "1.0.1";
    // info_b.state = PreinstallState::PREINSTALL_STATE_INSTALL_FAILURE;
    // info_b.failReason = PreinstallFailReason::PREINSTALL_FAIL_REASON_SIGNATURE_VERIFICATION_FAILURE;
    // appInstallInfoList.push_back(info_b);

    // handleOnAppInstallationStatus(appInstallInfoList);
    // return result;
    // }

    } /* namespace Plugin */
} /* namespace WPEFramework */