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
#include "TelemetryMetricsReporting.h"
#include "UtilsLogging.h"
#include "tracing/Logging.h"

namespace WPEFramework
{
namespace Plugin
{
    LifecycleManagerTelemetryReporting::LifecycleManagerTelemetryReporting()
    {
    }

    LifecycleManagerTelemetryReporting::~LifecycleManagerTelemetryReporting()
    {
    }

    LifecycleManagerTelemetryReporting& LifecycleManagerTelemetryReporting::getInstance()
    {
        LOGINFO("Get LifecycleManagerTelemetryReporting Instance");
        static LifecycleManagerTelemetryReporting instance;
        return instance;
    }
    uint64_t LifecycleManagerTelemetryReporting::getCurrentTimestamp()
    {
        return (TelemetryMetricsReporting::getInstance().getCurrentTimestamp());
    }
    void LifecycleManagerTelemetryReporting::reportTelemetryDataOnStateChange(ApplicationContext* context, const JsonObject &data)
    {
        string appId = "";
        RequestType requestType = REQUEST_TYPE_NONE;
        uint64_t requestTime = 0;
        uint64_t currentTime = 0;
        Exchange::ILifecycleManager::LifecycleState targetLifecycleState;
        Exchange::ILifecycleManager::LifecycleState newLifecycleState;
        JsonObject jsonParam;
        std::string telemetryMetrics = "";
        std::string markerName = "";
        TelemetryMetricsReporting& telemetryMetricsInstance = TelemetryMetricsReporting::getInstance();

        if ((data.HasLabel("appId") && !(appId = data["appId"].String()).empty()))
        {
            if (nullptr != context)
            {
                requestType = context->getRequestType();
                requestTime = context->getRequestTime();
                currentTime = telemetryMetricsInstance.getCurrentTimestamp();
                targetLifecycleState = context->getTargetLifecycleState();
                newLifecycleState = static_cast<Exchange::ILifecycleManager::LifecycleState>(data["newLifecycleState"].Number());
                LOGINFO("Received state change for appId %s newLifecycleState %d requestType %d", appId.c_str(), newLifecycleState, requestType);

                if ((REQUEST_TYPE_LAUNCH == requestType) &&
                    (((Exchange::ILifecycleManager::LifecycleState::ACTIVE == newLifecycleState) && (Exchange::ILifecycleManager::LifecycleState::ACTIVE == targetLifecycleState)) ||
                    ((Exchange::ILifecycleManager::LifecycleState::PAUSED == newLifecycleState) && (Exchange::ILifecycleManager::LifecycleState::PAUSED == targetLifecycleState))))
                {
                    /*Telemetry reporting - launch case*/
                    jsonParam["lifecycleManagerSpawnTime"] = (int)(currentTime - requestTime);
                    jsonParam.ToString(telemetryMetrics);
                    markerName = TELEMETRY_MARKER_LAUNCH_TIME;
                }
                else if ((REQUEST_TYPE_TERMINATE == requestType) && (Exchange::ILifecycleManager::LifecycleState::UNLOADED == newLifecycleState))
                {
                    /*Telemetry reporting - close case*/
                    jsonParam["lifecycleManagerSetTargetStateTime"] = (int)(currentTime - requestTime);
                    jsonParam.ToString(telemetryMetrics);
                    markerName = TELEMETRY_MARKER_CLOSE_TIME;
                }
                else
                {
                    jsonParam["lifecycleManagerSetTargetStateTime"] = (int)(currentTime - requestTime);
                    jsonParam["appId"] = appId;
                    jsonParam["appInstanceId"] = context->getAppInstanceId();
                    jsonParam.ToString(telemetryMetrics);

                    if ((REQUEST_TYPE_SUSPEND == requestType) && (Exchange::ILifecycleManager::LifecycleState::SUSPENDED == newLifecycleState))
                    {
                        /*Telemetry reporting - suspend case*/
                        markerName = TELEMETRY_MARKER_SUSPEND_TIME;
                    }
                    else if ((REQUEST_TYPE_RESUME == requestType) && (Exchange::ILifecycleManager::LifecycleState::ACTIVE == newLifecycleState))
                    {
                        /*Telemetry reporting - resume case*/
                        markerName = TELEMETRY_MARKER_RESUME_TIME;
                    }
                    else if ((REQUEST_TYPE_HIBERNATE == requestType) && (Exchange::ILifecycleManager::LifecycleState::HIBERNATED == newLifecycleState))
                    {
                        /*Telemetry reporting - hibernate case*/
                        markerName = TELEMETRY_MARKER_HIBERNATE_TIME;
                    }
                    else if ((REQUEST_TYPE_TERMINATE == requestType) && (Exchange::ILifecycleManager::LifecycleState::SUSPENDED == newLifecycleState))
                    {
                        /*Telemetry reporting - wake case, wake is called during app terminate*/
                        markerName = TELEMETRY_MARKER_WAKE_TIME;
                    }
                }

                if(!markerName.empty() && !telemetryMetrics.empty())
                {
                    telemetryMetricsInstance.recordTelemetryMetrics(markerName, appId, telemetryMetrics);

                    if ((REQUEST_TYPE_SUSPEND == requestType) || (REQUEST_TYPE_HIBERNATE == requestType) || (REQUEST_TYPE_RESUME == requestType) ||
                        ((REQUEST_TYPE_TERMINATE == requestType) && (Exchange::ILifecycleManager::LifecycleState::SUSPENDED == newLifecycleState)))
                    {
                        telemetryMetricsInstance.publishTelemetryMetrics(markerName, appId);
                    }
                }
            }
        }
        else
        {
            LOGERR("appId not present or empty");
        }
    }

} /* namespace Plugin */
} /* namespace WPEFramework */

