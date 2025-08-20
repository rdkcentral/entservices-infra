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
        case APP_EVENT_LIFECYCLE_STATE_CHANGED:
        {
            // string appId = "";
            // string appInstanceId = "";
            // AppLifecycleState newState = Exchange::IPreinstallManager::AppLifecycleState::APP_STATE_UNKNOWN;
            // AppLifecycleState oldState = Exchange::IPreinstallManager::AppLifecycleState::APP_STATE_UNKNOWN;
            // AppErrorReason errorReason = Exchange::IPreinstallManager::AppErrorReason::APP_ERROR_NONE;

            // if (!(params.HasLabel("appId") && !(appId = params["appId"].String()).empty()))
            // {
            //     LOGERR("appId not present or empty");
            // }
            // else if (!(params.HasLabel("appInstanceId") && !(appInstanceId = params["appInstanceId"].String()).empty()))
            // {
            //     LOGERR("appInstanceId not present or empty");
            // }
            // else if (!params.HasLabel("newState"))
            // {
            //     LOGERR("newState not present");
            // }
            // else if (!params.HasLabel("oldState"))
            // {
            //     LOGERR("oldState not present");
            // }
            // else if (!params.HasLabel("errorReason"))
            // {
            //     LOGERR("errorReason not present");
            // }
            // else
            // {
            //     newState = static_cast<AppLifecycleState>(params["newState"].Number());
            //     oldState = static_cast<AppLifecycleState>(params["oldState"].Number());
            //     errorReason = static_cast<AppErrorReason>(params["errorReason"].Number());
            //     mAdminLock.Lock();
            //     for (auto& notification : mPreinstallManagerNotifications)
            //     {
            //         notification->OnAppLifecycleStateChanged(appId, appInstanceId, newState, oldState, errorReason);
            //     }
            //     mAdminLock.Unlock();
            // }
            break;
        }
        default:
            LOGERR("Unknown event: %d", static_cast<int>(event));
            break;
        }
    }


    void PreinstallManagerImplementation::StartPreInstall(bool forceInstall)
    {
        // Implementation for starting the pre-install process
        LOGINFO("Starting pre-install process with forceInstall=%s", forceInstall ? "true" : "false");
        mAdminLock.Lock();
        std::string jsonstr = "{\"forceInstall\": " + std::to_string(forceInstall) + "}";
        for (auto notification: mPreinstallManagerNotifications) {
            notification->OnAppInstallationStatus(jsonstr);
            LOGTRACE();
        }
        mAdminLock.Unlock();
    }

    } /* namespace Plugin */
} /* namespace WPEFramework */