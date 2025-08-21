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


namespace WPEFramework
{
    namespace Plugin
    {

    SERVICE_REGISTRATION(PreinstallManagerImplementation, 1, 0);
    PreinstallManagerImplementation *PreinstallManagerImplementation::_instance = nullptr;

    PreinstallManagerImplementation::PreinstallManagerImplementation()
        : mAdminLock(), mPreinstallManagerNotifications(), mCurrentservice(nullptr)
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

    void handleOnAppInstallationStatus(list<AppInstallInfo> appInstallInfoList)
    {
        JsonArray appInfoJsonArray;
        for (const auto& appInstallInfo : appInstallInfoList)
        {
            JsonObject appInfoJson;
            appInfoJson["packageId"] = appInstallInfo.packageId;
            appInfoJson["version"] = appInstallInfo.version;
            appInfoJson["PreinstallState"] = static_cast<int>(PreinstallState);
            appInfoJson["PreinstallFailReason"] = static_cast<int>(PreinstallFailReason);
            appInfoJsonArray.Add(appInfoJson);
        }
        std::string appInstallInfoArrayString;
        appInstallInfoArrayString = appInfoJsonArray.ToString();

        mAdminLock.Lock();
        for (auto notification: mPreinstallManagerNotifications) {
            notification->OnAppInstallationStatus(appInstallInfoArrayString); //move to dispatchEvent() if necessary?
            LOGTRACE();
        }
        mAdminLock.Unlock();

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
        // Implementation for starting the pre-install process
        LOGINFO("Starting pre-install process with forceInstall=%s", forceInstall ? "true" : "false");
        result = Core::ERROR_NONE; // fix after implementation

        //dummy code to be removed after implementation
        list<AppInstallInfo> appInstallInfoList;
        AppInstallInfo info_a,info_b;
        info_a.packageId = "com.example.app";
        info_a.version = "1.0.0";
        info_a.preinstallState = PreinstallState::PREINSTALL_STATE_INSTALLED;
        info_a.preinstallFailReason = PreinstallFailReason::PREINSTALL_FAIL_REASON_NONE;
        appInstallInfoList.push_back(info_a);
        info_b.packageId = "com.example.app2";
        info_b.version = "1.0.1";
        info_b.preinstallState = PreinstallState::PREINSTALL_STATE_INSTALL_FAILURE;
        info_b.preinstallFailReason = PreinstallFailReason::PREINSTALL_FAIL_REASON_SIGNATURE_VERIFICATION_FAILURE;
        appInstallInfoList.push_back(info_b);

        handleOnAppInstallationStatus(appInstallInfoList);
        return result;
    }

    } /* namespace Plugin */
} /* namespace WPEFramework */