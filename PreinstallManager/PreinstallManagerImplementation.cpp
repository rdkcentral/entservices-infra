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

#include <chrono>

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
          mPackageManagerInstallerObject(nullptr), mPackageManagerNotification(*this)
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

    /**
     * Initialize the implementation with the current service
     */
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
            if( params.HasLabel("jsonresponse") == false)
            {
                LOGERR("jsonresponse not found in params");
                break;
            }
            std::string jsonresponse = params["jsonresponse"].String();
            LOGINFO("Sending OnAppInstallationStatus event : %s", jsonresponse.c_str());
            mAdminLock.Lock();
            for (auto notification : mPreinstallManagerNotifications)
            {
                notification->OnAppInstallationStatus(jsonresponse);
                LOGTRACE();
            }
            mAdminLock.Unlock();
            break;
        }
        default:
            LOGERR("Unknown event: %d", static_cast<int>(event));
            break;
        }
    }

    /**
     * Pass on the AppInstallationStatus event from package manager to all registered listeners
     */
    void PreinstallManagerImplementation::handleOnAppInstallationStatus(const std::string &jsonresponse)
    {
        if (!jsonresponse.empty())
        {
            JsonObject eventDetails;
            eventDetails["jsonresponse"] = jsonresponse;
            dispatchEvent(PREINSTALL_MANAGER_APP_INSTALLATION_STATUS, eventDetails);
        }
        else
        {
            LOGERR("jsonresponse string from package manager is empty");
        }
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
            mPackageManagerInstallerObject->Register(&mPackageManagerNotification);
            status = Core::ERROR_NONE;
        }
        return status;
    }

    void PreinstallManagerImplementation::releasePackageManagerObject()
    {
        ASSERT(nullptr != mPackageManagerInstallerObject);
        if (mPackageManagerInstallerObject)
        {
            mPackageManagerInstallerObject->Unregister(&mPackageManagerNotification);
            mPackageManagerInstallerObject->Release();
            mPackageManagerInstallerObject = nullptr;
        }
    }

    // helper to validate package version string after stripping any pre-release or build-metadata specifiers
    // currently expects x.y.z where x,y,z are numerical values
    bool PreinstallManagerImplementation::isValidSemVer(const std::string &version)
    {
        int maj = -1, min = -1, patch = -1;
        if (std::sscanf(version.c_str(), "%d.%d.%d", &maj, &min, &patch) != 3)
        {
            return false;
        }
        // for (char c : version)
        // {
        //     if (!(std::isdigit(c) || c == '.'))
        //     {
        //         return false;
        //     }
        // }
        return maj >= 0 && min >= 0 && patch >= 0;


        // todo regex match
        // static const std::regex semverRegex(R"(^\d+\.\d+\.\d+$)");
        // return std::regex_match(version, semverRegex);
    }

    //compare package versions
    bool PreinstallManagerImplementation::isNewerVersion(const std::string &v1, const std::string &v2)
    {
        // Strip at first '-' or '+'
        std::string::size_type pos1 = std::min(v1.find('-'), v1.find('+'));
        std::string::size_type pos2 = std::min(v2.find('-'), v2.find('+'));

        std::string base1 = (pos1 == std::string::npos) ? v1 : v1.substr(0, pos1);
        std::string base2 = (pos2 == std::string::npos) ? v2 : v2.substr(0, pos2);

        if (!isValidSemVer(base1) || !isValidSemVer(base2))
        {
            //LOGINFO("Invalid version strings: %s or %s", base1.c_str(), base2.c_str());
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

    // bool packageWgtExists(const std::string& folderPath) //required??
    // {
    //     std::string packageWgtPath = folderPath + "/package.wgt";
    //     struct stat st;
    //     return (stat(packageWgtPath.c_str(), &st) == 0) && S_ISREG(st.st_mode);
    // }

    /**
     * Traverse the preinstall directory and populate the list of packages to be preinstalled,
     * also fetches the package details
     */
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
            LOGDBG("Found package folder: %s", filepath.c_str());
            if (mPackageManagerInstallerObject->GetConfigForPackage(packageInfo.fileLocator, packageInfo.packageId, packageInfo.version, packageInfo.configMetadata) == Core::ERROR_NONE)
            {
                LOGINFO("Found package: %s, version: %s", packageInfo.packageId.c_str(), packageInfo.version.c_str());
            }
            else
            {
                LOGINFO("Skipping invalid package file: %s", filename.c_str());
                packageInfo.installStatus = "SKIPPED: getConfig failed for [" + filename + "]";
                // continue; -> so that it is printed as skipped and not go undetected
            }
            packages.push_back(packageInfo);
        }

        closedir(dir);
        return true;
    }

    string PreinstallManagerImplementation::getFailReason(FailReason reason) {
            switch (reason) {
                case FailReason::SIGNATURE_VERIFICATION_FAILURE : return "SIGNATURE_VERIFICATION_FAILURE";
                case FailReason::PACKAGE_MISMATCH_FAILURE : return "PACKAGE_MISMATCH_FAILURE";
                case FailReason::INVALID_METADATA_FAILURE : return "INVALID_METADATA_FAILURE";
                case FailReason::PERSISTENCE_FAILURE : return "PERSISTENCE_FAILURE";
                default: return "NONE";
            }
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
        auto installStart = std::chrono::steady_clock::now(); // for measuring duration taken

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
                    LOGINFO("Newer version check failed, skipping package: %s, version: %s , existing_version : %s", toBeInstalledApp->packageId.c_str(), toBeInstalledApp->version.c_str(), found->version.c_str());
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
        // std::list<std::string> failedAppsList;

        for (auto &pkg : preinstallPackages)
        {
            if((pkg.packageId.empty() || pkg.version.empty() || pkg.fileLocator.empty()) /*&& !forceInstall */) // force install anyway
            {
                LOGERR("Skipping invalid package with empty fields: %s", pkg.fileLocator.empty() ? "NULL" : pkg.fileLocator.c_str());
                if(pkg.installStatus.empty()) // do not overwrite if already set to skipped
                {
                    pkg.installStatus = "FAILED: empty fields";
                }
                //populate empty fields to avoid null errors
                pkg.fileLocator = pkg.fileLocator.empty() ? "NULL" : pkg.fileLocator;
                pkg.packageId = pkg.packageId.empty() ? pkg.fileLocator : pkg.packageId; // use fileLocator if packageId is empty for logging
                pkg.version = pkg.version.empty() ? "NULL" : pkg.version;
                // installError = true; //required??
                failedApps++;
                continue; // do not install with empty fields
            }

            LOGINFO("Installing package: %s, version: %s", pkg.packageId.c_str(), pkg.version.c_str());

                // install the package.wgt file inside the folder
                // packageWgtExists(pkg.fileLocator) //required??
            std::string packageLocator = pkg.fileLocator + "/package.wgt";
            FailReason failReason;
            Exchange::IPackageInstaller::IKeyValueIterator* additionalMetadata = nullptr; // todo add additionalMetadata if needed
                // additionalMetadata = Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IKeyValueIterator>>::Create<Exchange::IPackageInstaller::IKeyValueIterator>(keyValues);


            Core::hresult installResult = mPackageManagerInstallerObject->Install(pkg.packageId, pkg.version, additionalMetadata, packageLocator, failReason);
            if (installResult != Core::ERROR_NONE)
            {
                LOGERR("Failed to install package: %s, version: %s, failReason: %s", pkg.packageId.c_str(), pkg.version.c_str(), getFailReason(failReason).c_str());
                installError = true;
                failedApps++;
                // failedAppsList.push_back(pkg.packageId + "_" + pkg.version);
                pkg.installStatus = "FAILED: reason " + getFailReason(failReason);
                continue;
            }
            else
            {
                LOGINFO("Successfully installed package: %s, version: %s, fileLocator: %s", pkg.packageId.c_str(), pkg.version.c_str(), pkg.fileLocator.c_str());
                pkg.installStatus = "SUCCESS";
            }
        }
        auto installEnd = std::chrono::steady_clock::now();
        auto installDuration = std::chrono::duration_cast<std::chrono::seconds>(installEnd - installStart).count();
        auto installDurationMs = std::chrono::duration_cast<std::chrono::milliseconds>(installEnd - installStart).count();
        LOGDBG("StartPreinstall() process completed in %lld seconds (%lld ms)", installDuration, installDurationMs);
        LOGINFO("Installation summary: %d/%d packages installed successfully. %d apps failed.", totalApps - failedApps, totalApps, failedApps);
        // print package wise result
        for (const auto &pkg : preinstallPackages)
        {
            LOGINFO("Package: %s [version:%s]............status:[ %s ]", pkg.packageId.c_str(), pkg.version.c_str(), pkg.installStatus.c_str());
        }

        // print failed apps separately if required.
        // if(failedApps > 0)
        // {
        //     std::string failedAppsStr;
        //     for (const auto &app : failedAppsList)
        //     {
        //         if (!failedAppsStr.empty())
        //         {
        //             failedAppsStr += ", ";
        //         }
        //         failedAppsStr += app;
        //     }
        //     LOGERR("Failed apps: %s", failedAppsStr.c_str());
        // }
        // LOGINFO("Installation summary: Skipped packages: %d", skippedApps);

        //cleanup
        releasePackageManagerObject();

        if(!installError)
        {
            result = Core::ERROR_NONE; // return error if any app install fails todo required??
        }


        return result;
    }

    } /* namespace Plugin */
} /* namespace WPEFramework */