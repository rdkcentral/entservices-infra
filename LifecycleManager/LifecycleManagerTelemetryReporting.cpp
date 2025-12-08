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

#include "LifecycleManagerTelemetryReporting.h"
#include "UtilsLogging.h"
#include "tracing/Logging.h"
#include <time.h>

namespace WPEFramework
{
namespace Plugin
{
    LifecycleManagerTelemetryReporting::LifecycleManagerTelemetryReporting(): mTelemetryPluginObject(nullptr), mCurrentservice(nullptr)
    {
    }

    LifecycleManagerTelemetryReporting::~LifecycleManagerTelemetryReporting()
    {
        if(mTelemetryPluginObject )
        {
            mTelemetryPluginObject ->Release();
            mTelemetryPluginObject = nullptr;
            mCurrentservice = nullptr;
        }
    }

    LifecycleManagerTelemetryReporting& LifecycleManagerTelemetryReporting::getInstance()
    {
        LOGINFO("Get LifecycleManagerTelemetryReporting Instance");
        static LifecycleManagerTelemetryReporting instance;
        return instance;
    }

    void LifecycleManagerTelemetryReporting::initialize(PluginHost::IShell* service)
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

    time_t LifecycleManagerTelemetryReporting::getCurrentTimestamp()
    {
        timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return ((time_t)(ts.tv_sec * 1000) + ((time_t)ts.tv_nsec/1000000));
    }

/*
* Creates Telemetry plugin object to access interface methods
*/
    Core::hresult LifecycleManagerTelemetryReporting::createTelemetryPluginObject()
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

    void LifecycleManagerTelemetryReporting::reportTelemetryDataOnStateChange(ApplicationContext* context, const JsonObject &data)
    {
        string appId = "";
        RequestType requestType = REQUEST_TYPE_NONE;
        time_t requestTime = 0;
        time_t currentTime = 0;
        Exchange::ILifecycleManager::LifecycleState targetLifecycleState;
        Exchange::ILifecycleManager::LifecycleState newLifecycleState;
        JsonObject jsonParam;
        std::string telemetryMetrics = "";
        std::string markerName = "";
        std::string appMarker = "";
        bool shouldPublish = false;

        if(nullptr == mTelemetryPluginObject) /*mTelemetryPluginObject is null retry to create*/
        {
            if(Core::ERROR_NONE != createTelemetryPluginObject())
            {
                LOGERR("Failed to create TelemetryObject\n");
            }
        }

        if (nullptr == context)
        {
            LOGERR("context is nullptr");
        }
        else if (nullptr == mTelemetryPluginObject)
        {
            LOGERR("mTelemetryPluginObject is not valid");
        }
        else if (!data.HasLabel("appId") || (appId = data["appId"].String()).empty())
        {
            LOGERR("appId not present or empty");
        }
        else
        {
            requestType = context->getRequestType();
            requestTime = context->getRequestTime();
            currentTime = getCurrentTimestamp();
            targetLifecycleState = context->getTargetLifecycleState();
            newLifecycleState = static_cast<Exchange::ILifecycleManager::LifecycleState>(data["newLifecycleState"].Number());
            LOGINFO("Received state change for appId %s newLifecycleState %d requestType %d", appId.c_str(), newLifecycleState, requestType);

            switch(requestType)
            {
                case REQUEST_TYPE_LAUNCH:
                    if (((Exchange::ILifecycleManager::LifecycleState::ACTIVE == newLifecycleState) && (Exchange::ILifecycleManager::LifecycleState::ACTIVE == targetLifecycleState)) ||
                        ((Exchange::ILifecycleManager::LifecycleState::PAUSED == newLifecycleState) && (Exchange::ILifecycleManager::LifecycleState::PAUSED == targetLifecycleState)))
                    {
                        /*Telemetry reporting - launch case*/
                        markerName = TELEMETRY_MARKER_LAUNCH_TIME;
                        jsonParam["lifecycleManagerSpawnTime"] = (int)(currentTime - requestTime);
                        jsonParam["markerFilters"] = TELEMETRY_MARKER_LAUNCH_TIME_FILTER;
                        jsonParam["markerName"] = markerName;
                        jsonParam.ToString(telemetryMetrics);
                        appMarker = appId + ":" + markerName;
                        mTelemetryPluginObject->Record(appMarker, telemetryMetrics);
                    }
                break;
                case REQUEST_TYPE_TERMINATE:
                    if(Exchange::ILifecycleManager::LifecycleState::UNLOADED == newLifecycleState)
                    {
                        /*Telemetry reporting - close case*/
                        markerName = TELEMETRY_MARKER_CLOSE_TIME;
                        jsonParam["lifecycleManagerSetTargetStateTime"] = (int)(currentTime - requestTime);
                        jsonParam["markerFilters"] = TELEMETRY_MARKER_CLOSE_TIME_FILTER;
                        jsonParam["markerName"] = markerName;
                        jsonParam.ToString(telemetryMetrics);
                        appMarker = appId + ":" + markerName;
                        mTelemetryPluginObject->Record(appMarker, telemetryMetrics);
                    }
                    else if(Exchange::ILifecycleManager::LifecycleState::SUSPENDED == newLifecycleState)
                    {
                        /*Telemetry reporting - wake case, wake is called during app terminate*/
                        markerName = TELEMETRY_MARKER_WAKE_TIME;
                        jsonParam["markerFilters"] = TELEMETRY_MARKER_WAKE_TIME_FILTER;
                        shouldPublish = true;
                    }
                break;
                case REQUEST_TYPE_SUSPEND:
                    /*Telemetry reporting - suspend case*/
                    if(Exchange::ILifecycleManager::LifecycleState::SUSPENDED == newLifecycleState)
                    {
                        markerName = TELEMETRY_MARKER_SUSPEND_TIME;
                        jsonParam["markerFilters"] = TELEMETRY_MARKER_SUSPEND_TIME_FILTER;
                        shouldPublish = true;
                    }
                break;
                case REQUEST_TYPE_RESUME:
                    /*Telemetry reporting - resume case*/
                    if(Exchange::ILifecycleManager::LifecycleState::ACTIVE == newLifecycleState)
                    {
                        markerName = TELEMETRY_MARKER_RESUME_TIME;
                        jsonParam["markerFilters"] = TELEMETRY_MARKER_RESUME_TIME_FILTER;
                        shouldPublish = true;
                    }
                break;
                case REQUEST_TYPE_HIBERNATE:
                    /*Telemetry reporting - hibernate case*/
                    if(Exchange::ILifecycleManager::LifecycleState::HIBERNATED == newLifecycleState)
                    {
                        markerName = TELEMETRY_MARKER_HIBERNATE_TIME;
                        jsonParam["markerFilters"] = TELEMETRY_MARKER_HIBERNATE_TIME_FILTER;
                        shouldPublish = true;
                    }
                break;
                default:
                    LOGERR("requestType is invalid");
                break;
            }

            if (!markerName.empty() && shouldPublish)
            {
                jsonParam["appId"] = appId;
                jsonParam["appInstanceId"] = context->getAppInstanceId();
                jsonParam["lifecycleManagerSetTargetStateTime"] = (int)(currentTime - requestTime);
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
    }

} /* namespace Plugin */
} /* namespace WPEFramework */

