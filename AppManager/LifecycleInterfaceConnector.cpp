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

using namespace std;
using namespace Utils;

namespace WPEFramework
{
    namespace Plugin
    {
        class NotificationHandler : public Exchange::ILifecycleManager::INotification {

            public:
                NotificationHandler(LifecycleInterfaceConnector& parent) : mParent(parent){}
                ~NotificationHandler(){}

                void OnAppStateChanged(const string& appId, Exchange::ILifecycleManager::LifecycleState state, const string& errorReason)
                {
                    mParent.OnAppStateChanged(appId, state, errorReason);
                }

                BEGIN_INTERFACE_MAP(NotificationHandler)
                INTERFACE_ENTRY(Exchange::ILifecycleManager::INotification)
                END_INTERFACE_MAP

            private:
                LifecycleInterfaceConnector& mParent;
        };

        LifecycleInterfaceConnector* LifecycleInterfaceConnector::_instance = nullptr;

        LifecycleInterfaceConnector::LifecycleInterfaceConnector(PluginHost::IShell* service)
		: mLifecycleManagerRemoteObject(nullptr),
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
            Core::Sink<NotificationHandler> notification(*this);

            if (nullptr == mCurrentservice)
            {
                LOGWARN("mCurrentservice is null \n");
            }
            else if (nullptr == (mLifecycleManagerRemoteObject = mCurrentservice->QueryInterfaceByCallsign<WPEFramework::Exchange::ILifecycleManager>("org.rdk.LifecycleManager")))
            {
                LOGWARN("Failed to create LifecycleManager Remote object\n");
            }
            else
            {
                LOGINFO("Created LifecycleManager Remote Object \n");
                mLifecycleManagerRemoteObject->AddRef();
                mLifecycleManagerRemoteObject->Register(&notification);
                LOGINFO("LifecycleManager notification registered");
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
        }

/*
 * @brief LaunchApp invokes this to call LifecycleManager API.
 * @Params  : const string& appId , const string& intent , const string& launchArgs
 * @return  : Core::hresult
 */
        Core::hresult LifecycleInterfaceConnector::launch(const string& appId, const string& intent, const string& launchArgs)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            AppManagerImplementation::LoadedAppInfo loadedAppInfo;
            AppManagerImplementation*appManagerImplInstance = AppManagerImplementation::getInstance();

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

