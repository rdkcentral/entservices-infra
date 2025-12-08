/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2025 RDK Management
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

#include "AppManagerTelemetryReporting.h"
#include "UtilsLogging.h"
#include "tracing/Logging.h"

namespace WPEFramework
{
namespace Plugin
{
    AppManagerTelemetryReporting::AppManagerTelemetryReporting(): mTelemetryPluginObject(nullptr), mCurrentservice(nullptr)
    {
    }

    AppManagerTelemetryReporting::~AppManagerTelemetryReporting()
    {
    }

    AppManagerTelemetryReporting& AppManagerTelemetryReporting::getInstance()
    {
        LOGINFO("Get AppManagerTelemetryReporting Instance");
        static AppManagerTelemetryReporting instance;
        return instance;
    }

    void AppManagerTelemetryReporting::initialize(PluginHost::IShell* service)
    {
        ASSERT(nullptr != service);
        mAdminLock.Lock();
        mCurrentservice = service;
        mAdminLock.Unlock();
        if(Core::ERROR_NONE != createTelemetryPluginObject())
        {
            LOGERR("Failed to create TelemetryObject\n");
        }
    }

    time_t AppManagerTelemetryReporting::getCurrentTimestamp()
    {
        timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return ((time_t)(ts.tv_sec * 1000) + ((time_t)ts.tv_nsec/1000000));
    }

    Core::hresult AppManagerTelemetryReporting::createTelemetryPluginObject()
    {
        Core::hresult status = Core::ERROR_GENERAL;

        mAdminLock.Lock();
        if (nullptr == mCurrentservice)
        {
                LOGERR("mCurrentservice is null \n");
        }
        else if (nullptr == (mTelemetryPluginObject = mCurrentservice->QueryInterfaceByCallsign<WPEFramework::Exchange::ITelemetry>("org.rdk.Telemetry")))
        {
                LOGERR("Failed to create TelemetryObject\n");
        }
        else
        {
            status = Core::ERROR_NONE;
            LOGINFO("created Telemetry Object");
        }
        mAdminLock.Unlock();
        return status;
    }

    void AppManagerTelemetryReporting::reportTelemetryData(const std::string& appId, AppManagerImplementation::CurrentAction currentAction)
    {
        JsonObject jsonParam;
        std::string telemetryMetrics = "";
        std::string markerName = "";
        std::string appMarker = "";
        time_t currentTime = getCurrentTimestamp();
        AppManagerImplementation*appManagerImplInstance = AppManagerImplementation::getInstance();

        if(nullptr == mTelemetryPluginObject) /*mTelemetryPluginObject is null retry to create*/
        {
            if(Core::ERROR_NONE != createTelemetryPluginObject())
            {
                LOGERR("Failed to create TelemetryObject\n");
            }
        }

        auto it = appManagerImplInstance->mAppInfo.find(appId);
        if((it != appManagerImplInstance->mAppInfo.end()) && (currentAction == it->second.currentAction) && (nullptr != mTelemetryPluginObject))
        {
            LOGINFO("Received data for appId %s current action %d ",appId.c_str(), currentAction);

            switch(currentAction)
            {
                case AppManagerImplementation::APP_ACTION_LAUNCH:
                case AppManagerImplementation::APP_ACTION_PRELOAD:
                    jsonParam["appManagerLaunchTime"] = (int)(currentTime - it->second.currentActionTime);
                    markerName = TELEMETRY_MARKER_LAUNCH_TIME;
                break;
                case AppManagerImplementation::APP_ACTION_CLOSE:
                    if ((Exchange::IAppManager::AppLifecycleState::APP_STATE_SUSPENDED != it->second.targetAppState) &&
                        (Exchange::IAppManager::AppLifecycleState::APP_STATE_HIBERNATED != it->second.targetAppState))
                    {
                        jsonParam["appManagerCloseTime"] = (int)(currentTime - it->second.currentActionTime);
                        markerName = TELEMETRY_MARKER_CLOSE_TIME;
                    }
                break;
                case AppManagerImplementation::APP_ACTION_TERMINATE:
                case AppManagerImplementation::APP_ACTION_KILL:
                    jsonParam["appManagerCloseTime"] = (int)(currentTime - it->second.currentActionTime);
                    markerName = TELEMETRY_MARKER_CLOSE_TIME;
                break;
                default:
                    LOGERR("currentAction is invalid");
                break;
            }

            if(!markerName.empty())
            {
                jsonParam["markerName"] = markerName;
                jsonParam.ToString(telemetryMetrics);
                if(!telemetryMetrics.empty())
                {
                    appMarker = appId + ":" + markerName;
                    mTelemetryPluginObject->Record(appMarker, telemetryMetrics);
                }
            }
        }
        else
        {
            LOGERR("Failed to report telemetry data as appId/currentAction or mTelemetryPluginObject is not valid");
        }
    }

