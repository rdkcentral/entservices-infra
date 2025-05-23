/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2020 RDK Management
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

#include "Module.h"
#include "LifecycleInterfaceConnector.h"
#include <string>
#include <memory>
#include <iostream>
#include <mutex>
#include <thread>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <plugins/System.h>

#include <interfaces/ILifecycleManager.h>

#include "UtilsString.h"
#include "AppManagerImplementation.h"

#define SUSPEND_POLICY_FILE        "/tmp/AI2.0Suspendable"
using namespace std;
using namespace Utils;

namespace WPEFramework
{
    namespace Plugin
    {
        LifecycleInterfaceConnector* LifecycleInterfaceConnector::_instance = nullptr;
        static uint32_t gAppsActiveCounter = 0;

        LifecycleInterfaceConnector::LifecycleInterfaceConnector(PluginHost::IShell* service)
        : mLifecycleManagerRemoteObject(nullptr),
          mLifecycleManagerStateRemoteObject(nullptr),
          mNotification(*this),
          mCurrentservice(nullptr)
        {
            LOGINFO("Create LifecycleInterfaceConnector Instance");
            LifecycleInterfaceConnector::_instance = this;
            if (service != nullptr)
            {
                mCurrentservice = service;
                mCurrentservice->AddRef();
            }
        }

        LifecycleInterfaceConnector::~LifecycleInterfaceConnector()
        {
            if (nullptr != mCurrentservice)
            {
               mCurrentservice->Release();
               mCurrentservice = nullptr;
            }
            LifecycleInterfaceConnector::_instance = nullptr;
        }

        Core::hresult LifecycleInterfaceConnector::createLifecycleManagerRemoteObject()
        {
            Core::hresult status = Core::ERROR_GENERAL;

            if (nullptr == mCurrentservice)
            {
                LOGWARN("mCurrentservice is null \n");
            }
            else if (nullptr == (mLifecycleManagerRemoteObject = mCurrentservice->QueryInterfaceByCallsign<WPEFramework::Exchange::ILifecycleManager>("org.rdk.LifecycleManager")))
            {
                LOGWARN("Failed to create LifecycleManager Remote object\n");
            }
            else if (nullptr == (mLifecycleManagerStateRemoteObject = mCurrentservice->QueryInterfaceByCallsign<WPEFramework::Exchange::ILifecycleManagerState>("org.rdk.LifecycleManager")))
            {
                LOGWARN("Failed to create LifecycleManagerState Remote object\n");
            }
            else
            {
                LOGINFO("Created LifecycleManager Remote Object \n");
                mLifecycleManagerRemoteObject->AddRef();

                LOGINFO("Created LifecycleManagerState Remote Object \n");
                mLifecycleManagerStateRemoteObject->AddRef();
                mLifecycleManagerStateRemoteObject->Register(&mNotification);
                LOGINFO("LifecycleManagerState notification registered");

                status = Core::ERROR_NONE;
            }
            return status;
        }

        void LifecycleInterfaceConnector::releaseLifecycleManagerRemoteObject()
        {
            ASSERT(nullptr != mLifecycleManagerRemoteObject );
            if(mLifecycleManagerRemoteObject )
            {
                mLifecycleManagerRemoteObject ->Release();
                mLifecycleManagerRemoteObject = nullptr;
            }

            ASSERT(nullptr != mLifecycleManagerStateRemoteObject );
            if(mLifecycleManagerStateRemoteObject )
            {
                mLifecycleManagerStateRemoteObject->Unregister(&mNotification);
                mLifecycleManagerStateRemoteObject ->Release();
                mLifecycleManagerStateRemoteObject = nullptr;
            }
        }

        Core::hresult LifecycleInterfaceConnector::isAppLoaded(const string& appId, bool& loaded)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            mAdminLock.Lock();
            /* Checking if mLifecycleManagerRemoteObject is not valid then create the object */
            if (nullptr == mLifecycleManagerRemoteObject)
            {
                LOGINFO("Create LifecycleManager Remote store object");
                if (Core::ERROR_NONE != createLifecycleManagerRemoteObject())
                {
                    LOGERR("Failed to create LifecycleInterfaceConnector");
                }
            }
            ASSERT (nullptr != mLifecycleManagerRemoteObject);

