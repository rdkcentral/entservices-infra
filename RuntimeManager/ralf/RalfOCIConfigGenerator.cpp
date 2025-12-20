/**
 * If not stated otherwise in this file or this component's LICENSE
 * file the following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * You may not use this file except in compliance with the License.
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
#include "../Module.h" //Otherwise logging won't work
#include <UtilsLogging.h>

#include "RalfOCIConfigGenerator.h"
#include "RalfSupport.h"
#include <fstream>

namespace ralf
{

    bool RalfOCIConfigGenerator::generateRalfOCIConfig(const std::string &configFilePath, const std::vector<RalfPkgInfoPair> &ralfPackages)
    {
        Json::Value ociConfigRootNode;

        if (!JsonFromFile(RALF_OCI_BASE_SPEC_FILE, ociConfigRootNode))
        {
            LOGERR("Failed to load base OCI config template");
            return false;
        }
        // Load graphics config and integrate into OCI config
        Json::Value graphicsConfigNode;
        if (!JsonFromFile(RALF_GRAPHICS_LAYER_CONFIG, graphicsConfigNode))
        {
            LOGERR("Failed to load Ralf graphics config JSON from file: %s", RALF_GRAPHICS_LAYER_CONFIG.c_str());
            return false;
        }

        // Apply graphics configuration
        if (!applyGraphicsConfigToOCIConfig(ociConfigRootNode, graphicsConfigNode))
        {
            LOGERR("Failed to apply graphics config to OCI config");
            return false;
        }

        // Convert the modified JSON object back to a string
        std::string ociConfigJson;
        Json::StreamWriterBuilder writer;
        ociConfigJson = Json::writeString(writer, ociConfigRootNode);

        // Write to file
        std::ofstream outFile(configFilePath);
        if (!outFile)
        {
            LOGERR("Failed to open OCI config output file: %s", configFilePath.c_str());
            return false;
        }
        outFile << ociConfigJson;
        outFile.close();
        LOGINFO("Successfully generated Ralf OCI config file: %s", configFilePath.c_str());

        return true;
    }

    bool RalfOCIConfigGenerator::applyGraphicsConfigToOCIConfig(Json::Value &ociConfigRootNode, const Json::Value &graphicsConfigNode)
    {
        bool status = false;
        /* The structure is as follows
        {
            "vendorGpuSupport": {
            "devNodes": [
            "/dev/mali0",
            "/dev/dri/card0",
            "/dev/dma_heap/system",
            "/dev/dma_heap/vo_non-ve"
            ],
            "groupIds": [
            "video"
            ]
            }
        }

        We need to get the devNodes and groupIds and apply them to the OCI config json structure.
        */
        // Check if vendorGpuSupport/devNodes exists
        if (graphicsConfigNode.isMember("vendorGpuSupport") && graphicsConfigNode["vendorGpuSupport"].isMember("devNodes"))
        {
            const Json::Value &devNodes = graphicsConfigNode["vendorGpuSupport"]["devNodes"];
            for (Json::Value::ArrayIndex i = 0; i < devNodes.size(); ++i)
            {
                std::string devNodePath = devNodes[i].asString();
                unsigned int majorNum = 0, minorNum = 0;
                char devType = '\0';
                if (getDevNodeMajorMinor(devNodePath, majorNum, minorNum, devType))
                {
                    Json::Value deviceNode;
                    deviceNode["path"] = devNodePath;
                    deviceNode["type"] = std::string(1, devType);
                    deviceNode["major"] = majorNum;
                    deviceNode["minor"] = minorNum;

                    ociConfigRootNode["linux"]["devices"].append(deviceNode);

                    // Add in the resources devices section as well
                    Json::Value resourceDevice;
                    resourceDevice["type"] = std::string(1, devType);
                    resourceDevice["major"] = majorNum;
                    resourceDevice["minor"] = minorNum;
                    resourceDevice["access"] = "rwm";
                    resourceDevice["allow"] = true;
                    ociConfigRootNode["linux"]["resources"]["devices"].append(resourceDevice);
                    LOGDBG("Added device node to OCI config: %s (type=%c, major=%u, minor=%u)\n", devNodePath.c_str(), devType, majorNum, minorNum);
                }
                else
                {
                    LOGWARN("Failed to get major/minor for device node: %s\n", devNodePath.c_str());
                }
            }
            status = true;
        }
        else
        {
            LOGWARN("No vendorGpuSupport/devNodes found in graphics config\n");
        }
        return status;
    }
} // namespace ralf