    void AppManagerTelemetryReporting::reportTelemetryDataOnStateChange(const string& appId, const Exchange::ILifecycleManager::LifecycleState newState)
    {
        JsonObject jsonParam;
        std::string telemetryMetrics = "";
        std::string markerName = "";
        std::string appMarker = "";
        time_t currentTime = getCurrentTimestamp();
        AppManagerImplementation*appManagerImplInstance = AppManagerImplementation::getInstance();

        if(nullptr == mTelemetryPluginObject) /*mTelemetryPluginObject is null retry to create*/
        {
            if(Core::ERROR_NONE != createTelemetryPluginObject())
            {
                LOGERR("Failed to create TelemetryObject\n");
            }
        }

        auto it = appManagerImplInstance->mAppInfo.find(appId);
        if((it != appManagerImplInstance->mAppInfo.end()) && (nullptr != mTelemetryPluginObject))
        {
            switch(it->second.currentAction)
            {
                case AppManagerImplementation::APP_ACTION_LAUNCH:
                    if(Exchange::ILifecycleManager::LifecycleState::ACTIVE == newState)
                    {
                        jsonParam["totalLaunchTime"] = (int)(currentTime - it->second.currentActionTime);
                        jsonParam["launchType"] = ((AppManagerImplementation::APPLICATION_TYPE_INTERACTIVE == it->second.packageInfo.type)?"LAUNCH_INTERACTIVE":"START_SYSTEM");
                        jsonParam["markerFilters"] = TELEMETRY_MARKER_LAUNCH_TIME_FILTER;
                        markerName = TELEMETRY_MARKER_LAUNCH_TIME;
                    }
                break;
                case AppManagerImplementation::APP_ACTION_PRELOAD:
                    if(Exchange::ILifecycleManager::LifecycleState::PAUSED == newState)
                    {
                        jsonParam["totalLaunchTime"] = (int)(currentTime - it->second.currentActionTime);
                        jsonParam["launchType"] = ((AppManagerImplementation::APPLICATION_TYPE_INTERACTIVE == it->second.packageInfo.type)?"PRELOAD_INTERACTIVE":"START_SYSTEM");
                        jsonParam["markerFilters"] = TELEMETRY_MARKER_LAUNCH_TIME_FILTER;
                        markerName = TELEMETRY_MARKER_LAUNCH_TIME;
                    }
                break;
                case AppManagerImplementation::APP_ACTION_CLOSE:
                case AppManagerImplementation::APP_ACTION_TERMINATE:
                case AppManagerImplementation::APP_ACTION_KILL:
                        if(Exchange::ILifecycleManager::LifecycleState::UNLOADED == newState)
                        {
                            jsonParam["totalCloseTime"] = (int)(currentTime - it->second.currentActionTime);
                            jsonParam["closeType"] = ((AppManagerImplementation::APP_ACTION_CLOSE == it->second.currentAction)?"CLOSE":((AppManagerImplementation::APP_ACTION_TERMINATE == it->second.currentAction)?"TERMINATE":"KILL"));
                            jsonParam["markerFilters"] = TELEMETRY_MARKER_CLOSE_TIME_FILTER;
                            markerName = TELEMETRY_MARKER_CLOSE_TIME;
                        }
                break;
                default:
                    LOGERR("currentAction is invalid");
                break;
            }

            if(!markerName.empty())
            {
                jsonParam["appId"] = appId;
                jsonParam["appInstanceId"] = it->second.appInstanceId;
                jsonParam["appVersion"] = it->second.packageInfo.version;
                jsonParam["secondaryId"] = "appInstanceId";
                jsonParam["markerName"] = markerName;
                jsonParam.ToString(telemetryMetrics);
                if(!telemetryMetrics.empty())
                {
                    appMarker = appId + ":" + markerName;
                    mTelemetryPluginObject->Record(appMarker, telemetryMetrics);
                    mTelemetryPluginObject->Publish(appMarker);
                }
            }
        }
        else
        {
            LOGERR("Failed to report telemetry data as appId/mTelemetryPluginObject is not valid");
        }
    }

    void AppManagerTelemetryReporting::reportTelemetryErrorData(const std::string& appId, AppManagerImplementation::CurrentAction currentAction, AppManagerImplementation::CurrentActionError errorCode)
    {
        JsonObject jsonParam;
        std::string telemetryMetrics = "";
        std::string markerName = "";
        std::string appMarker = "";

        LOGINFO("Received data for appId %s current action %d app errorCode %d",appId.c_str(), currentAction, errorCode);

        if(nullptr == mTelemetryPluginObject) /*mTelemetryPluginObject is null retry to create*/
        {
            if(Core::ERROR_NONE != createTelemetryPluginObject())
            {
                LOGERR("Failed to create TelemetryObject\n");
            }
        }

        switch(currentAction)
        {
            case AppManagerImplementation::APP_ACTION_LAUNCH:
            case AppManagerImplementation::APP_ACTION_PRELOAD:
                jsonParam["markerFilters"] = TELEMETRY_MARKER_LAUNCH_ERROR_FILTER;
                markerName = TELEMETRY_MARKER_LAUNCH_ERROR;
            break;
            case AppManagerImplementation::APP_ACTION_CLOSE:
            case AppManagerImplementation::APP_ACTION_TERMINATE:
            case AppManagerImplementation::APP_ACTION_KILL:
                jsonParam["markerFilters"] = TELEMETRY_MARKER_CLOSE_ERROR_FILTER;
                markerName = TELEMETRY_MARKER_CLOSE_ERROR;
            break;
            default:
                LOGERR("currentAction is invalid");
            break;
        }

        if(!markerName.empty() && (nullptr != mTelemetryPluginObject))
        {
            jsonParam["errorCode"] = (int)errorCode;
            jsonParam["secondaryId"] = "appInstanceId";
            jsonParam["markerName"] = markerName;
            jsonParam.ToString(telemetryMetrics);
            if(!telemetryMetrics.empty())
            {
                appMarker = appId + ":" + markerName;
                mTelemetryPluginObject->Record(appMarker, telemetryMetrics);
                mTelemetryPluginObject->Publish(appMarker);
            }
        }
    }

} /* namespace Plugin */
} /* namespace WPEFramework */

