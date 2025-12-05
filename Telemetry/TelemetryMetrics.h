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

#pragma once

#include <string>
#include <unordered_set>
#include <unordered_map>
#include <mutex>
#include <json/json.h>
#include <core/core.h>

namespace WPEFramework {
namespace Plugin {

    //helper function to generate recordId
    std::string generateRecordId(const std::string& id, const std::string& name);

    //helper function to generate set from comma separated string
    std::unordered_set<std::string> generateFilterSet(const std::string& filterStr);

    /* Record telemetry metrics */
    Core::hresult RecordMetrics(
        const std::string& id,
        const std::string& metrics,
        const std::string& markerName,
        std::unordered_map<std::string, Json::Value>& metricsRecord,
        std::mutex& metricsMutex
    );

    /* Publish telemetry metrics */
    Core::hresult PublishMetrics(
        const std::string& id,
        const std::string& markerName,
        std::unordered_map<std::string, Json::Value>& metricsRecord,
        std::mutex& metricsMutex
    );

} // namespace Plugin
} // namespace WPEFramework
