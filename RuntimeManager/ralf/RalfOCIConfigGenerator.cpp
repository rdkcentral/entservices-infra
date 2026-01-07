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

    bool RalfOCIConfigGenerator::generateRalfOCIConfig(const WPEFramework::Plugin::ApplicationConfiguration &config, const WPEFramework::Exchange::RuntimeConfig &runtimeConfigObject)
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

        // Now apply each Ralf package configuration
        for (auto ralfPkgInfo : mRalfPackages)
        {
            Json::Value ralfPackageConfigNode;
            if (!JsonFromFile(ralfPkgInfo.first, ralfPackageConfigNode))
            {
                LOGERR("Failed to load Ralf package config JSON from file: %s", ralfPkgInfo.first.c_str());
                return false;
            }
            if (!applyRalfPackageConfigToOCIConfig(ociConfigRootNode, ralfPackageConfigNode))
            {
                LOGERR("Failed to apply Ralf package config to OCI config for package: %s", ralfPkgInfo.first.c_str());
                return false;
            }
        }
        if (generateHooksForOCIConfig(ociConfigRootNode) == false)
        {
            LOGERR("Failed to generate hooks for OCI config");
            return false;
        }
        // // Let us apply data from runtimeConfigObject and applicationConfiguration
        if (applyRuntimeAndAppConfigToOCIConfig(ociConfigRootNode, runtimeConfigObject, config) == false)
        {
            LOGERR("Failed to apply runtime and application config to OCI config");
            return false;
        }
        // Finally save the modified OCI config to file
        return saveOCIConfigToFile(ociConfigRootNode, config.mUserId, config.mGroupId);
    }
    bool RalfOCIConfigGenerator::applyRuntimeAndAppConfigToOCIConfig(Json::Value &ociConfigRootNode, const WPEFramework::Exchange::RuntimeConfig &runtimeConfigObject, const WPEFramework::Plugin::ApplicationConfiguration &appConfig)
    {
        bool status = true;
        // Set user and group ID
        ociConfigRootNode["process"]["user"]["uid"] = 0; // Run as root inside container
        ociConfigRootNode["process"]["user"]["gid"] = 0; // Run as root group inside container
        ociConfigRootNode["process"]["user"]["additionalGids"] = Json::Value(Json::arrayValue);
        ociConfigRootNode["process"]["user"]["additionalGids"].append(44); // video group

        // set hostname to appid
        ociConfigRootNode["hostname"] = appConfig.mAppId;

        // Set cwd as /home/root
        ociConfigRootNode["process"]["cwd"] = "/home/root";

        // Set uidMappings
        Json::Value uidMapping;
        uidMapping["containerID"] = 0;
        uidMapping["hostID"] = appConfig.mUserId;
        uidMapping["size"] = 1;
        ociConfigRootNode["linux"]["uidMappings"].append(uidMapping);

        // set gidMappings also
        Json::Value gidMapping;
        gidMapping["containerID"] = 0;
        gidMapping["hostID"] = appConfig.mGroupId;
        gidMapping["size"] = 1;
        ociConfigRootNode["linux"]["gidMappings"].append(gidMapping);

        // Add additional gidMapping for video group if not already added
        Json::Value vidMapping;
        vidMapping["containerID"] = 44;
        vidMapping["hostID"] = 44;
        vidMapping["size"] = 1;
        ociConfigRootNode["linux"]["gidMappings"].append(vidMapping);

        // Set westeros environment variable
        ociConfigRootNode["process"]["env"].append("WAYLAND_DISPLAY=" + appConfig.mWesterosSocketPath);
        // It is assumed that XDG_RUNTIME_DIR is set to appropriate value by the caller in WPEFramework::Exchange::RuntimeConfig::envVariables
        // That is a TODO. For now, we set it hardcoded.
        ociConfigRootNode["process"]["env"].append("XDG_RUNTIME_DIR=/tmp");

        // Need to mount bind  XDG_RUNTIME_DIR/WAYLAND_DISPLAY from host to container
        std::string xdgRuntimeDir = "/tmp"; // As set above
        std::string waylandSocketHostPath = xdgRuntimeDir + "/" + appConfig.mWesterosSocketPath;
        std::string waylandSocketContainerPath = waylandSocketHostPath; // Same path inside container
        addMountEntry(ociConfigRootNode, waylandSocketHostPath, waylandSocketContainerPath);
        LOGDBG("Mounted Wayland socket %s to container path %s\n", waylandSocketHostPath.c_str(), waylandSocketContainerPath.c_str());

        // Mount application storage path and set it as HOME environment variable
        std::string appStoragePath = appConfig.mAppStorageInfo.path;
        std::string homePath = "/home/root"; // Default HOME path
        ociConfigRootNode["process"]["env"].append("HOME=" + homePath);
        addMountEntry(ociConfigRootNode, appStoragePath, homePath);
        LOGDBG("Mounted application storage path %s to container HOME %s\n", appStoragePath.c_str(), homePath.c_str());

        //Finally add rialto path to the environment variables if RIALTO_IN_DAC_FEATURE_ENABLED is defined
        std::string rialtoSocketPath = "/tmp/rlto-" + appConfig.mAppInstanceId;;
        ociConfigRootNode["process"]["env"].append("RIALTO_SOCKET_PATH=" + rialtoSocketPath);
        LOGDBG("Added RIALTO_SOCKET environment variable with value %s\n", rialtoSocketPath.c_str());
        addMountEntry(ociConfigRootNode, rialtoSocketPath, rialtoSocketPath);
        LOGDBG("Mounted rialto socket path %s to container path %s\n", rialtoSocketPath.c_str(), rialtoSocketPath.c_str());
        return status;
    }
    bool RalfOCIConfigGenerator::generateHooksForOCIConfig(Json::Value &ociConfigRootNode)
    {
        // We need to add four hooks: createRuntime, createContainer, poststart, poststop
        return generateHooksForOCIConfig(ociConfigRootNode, "createRuntime") &&
               generateHooksForOCIConfig(ociConfigRootNode, "createContainer") &&
               generateHooksForOCIConfig(ociConfigRootNode, "poststart") &&
               generateHooksForOCIConfig(ociConfigRootNode, "poststop");
    }
    bool RalfOCIConfigGenerator::generateHooksForOCIConfig(Json::Value &ociConfigRootNode, const std::string &operation)
    {
        /*
        The hooks structure is as follows:
        "hooks": {
            "operation": [
                {
                    "path": "/usr/bin/DobbyPluginLauncher",
                    "args": [
                        "DobbyPluginLauncher",
                        "-h",
                        "operation",
                        "-c",
                        "path to config file",
                        "-vv"
                    ],
                }
            ]
        }
        */

        Json::Value hookEntry;
        hookEntry["path"] = "/usr/bin/DobbyPluginLauncher";
        hookEntry["args"] = Json::Value(Json::arrayValue);
        hookEntry["args"].append("DobbyPluginLauncher");
        hookEntry["args"].append("-h");
        hookEntry["args"].append(operation);
        hookEntry["args"].append("-c");
        hookEntry["args"].append(mConfigFilePath);
        hookEntry["args"].append("-vv");

        ociConfigRootNode["hooks"][operation] = Json::Value(Json::arrayValue);
        ociConfigRootNode["hooks"][operation].append(hookEntry);

        return true;
    }
    void RalfOCIConfigGenerator::addMountEntry(Json::Value &ociConfigRootNode, const std::string &source, const std::string &destination)
    {
        Json::Value mountEntry;
        mountEntry["source"] = source;
        mountEntry["destination"] = destination;
        mountEntry["type"] = "bind";

        Json::Value mountOptions(Json::arrayValue); 
        mountOptions.append("rbind");
        mountOptions.append("rw");
/*        mountOptions.append("nosuid");
        mountOptions.append("nodev");
        mountOptions.append("noexec");*/
        mountEntry["options"] = mountOptions;

        ociConfigRootNode["mounts"].append(mountEntry);
    }
    bool RalfOCIConfigGenerator::saveOCIConfigToFile(const Json::Value &ociConfigRootNode, int uid, int gid)
    {
        bool status = false;
        std::string ociConfigJson;
        Json::StreamWriterBuilder writer;
        ociConfigJson = Json::writeString(writer, ociConfigRootNode);

        LOGDBG("Generated OCI config JSON: Writing to file  %s\n", mConfigFilePath.c_str());
        // Write to file
        std::ofstream outFile(mConfigFilePath.c_str());
        if (outFile)
        {

            outFile << ociConfigJson;
            outFile.close();
            // Change ownership to uid:gid
            if (chown(mConfigFilePath.c_str(), uid, gid) != 0)
            {
                LOGERR("Failed to change ownership of OCI config file %s to %d:%d\n", mConfigFilePath.c_str(), uid, gid);
            }
            status = true;
        }
        else
        {
            LOGERR("Failed to open OCI config output file: %s", mConfigFilePath.c_str());
        }
        return status;
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
    bool RalfOCIConfigGenerator::applyRalfPackageConfigToOCIConfig(Json::Value &ociConfigRootNode, const Json::Value &ralfPackageConfigNode)
    {
        bool status = true;

        // Apply entryPoint if exists
        if (ralfPackageConfigNode.isMember("entryPoint"))
        {
            // args is a array of strings. So append each string to args array
            ociConfigRootNode["process"]["args"].append(ralfPackageConfigNode["entryPoint"]);
            LOGDBG("Applied entryPoint to OCI config\n");
        }
        else
        {
            LOGWARN("No entryPoint found in Ralf package config\n");
        }

        // Apply permissions if exists
        if (ralfPackageConfigNode.isMember("permissions"))
        {
            const Json::Value &permissions = ralfPackageConfigNode["permissions"];
            for (Json::Value::ArrayIndex i = 0; i < permissions.size(); ++i)
            {
                ociConfigRootNode["linux"]["capabilities"].append(permissions[i]);
                LOGDBG("Applied permission %s to OCI config\n", permissions[i].asString().c_str());
            }
        }
        else
        {
            LOGWARN("No permissions found in Ralf package config\n");
        }

        // Apply configurations if exists
        if (ralfPackageConfigNode.isMember("configurations"))
        {
            const Json::Value &configurations = ralfPackageConfigNode["configurations"];
            for (Json::Value::ArrayIndex i = 0; i < configurations.size(); ++i)
            {
                const Json::Value &config = configurations[i];
                // Assuming configurations are key-value pairs to be added as environment variables
                if (config.isMember("key") && config.isMember("value"))
                {
                    std::string envVar = config["key"].asString() + "=" + config["value"].asString();
                    ociConfigRootNode["process"]["env"].append(envVar);
                    LOGDBG("Applied configuration %s to OCI config\n", envVar.c_str());
                }
            }
        }
        else
        {
            LOGWARN("No configurations found in Ralf package config\n");
        }

        return status;
    }
} // namespace ralf