            if (appId.empty())
            {
                LOGERR("appId is empty");
            }
            else
            {
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
                    status = mLifecycleManagerRemoteObject->SpawnApp(appId, appPath, appConfig, runtimeAppId, runtimePath, runtimeConfig, intent, environmentVars,
                                                                  enableDebugger, Exchange::ILifecycleManager::LifecycleState::ACTIVE, launchArgs, appInstanceId, errorReason, success);

                    if (status == Core::ERROR_NONE)
                    {
                        LOGINFO("Update App Info");
                        loadedAppInfo.appInstanceId = std::move(appInstanceId);
                        loadedAppInfo.type = AppManagerImplementation::APPLICATION_TYPE_INTERACTIVE;
                        loadedAppInfo.appIntent = intent;
                        loadedAppInfo.currentAppState = Exchange::IAppManager::AppLifecycleState::APP_STATE_UNKNOWN;
                        loadedAppInfo.currentAppLifecycleState = Exchange::ILifecycleManager::LOADING;
                        loadedAppInfo.targetAppState = Exchange::IAppManager::AppLifecycleState::APP_STATE_ACTIVE;

                        /*Insert/update loaded app info*/
                        if (nullptr != appManagerImplInstance)
                        {
                            appManagerImplInstance->mLoadedAppInfo[appId] = std::move(loadedAppInfo);
                        }
                    }
                    else
                    {
                        LOGERR("App launch failed");
                    }
                }
                mAdminLock.Unlock();
            }
            return status;
        }

        /* PreloadApp invokes it */
        Core::hresult LifecycleInterfaceConnector::preLoadApp(const string& appId, const string& launchArgs, string& error)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            AppManagerImplementation::LoadedAppInfo loadedAppInfo;
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

            if (appId.empty())
            {
                LOGERR("appId is empty");
            }
            else
            {
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
                    status = mLifecycleManagerRemoteObject->SpawnApp(appId, appPath, appConfig, runtimeAppId, runtimePath, runtimeConfig, intent, environmentVars,
                          enableDebugger, Exchange::ILifecycleManager::LifecycleState::RUNNING, launchArgs, appInstanceId, error, success);
                    if (status == Core::ERROR_NONE)
                    {
                        LOGINFO("Update App Info");
                        loadedAppInfo.appInstanceId = std::move(appInstanceId);
                        loadedAppInfo.type = AppManagerImplementation::APPLICATION_TYPE_INTERACTIVE;
                        loadedAppInfo.currentAppState = Exchange::IAppManager::AppLifecycleState::APP_STATE_UNKNOWN;
                        loadedAppInfo.currentAppLifecycleState = Exchange::ILifecycleManager::LOADING;
                        loadedAppInfo.targetAppState = Exchange::IAppManager::AppLifecycleState::APP_STATE_RUNNING;

                        /*Insert/update loaded app info*/
                        if (nullptr != appManagerImplInstance)
                        {
                            appManagerImplInstance->mLoadedAppInfo[appId] = std::move(loadedAppInfo);
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
                for ( std::map<std::string, AppManagerImplementation::LoadedAppInfo>::iterator appIterator = appManagerImplInstance->mLoadedAppInfo.begin(); appIterator != appManagerImplInstance->mLoadedAppInfo.end(); appIterator++)
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
                for ( std::map<std::string, AppManagerImplementation::LoadedAppInfo>::iterator appIterator = appManagerImplInstance->mLoadedAppInfo.begin(); appIterator != appManagerImplInstance->mLoadedAppInfo.end(); appIterator++)
                {
                    if (appIterator->first.compare(appId) == 0)
                    {
                        foundAppId = true;
                        appInstanceId = appIterator->second.appInstanceId;
                        targetLifecycleState = appIterator->second.currentAppState;

                        /* Check targetLifecycleState is in RUNNING state */
                        if(targetLifecycleState == Exchange::IAppManager::AppLifecycleState::APP_STATE_RUNNING)
                        {
                            if (nullptr != mLifecycleManagerRemoteObject)
                            {
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
                                else
                                {
                                    /* Remove the appId from the database */
                                    appManagerImplInstance->mLoadedAppInfo.erase(appIterator);
                                    LOGINFO("AppId %s and its corresponding data removed from database.", appId.c_str());
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

                    result = mLifecycleManagerRemoteObject->KillApp(appInstanceId, errorReason, success);

                    if (!(result == Core::ERROR_NONE && success))
                    {
                        LOGERR("killApp failed, result: %d, success: %d, errorReason: %s", result, success, errorReason.c_str());
                    }
                    else // killApp succedded
                    {
                        RemoveApp(appId);
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
                    for (const auto& appEntry :appManagerImplInstance->mLoadedAppInfo)
                    {
                        JsonObject loadedAppObject;

                        loadedAppObject["appId"] = appEntry.first;
                        loadedAppObject["type"] = static_cast<int>(appEntry.second.type);
                        loadedAppObject["lifecycleState"] = static_cast<int>(appEntry.second.currentAppState);
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

        void LifecycleInterfaceConnector::OnAppStateChanged(const string& appId, Exchange::ILifecycleManager::LifecycleState state, const string& errorReason)
        {
            string appInstanceId = "";
            Exchange::IAppManager::AppLifecycleState oldState = Exchange::IAppManager::AppLifecycleState::APP_STATE_UNKNOWN;
            Exchange::IAppManager::AppLifecycleState newState = Exchange::IAppManager::AppLifecycleState::APP_STATE_UNKNOWN;
            AppManagerImplementation*appManagerImplInstance = AppManagerImplementation::getInstance();

            LOGINFO("onAppLifecycleStateChanged event triggered ***\n");

            switch(state)
            {
                case Exchange::ILifecycleManager::LifecycleState::INITIALIZING:
                case Exchange::ILifecycleManager::LifecycleState::TERMINATEREQUESTED:
                case Exchange::ILifecycleManager::LifecycleState::TERMINATING:
                case Exchange::ILifecycleManager::LifecycleState::RUNNING:
                    newState = Exchange::IAppManager::AppLifecycleState::APP_STATE_RUNNING;
                    break;

                case Exchange::ILifecycleManager::LifecycleState::ACTIVATEREQUESTED:
                case Exchange::ILifecycleManager::LifecycleState::ACTIVE:
                    newState = Exchange::IAppManager::AppLifecycleState::APP_STATE_ACTIVE;
                    break;

                case Exchange::ILifecycleManager::LifecycleState::SUSPENDREQUESTED:
                case Exchange::ILifecycleManager::LifecycleState::SUSPENDED:
                    newState = Exchange::IAppManager::AppLifecycleState::APP_STATE_SUSPENDED;

                default:
                    LOGWARN("Unknown state %u", state);
                    break;
            }

            mAdminLock.Lock();
            if (nullptr != appManagerImplInstance)
            {
                for ( std::map<std::string, AppManagerImplementation::LoadedAppInfo>::iterator it = appManagerImplInstance->mLoadedAppInfo.begin(); it != appManagerImplInstance->mLoadedAppInfo.end(); it++)
                {
                    if (it->first.compare(appId) == 0)
                    {
                        appInstanceId = it->second.appInstanceId;
                        oldState = it->second.currentAppState;
                        it->second.currentAppLifecycleState = state;
                        if (oldState != newState)
                        {
                            it->second.currentAppState = newState;
                        }
                        break;
                    }
                }
            }
            /*TODO: AppManager event notification to be handled in upcoming phase*/
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

            auto it = appManagerImpl->mLoadedAppInfo.find(appId);
            if (it == appManagerImpl->mLoadedAppInfo.end())
                return {};
            else
                return it->second.appInstanceId;
        }

        void LifecycleInterfaceConnector::RemoveApp(const string& appId)
        {
            AppManagerImplementation* appManagerImpl = AppManagerImplementation::getInstance();
            if (!appManagerImpl)
                return;

            auto it = appManagerImpl->mLoadedAppInfo.find(appId);
            if (it != appManagerImpl->mLoadedAppInfo.end())
                appManagerImpl->mLoadedAppInfo.erase(it);
            else
                LOGERR("AppInfo for appId '%s' not found", appId.c_str());
        }      

     } /* namespace Plugin */
} /* namespace WPEFramework */