            if (nullptr != mLifecycleManagerRemoteObject)
            {
                status = mLifecycleManagerRemoteObject->IsAppLoaded(appId, loaded);
            }
            mAdminLock.Unlock();
            return status;
        }


/*
 * @brief LaunchApp invokes this to call LifecycleManager API.
 * @Params  : const string& appId , const string& intent , const string& launchArgs
 * @return  : Core::hresult
 */
        Core::hresult LifecycleInterfaceConnector::launch(const string& appId, const string& intent, const string& launchArgs, WPEFramework::Exchange::RuntimeConfig& runtimeConfigObject)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            AppManagerImplementation*appManagerImplInstance = AppManagerImplementation::getInstance();
            bool loaded = false;
            string appPath = "";
            string appConfig = "";
            string runtimeAppId = "";
            string runtimePath = "";
            string runtimeConfig = "";
            string environmentVars = "";
            bool enableDebugger = false;
            string appInstanceId = "";
            string errorReason = "";
            bool success = true;
            Exchange::ILifecycleManager::LifecycleState state = Exchange::ILifecycleManager::LifecycleState::UNLOADED;
            if (appId.empty())
            {
                LOGERR("appId is empty");
            }
            else
            {
                mAdminLock.Lock();
                if (nullptr != appManagerImplInstance)
                {
                    AppManagerImplementation::PackageInfo packageData;
                    if(appManagerImplInstance->fetchPackageInfoByAppId(appId, packageData))
                    {
                        appPath = packageData.unpackedPath;
                        LOGINFO("Got PackageAppInfo appPath :[%s]", appPath.c_str());
                    }
                    else
                    {
                        LOGERR("Couldn't get the package details for appId: %s", appId.c_str());
                    }
                }
                /* Checking if mLifecycleManagerRemoteObject is not valid then create the object */
                if (nullptr == mLifecycleManagerRemoteObject)
                {
                    LOGINFO("Create LifecycleManager Remote store object");
                    if (Core::ERROR_NONE != createLifecycleManagerRemoteObject())
                    {
                        LOGERR("Failed to create LifecycleInterfaceConnector");
                    }
                }
                ASSERT (nullptr != mLifecycleManagerRemoteObject);

                if (nullptr != mLifecycleManagerRemoteObject)
                {
                    status = isAppLoaded(appId, loaded);
                    if (appManagerImplInstance != nullptr)
                    {
                        auto it = appManagerImplInstance->mAppInfo.find(appId);
                        if ((loaded == true) &&
                            (Core::ERROR_NONE == status) &&
                            (it != appManagerImplInstance->mAppInfo.end()) &&
                            (it->second.appNewState == Exchange::IAppManager::AppLifecycleState::APP_STATE_SUSPENDED))
                        {
                            appManagerImplInstance->mCurrentAction = AppManagerImplementation::APP_ACTION_RESUME;
                            state = Exchange::ILifecycleManager::LifecycleState::ACTIVE;
                            LOGINFO("launchApp appInstanceId %s", it->second.appInstanceId.c_str());
                            status = mLifecycleManagerRemoteObject->SetTargetAppState(it->second.appInstanceId, state, intent);

                            if (status == Core::ERROR_NONE)
                            {
                                LOGINFO("Update App Info");
                                it->second.targetAppState = Exchange::IAppManager::AppLifecycleState::APP_STATE_ACTIVE;
                                it->second.appIntent = intent;
                            }
                            else
                            {
                                LOGERR("SetTargetAppState Failed");
                            }
                        }
                        else
                        {
                            if (fileExists(SUSPEND_POLICY_FILE) == true)
                            {
                                appManagerImplInstance->mCurrentAction = AppManagerImplementation::APP_ACTION_SUSPEND;
                                state = Exchange::ILifecycleManager::LifecycleState::SUSPENDED;
                            }
                            else
                            {
                                appManagerImplInstance->mCurrentAction = AppManagerImplementation::APP_ACTION_LAUNCH;
                                state = Exchange::ILifecycleManager::LifecycleState::ACTIVE;
                            }
                            LOGINFO("spawnApp called ,state %u",state);
                            status = mLifecycleManagerRemoteObject->SpawnApp(appId, appPath, appConfig, runtimeAppId, runtimePath, runtimeConfig, intent, environmentVars,
                                                                          enableDebugger, state, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success);

                            if (status == Core::ERROR_NONE)
                            {
                                LOGINFO("Update App Info");
                                it->second.appInstanceId   = std::move(appInstanceId);
                                it->second.appIntent       = intent;
                                it->second.packageInfo.type = AppManagerImplementation::APPLICATION_TYPE_INTERACTIVE;
                                it->second.targetAppState  =    (state == Exchange::ILifecycleManager::LifecycleState::SUSPENDED)
                                                                                                                                           ? Exchange::IAppManager::AppLifecycleState::APP_STATE_SUSPENDED
                                                                                                                                           : Exchange::IAppManager::AppLifecycleState::APP_STATE_ACTIVE;
                            }
                            else
                            {
                                LOGERR("spawnApp failed");
                            }
                        }
                    }
                }
                mAdminLock.Unlock();
            }
            return status;
        }

        /* PreloadApp invokes it */
        Core::hresult LifecycleInterfaceConnector::preLoadApp(const string& appId, const string& launchArgs, WPEFramework::Exchange::RuntimeConfig& runtimeConfigObject, string& error)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            AppManagerImplementation *appManagerImplInstance = AppManagerImplementation::getInstance();

            string appPath = "";
            string intent = "";
            string appConfig = "";
            string runtimeAppId = "";
            string runtimePath = "";
            string runtimeConfig = "";
            string environmentVars = "";
            bool enableDebugger = false;
            string appInstanceId = "";
            bool success = true;
            Exchange::ILifecycleManager::LifecycleState state = Exchange::ILifecycleManager::LifecycleState::UNLOADED;
            if (appId.empty())
            {
                LOGERR("appId is empty");
            }
            else
            {
                mAdminLock.Lock();
                if (nullptr != appManagerImplInstance)
                {
                    AppManagerImplementation::PackageInfo packageData;
                    if(appManagerImplInstance->fetchPackageInfoByAppId(appId, packageData))
                    {
                        appPath = packageData.unpackedPath;
                        LOGINFO("Got PackageAppInfo appPath :[%s]", appPath.c_str());
                    }
                    else
                    {
                        LOGERR("Couldn't get the package details for appId: %s", appId.c_str());
                    }
                }
                /* Checking if mLifecycleManagerRemoteObject is not valid then create the object */
                if (nullptr == mLifecycleManagerRemoteObject)
                {
                    LOGINFO("Create LifecycleManager Remote store object");
                    if (Core::ERROR_NONE != createLifecycleManagerRemoteObject())
                    {
                        LOGERR("Failed to create LifecycleInterfaceConnector");
                    }
                }
                ASSERT (nullptr != mLifecycleManagerRemoteObject);
                if (nullptr != mLifecycleManagerRemoteObject)
                {
                    if (fileExists(SUSPEND_POLICY_FILE) == true)
                    {
                        appManagerImplInstance->mCurrentAction = AppManagerImplementation::APP_ACTION_SUSPEND;
                        state = Exchange::ILifecycleManager::LifecycleState::SUSPENDED;
                    }
                    else
                    {
                        appManagerImplInstance->mCurrentAction = AppManagerImplementation::APP_ACTION_PRELOAD;
                        state = Exchange::ILifecycleManager::LifecycleState::PAUSED;
                    }
                    status = mLifecycleManagerRemoteObject->SpawnApp(appId, appPath, appConfig, runtimeAppId, runtimePath, runtimeConfig, intent, environmentVars,
                          enableDebugger, state, runtimeConfigObject, launchArgs, appInstanceId, error, success);
                    if (status == Core::ERROR_NONE)
                    {
                        LOGINFO("Update App Info");

                        /*Insert/update loaded app info*/
                        if (nullptr != appManagerImplInstance)
                        {
                            appManagerImplInstance->mAppInfo[appId].appInstanceId   = std::move(appInstanceId);
                            appManagerImplInstance->mAppInfo[appId].packageInfo.type = AppManagerImplementation::APPLICATION_TYPE_INTERACTIVE;
                            appManagerImplInstance->mAppInfo[appId].targetAppState  =    (state == Exchange::ILifecycleManager::LifecycleState::SUSPENDED)
                                                                                                                                       ? Exchange::IAppManager::AppLifecycleState::APP_STATE_SUSPENDED
                                                                                                                                       : Exchange::IAppManager::AppLifecycleState::APP_STATE_PAUSED;
                        }
                    }
                    else
                    {
                        LOGERR("PreLoad failed");
                    }
                }
                mAdminLock.Unlock();
            }
            return status;
        }

        /* Close App invokes it */
        Core::hresult LifecycleInterfaceConnector::closeApp(const string& appId)
        {
            uint32_t status = Core::ERROR_GENERAL;
            std::string appInstanceId = "";
            std::string appIntent = "";
            Exchange::IAppManager::AppLifecycleState targetLifecycleState = Exchange::IAppManager::AppLifecycleState::APP_STATE_UNKNOWN;
            AppManagerImplementation* appManagerImplInstance = AppManagerImplementation::getInstance();

            /* Retrieve the appId from the parameters object */
            LOGINFO("AppId retrieved: %s", appId.c_str());

            mAdminLock.Lock();

            if (nullptr != appManagerImplInstance)
            {
                for ( std::map<std::string, AppManagerImplementation::AppInfo>::iterator appIterator = appManagerImplInstance->mAppInfo.begin(); appIterator != appManagerImplInstance->mAppInfo.end(); appIterator++)
                {
                    if (appIterator->first.compare(appId) == 0)
                    {
                        appInstanceId = appIterator->second.appInstanceId;
                        appIntent = appIterator->second.appIntent;
                        targetLifecycleState = appIterator->second.targetAppState;

                        /* Check targetLifecycleState is in ACTIVE state */
                        if(targetLifecycleState == Exchange::IAppManager::AppLifecycleState::APP_STATE_ACTIVE)
                        {
                            if (nullptr != mLifecycleManagerRemoteObject)
                            {
                                appManagerImplInstance->mCurrentAction = AppManagerImplementation::APP_ACTION_CLOSE;
                                /* Call setTargetAppState from Adapter file once ready */
                                 status = mLifecycleManagerRemoteObject->SetTargetAppState(appInstanceId, Exchange::ILifecycleManager::LifecycleState::SUSPENDED, appIntent);
                                if(status == Core::ERROR_NONE)
                                {
                                    appIterator->second.targetAppState = Exchange::IAppManager::AppLifecycleState::APP_STATE_SUSPENDED;
                                }
                                else
                                {
                                    LOGERR("Fail to closeApp AppId: %s status:%u", appId.c_str(), status);
                                }
                            }
                        }
                        break;
                    }
                    else
                    {
                        LOGERR("appId not found");
                    }
                }
            }
            else
            {
                LOGERR("appManagerImplInstance is null");
            }
            mAdminLock.Unlock();
            return status;
        }

        /* Terminate App invokes it */
        Core::hresult LifecycleInterfaceConnector::terminateApp(const string& appId)
        {
            uint32_t status = Core::ERROR_GENERAL;
            std::string appInstanceId = "";
            Exchange::IAppManager::AppLifecycleState targetLifecycleState = Exchange::IAppManager::AppLifecycleState::APP_STATE_UNKNOWN;
            AppManagerImplementation* appManagerImplInstance = AppManagerImplementation::getInstance();
            bool success = false;
            std::string errorReason = "";
            bool foundAppId = false;

            /* Retrieve the appId from the parameters object */
            LOGINFO("AppId retrieved: %s", appId.c_str());

            mAdminLock.Lock();
            if (nullptr != appManagerImplInstance)
            {
                for ( std::map<std::string, AppManagerImplementation::AppInfo>::iterator appIterator = appManagerImplInstance->mAppInfo.begin(); appIterator != appManagerImplInstance->mAppInfo.end(); appIterator++)
                {
                    if (appIterator->first.compare(appId) == 0)
                    {
                        foundAppId = true;
                        appInstanceId = appIterator->second.appInstanceId;
                        targetLifecycleState = appIterator->second.appNewState;

                        /* Check targetLifecycleState is in RUNNING state */
                        if(targetLifecycleState == Exchange::IAppManager::AppLifecycleState::APP_STATE_PAUSED)
                        {
                            if (nullptr != mLifecycleManagerRemoteObject)
                            {
                                appManagerImplInstance->mCurrentAction = AppManagerImplementation::APP_ACTION_TERMINATE;
                                status = mLifecycleManagerRemoteObject->UnloadApp(appInstanceId, errorReason, success);
                                if (status != Core::ERROR_NONE)
                                {
                                    if (!errorReason.empty())
                                    {
                                        LOGERR("UnloadApp failed with error reason: %s", errorReason.c_str());
                                    }
                                    else
                                    {
                                        LOGERR("UnloadApp failed with an unknown error.");
                                    }
                                }
                            }
                        }
                        break;
                    }
                }
                if (!foundAppId)
                {
                    LOGERR("AppId %s not found in database", appId.c_str());
                }
            }
            else
            {
                LOGERR("appManagerImplInstance is null");
            }
            mAdminLock.Unlock();
            return status;
        }

        /* Kill App invokes it */
        Core::hresult LifecycleInterfaceConnector::killApp(const string& appId)
        {
            LOGINFO("killApp entered");
            Core::hresult result = Core::ERROR_GENERAL;
            AppManagerImplementation* appManagerImplInstance = AppManagerImplementation::getInstance();

            if (appId.empty())
            {
                LOGERR("appId is empty");
                return result;
            }

            if (nullptr == mLifecycleManagerRemoteObject)
            {
                LOGINFO("Create LifecycleManager Remote store object");
                if (Core::ERROR_NONE != createLifecycleManagerRemoteObject())
                {
                    LOGERR("Failed to create LifecycleInterfaceConnector");
                }
            }
            ASSERT (nullptr != mLifecycleManagerRemoteObject);
            if (nullptr != mLifecycleManagerRemoteObject)
            {
                mAdminLock.Lock();
                string appInstanceId = GetAppInstanceId(appId);
                if (appInstanceId.empty())
                {
                    LOGERR("appInstanceId not found for appId '%s'", appId.c_str());
                }
                else
                {
                    string errorReason{};
                    bool success{};

                    appManagerImplInstance->mCurrentAction = AppManagerImplementation::APP_ACTION_KILL;
                    result = mLifecycleManagerRemoteObject->KillApp(appInstanceId, errorReason, success);

                    if (!(result == Core::ERROR_NONE && success))
                    {
                        LOGERR("killApp failed, result: %d, success: %d, errorReason: %s", result, success, errorReason.c_str());
                    }
                }
                mAdminLock.Unlock();
            }

            return result;
        }

        /* Send Intent invokes it */
        Core::hresult LifecycleInterfaceConnector::sendIntent(const string& appId, const string& intent)
        {
            LOGINFO("sendIntent Entered");
            Core::hresult result = Core::ERROR_GENERAL;

            if (appId.empty())
            {
                LOGERR("appId is empty");
                return result;
            }

            if (nullptr == mLifecycleManagerRemoteObject)
            {
                LOGINFO("Create LifecycleManager Remote store object");
                if (Core::ERROR_NONE != createLifecycleManagerRemoteObject())
                {
                    LOGERR("Failed to create LifecycleInterfaceConnector");
                }
            }

            if (nullptr != mLifecycleManagerRemoteObject)
            {
                mAdminLock.Lock();
                string appInstanceId = GetAppInstanceId(appId);
                if (appInstanceId.empty())
                {
                    LOGERR("appInstanceId not found for appId '%s'", appId.c_str());
                }
                else
                {
                    string errorReason{};
                    bool success{};

                    result = mLifecycleManagerRemoteObject->SendIntentToActiveApp(appInstanceId, intent, errorReason, success);

                    if (!(result == Core::ERROR_NONE && success))
                    {
                        LOGERR("sentIntent failed, result: %d, success: %d, errorReason: %s", result, success, errorReason.c_str());
                    }
                }
                mAdminLock.Unlock();
            }

            return result;
        }

        /* GetLoadedApps invokes it */
        Core::hresult LifecycleInterfaceConnector::getLoadedApps(string& apps)
        {
            LOGINFO("getLoadedApps Entered");
            Core::hresult result = Core::ERROR_GENERAL;
            AppManagerImplementation *appManagerImplInstance = AppManagerImplementation::getInstance();

            mAdminLock.Lock();
            /* Checking if mLifecycleManagerRemoteObject is not valid then create the object */
            if (nullptr == mLifecycleManagerRemoteObject)
            {
                LOGINFO("Create LifecycleManager Remote store object");
                if (Core::ERROR_NONE != createLifecycleManagerRemoteObject())
                {
                    LOGERR("Failed to create LifecycleInterfaceConnector");
                }
            }

            ASSERT (nullptr != mLifecycleManagerRemoteObject);
            if (nullptr != mLifecycleManagerRemoteObject)
            {
                JsonArray loadedAppsArray;
                if (nullptr != appManagerImplInstance)
                {
                    for (const auto& appEntry :appManagerImplInstance->mAppInfo)
                    {
                        JsonObject loadedAppObject;

                        loadedAppObject["appId"] = appEntry.first;
                        loadedAppObject["lifecycleState"] = static_cast<int>(appEntry.second.appNewState);
                        loadedAppObject["type"] = static_cast<int>(appEntry.second.packageInfo.type);
                        loadedAppObject["targetLifecycleState"] = static_cast<int>(appEntry.second.targetAppState);
                        loadedAppObject["activeSessionId"] = appEntry.second.activeSessionId;
                        loadedAppObject["appInstanceId"] = appEntry.second.appInstanceId;

                        loadedAppsArray.Add(loadedAppObject);
                    }
                    loadedAppsArray.ToString(apps);
                    LOGINFO("getLoadedApps: %s", apps.c_str());
                    result = Core::ERROR_NONE;
                }
                else
                {
                    LOGERR("appManagerImplementation instance is nullptr");
                }
            }
            mAdminLock.Unlock();
            return result;
        }

        Exchange::IAppManager::AppLifecycleState WPEFramework::Plugin::LifecycleInterfaceConnector::mapAppLifecycleState(Exchange::ILifecycleManager::LifecycleState state)
        {
            Exchange::IAppManager::AppLifecycleState result = Exchange::IAppManager::AppLifecycleState::APP_STATE_UNKNOWN;
            switch(state)
            {
                case Exchange::ILifecycleManager::LifecycleState::UNLOADED:
                    LOGINFO("UNLOADED state %u", state);
                    result = Exchange::IAppManager::AppLifecycleState::APP_STATE_UNLOADED;
                    break;
                case Exchange::ILifecycleManager::LifecycleState::LOADING:
                    LOGINFO("LOADING state %u", state);
                    result = Exchange::IAppManager::AppLifecycleState::APP_STATE_LOADING;
                    break;
                case Exchange::ILifecycleManager::LifecycleState::ACTIVE:
                    LOGINFO("ACTIVE state %u", state);
                    result = Exchange::IAppManager::AppLifecycleState::APP_STATE_ACTIVE;
                    break;
                case Exchange::ILifecycleManager::LifecycleState::PAUSED:
                    LOGINFO("PAUSED state %u", state);
                    result = Exchange::IAppManager::AppLifecycleState::APP_STATE_PAUSED;
                    break;
                case Exchange::ILifecycleManager::LifecycleState::INITIALIZING:
                    LOGINFO("INITIALIZING state %u", state);
                    result = Exchange::IAppManager::AppLifecycleState::APP_STATE_INITIALIZING;
                    break;
                case Exchange::ILifecycleManager::LifecycleState::SUSPENDED:
                    LOGINFO("SUSPENDED state %u", state);
                    result = Exchange::IAppManager::AppLifecycleState::APP_STATE_SUSPENDED;
                    break;
                case Exchange::ILifecycleManager::LifecycleState::TERMINATING:
                    LOGINFO("TERMINATED state %u", state);
                    result = Exchange::IAppManager::AppLifecycleState::APP_STATE_TERMINATED;
                    break;
                case Exchange::ILifecycleManager::LifecycleState::HIBERNATED:
                    LOGINFO("HIBERNATED state %u", state);
                    result = Exchange::IAppManager::AppLifecycleState::APP_STATE_HIBERNATED;
                    break;
                default:
                    LOGWARN("Unknown state %u", state);
                    result = Exchange::IAppManager::AppLifecycleState::APP_STATE_UNKNOWN;
                    break;
            }
            return result;
        }

        void LifecycleInterfaceConnector::OnAppLifecycleStateChanged(const string& appId, const string& appInstanceId, const Exchange::ILifecycleManager::LifecycleState oldState, const Exchange::ILifecycleManager::LifecycleState newState, const string& navigationIntent)
        {
            AppManagerImplementation*appManagerImplInstance = AppManagerImplementation::getInstance();
            Exchange::IAppManager::AppLifecycleState newAppState = mapAppLifecycleState(newState);
            Exchange::IAppManager::AppLifecycleState oldAppState = mapAppLifecycleState(oldState);
            bool shouldNotify = false;
            LOGINFO("OnAppLifecycleStateChanged event triggered ***\n");

            if (newAppState == Exchange::IAppManager::APP_STATE_UNKNOWN ||
                oldAppState == Exchange::IAppManager::APP_STATE_UNKNOWN)
            {
                LOGINFO("Skipping notification: new or old app state is UNKNOWN");
                return;
            }

            mAdminLock.Lock();
            if(nullptr != appManagerImplInstance)
            {
                for( std::map<std::string, AppManagerImplementation::AppInfo>::iterator it = appManagerImplInstance->mAppInfo.begin(); it != appManagerImplInstance->mAppInfo.end(); it++)
                {
                    if((it->first.compare(appId) == 0) && (it->second.appInstanceId.compare(appInstanceId) == 0))
                    {
                        it->second.appOldState = oldAppState;
                        it->second.appNewState = newAppState;
                        it->second.appLifecycleState = newState;
                        it->second.appIntent = navigationIntent;

                        if (oldState == Exchange::ILifecycleManager::LifecycleState::ACTIVE ||
                            newState == Exchange::ILifecycleManager::LifecycleState::ACTIVE)
                        {
                            struct timespec stateChangeTime;
                            if (timespec_get(&stateChangeTime, TIME_UTC) != 0)
                            {
                                it->second.lastActiveStateChangeTime = stateChangeTime;
                            }
                            else
                            {
                                LOGERR("Unable to update the active state change time for appInstanceId: %s", appInstanceId.c_str());
                            }
                        }

                        if (newState == Exchange::ILifecycleManager::LifecycleState::ACTIVE)
                        {
                            gAppsActiveCounter++;
                            it->second.lastActiveIndex = gAppsActiveCounter;
                        }
                        break;
                    }
                }

                shouldNotify = ((newAppState == Exchange::IAppManager::AppLifecycleState::APP_STATE_LOADING) ||
                                       (newAppState == Exchange::IAppManager::AppLifecycleState::APP_STATE_ACTIVE) ||
                                       ((newAppState == Exchange::IAppManager::AppLifecycleState::APP_STATE_PAUSED) &&
                                         (appManagerImplInstance->mCurrentAction == AppManagerImplementation::APP_ACTION_CLOSE)));

                LOGINFO("shouldNotify %d for state %u",shouldNotify, newAppState);

                if(shouldNotify)
                {
                    appManagerImplInstance->handleOnAppLifecycleStateChanged(appId, appInstanceId, newAppState, oldAppState, Exchange::IAppManager::AppErrorReason::APP_ERROR_NONE);
                }
            }
            mAdminLock.Unlock();
        }

        /* Returns appInstanceId for appId. Does not synchronize access to LoadedAppInfo.
         * Caller should take care about the synchronization.
         */
        string LifecycleInterfaceConnector::GetAppInstanceId(const string& appId) const
        {
            AppManagerImplementation* appManagerImpl = AppManagerImplementation::getInstance();
            if (!appManagerImpl)
                return {};

            auto it = appManagerImpl->mAppInfo.find(appId);
            if (it == appManagerImpl->mAppInfo.end())
                return {};
            else
                return it->second.appInstanceId;
        }

        void LifecycleInterfaceConnector::RemoveApp(const string& appId)
        {
            AppManagerImplementation* appManagerImpl = AppManagerImplementation::getInstance();
            if (!appManagerImpl)
                return;

            auto it = appManagerImpl->mAppInfo.find(appId);
            if (it != appManagerImpl->mAppInfo.end())
                appManagerImpl->mAppInfo.erase(it);
            else
                LOGERR("AppInfo for appId '%s' not found", appId.c_str());
        }

        bool LifecycleInterfaceConnector::fileExists(const char* pFileName)
        {
            bool isRegular = false;

            if (pFileName != nullptr)
            {
                struct stat fileStat;
                if (stat(pFileName, &fileStat) == 0)
                {
                    if (S_ISREG(fileStat.st_mode))
                    {
                        isRegular = true;
                        LOGINFO("File: '%s' exists", pFileName);
                    }
                    else
                    {
                        LOGINFO("File: '%s' exists but is not a regular file", pFileName);
                    }
                }
                else
                {
                    LOGINFO("File: Failed to stat file '%s'", pFileName);
                }
            }
           else
            {
                LOGINFO("Filename pointer is null");
            }

            return isRegular;
        }
     } /* namespace Plugin */
} /* namespace WPEFramework */
