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

#include "Module.h"
#include "TelemetryMetrics.h"
#include "UtilsLogging.h"
#include "UtilsTelemetry.h"
#include <sstream>
#include <memory>

namespace WPEFramework {
namespace Plugin {

    //helper function to generate set from comma separated string
    std::unordered_set<std::string> generateFilterSet(const std::string& filterStr)
    {
        std::unordered_set<std::string> filterSet;
        std::stringstream ss(filterStr);
        std::string item;

        while (std::getline(ss, item, ','))
        {
            filterSet.insert(item);
        }

        return filterSet;
    }

    Core::hresult RecordMetrics(
        const std::string& id,
        const std::string& metrics,
        std::unordered_map<std::string, Json::Value>& metricsRecord,
        std::mutex& metricsMutex)
    {
        Core::hresult status = Core::ERROR_GENERAL;

        /* Create JSON parser builder and CharReader for parsing JSON strings */
        Json::CharReaderBuilder builder;
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

        Json::Value newMetrics = Json::objectValue;
        std::string errs = "";

        /*  Parse the input JSON string into newMetrics */
        if (!reader->parse(metrics.c_str(), metrics.c_str() + metrics.size(), &newMetrics, &errs))
        {
            LOGERR("JSON parse failed: %s", errs.c_str());
        }
        else if (!newMetrics.isObject())
        {
            LOGERR("Input metrics must be a JSON object");
        }
        else
        {
            std::lock_guard<std::mutex> lock(metricsMutex);
            std::string recordId = id;
            if (!recordId.empty())
            {
                bool isNewRecord = (metricsRecord.find(recordId) == metricsRecord.end());

                /* Get existing metrics for the key or creates new entry if not exist */
                Json::Value &existing = metricsRecord[recordId];

                /* store markerName inside JSON value the first time */
                if (isNewRecord)
                {
                    existing["markerName"] = newMetrics["markerName"];
                    LOGINFO("Storing new markerName '%s' for recordId '%s'", markerName.c_str(), recordId.c_str());
                }
                else
                {
                    LOGINFO("RecordId '%s' already exists. markerName unchanged.", recordId.c_str());
                }
                newMetrics.removeMember("markerName"); // remove to avoid duplication

                /* Merge each metric from newMetrics into existing record */
                for (const std::string &metricKey : newMetrics.getMemberNames())
                {
                    if (existing.isMember(metricKey))
                    {
                        LOGWARN("Record:'%s' Overwriting key '%s'", recordId.c_str(), metricKey.c_str());
                    }
                    else
                    {
                        LOGINFO("Record:'%s' Adding new key '%s'", recordId.c_str(), metricKey.c_str());
                    }
                    existing[metricKey] = newMetrics[metricKey];
                }
                status = Core::ERROR_NONE;
            }
        }

        return status;
    }

    Core::hresult PublishMetrics(
        const std::string& id,
        std::unordered_map<std::string, Json::Value>& metricsRecord,
        std::mutex& metricsMutex)
    {
        Core::hresult status = Core::ERROR_GENERAL;

        std::string recordId = id;
        Json::Value filteredMetrics = Json::objectValue;
        std::string alternateId = ""; // To store value of the secondary ID field from current record (e.g, instanceID)
        std::string markerFilters = "";
        std::string secondaryIdField = "";
        std::string matchedOtherRecordId = "";
        std::unordered_set<std::string> filterKeys = {};

        bool error = false;
        bool useFilter = false;

        /* Lock mutex once for the entire critical section */
        std::lock_guard<std::mutex> lock(metricsMutex);


        /* Filter current recordMetrics and extract value of mergeKey */
        auto currentRecordIt = metricsRecord.find(recordId);
        if (currentRecordIt == metricsRecord.end())
        {
            LOGERR("Current record not found: %s", recordId.c_str());
            error = true;
        }
        else
        {
            Json::Value& currentMetrics = currentRecordIt->second;

        // secondaryIdField  Field name used to merge other records with this key as its identifier
        // markerFilters     A comma separated string of allowed metric keys for filtering.
            secondaryIdField = currentMetrics.get("secondaryId", "").asString(); //get alternate id value if present
            markerFilters = currentMetrics.get("markerFilters", "").asString(); //get marker filters if present
            useFilter = !markerFilters.empty();

            currentMetrics.removeMember("secondaryId");
            currentMetrics.removeMember("markerFilters"); // to prevent from it being published

            /* Generate filter keys if filters are provided */
            if (useFilter)
            {
                filterKeys = generateFilterSet(markerFilters);
                if (filterKeys.empty())
                {
                    LOGERR("Filter list error for marker: %s", markerName.c_str());
                    useFilter = false; // skip filtering in case of error
                }
            }

            for (const std::string &key : currentMetrics.getMemberNames())
            {
                if (!useFilter || filterKeys.count(key)) //bypass filtering if no filters are set
                {
                    filteredMetrics[key] = currentMetrics[key];
                }
                else
                {
                    LOGWARN("Key '%s' not allowed by filter for marker '%s'", key.c_str(), markerName.c_str());
                }

                if (!secondaryIdField.empty() && key == secondaryIdField && alternateId.empty())
                {
                    alternateId = currentMetrics[key].asString();
                    LOGDBG("Populated alternate identifier value as %s for %s", alternateId.c_str(), secondaryIdField.c_str());
                }
            }
        }

        if (!alternateId.empty() && filteredMetrics.isMember("markerName"))
        {
            alternateId = alternateId + ":" + filteredMetrics["markerName"].asString();
        }


        /* Merge other recordMetrics with the same id as alternate identifier and markerName */
        if (!error && !alternateId.empty())
        {
            if (metricsRecord.find(alternateId) == metricsRecord.end())
            {
                LOGINFO("No other records found to merge with alternateId: %s", alternateId.c_str());
            }
            else
            {
                LOGINFO("Merging records with alternateId: %s", alternateId.c_str());
                const Json::Value& otherMetrics = metricsRecord[alternateId];
                for (const std::string &key : otherMetrics.getMemberNames())
                {
                    if (!useFilter || filterKeys.count(key))
                    {
                        filteredMetrics[key] = otherMetrics[key];
                        LOGINFO("Merged key '%s' from '%s' into current record", key.c_str(), otherRecordId.c_str());
                    }
                }
                matchedOtherRecordId = alternateId;
                LOGINFO("Merged record: '%s' into '%s'", alternateId.c_str(), recordId.c_str());
            }
        }

        /* Publish metrics if no errors occurred */
        if (!error)
        {
            Json::StreamWriterBuilder writerBuilder;
            writerBuilder["indentation"] = " ";
            std::string publishMetrics = Json::writeString(writerBuilder, filteredMetrics);

            LOGINFO("Publishing metrics for RecordId:'%s' publishMetrics:'%s'", recordId.c_str(), publishMetrics.c_str());
            t2_event_s((char*)markerName.c_str(), (char*)publishMetrics.c_str());

            /* Remove Published Record */
            metricsRecord.erase(recordId);
            if (!matchedOtherRecordId.empty())
            {
                metricsRecord.erase(matchedOtherRecordId);
            }
            LOGINFO("Cleared published record: %s", recordId.c_str());

            status = Core::ERROR_NONE;
        }

        return status;
    }

} // namespace Plugin
} // namespace WPEFramework
