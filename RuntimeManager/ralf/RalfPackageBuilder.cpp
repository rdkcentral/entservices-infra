/**
 * If not stated otherwise in this file or this component's LICENSE
 * file the following copyright and licenses apply:
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
 **/
#include "../Module.h"
#include "UtilsLogging.h"
#include "RalfPackageBuilder.h"
#include "RalfOCIConfigGenerator.h"
#include "RalfSupport.h"

#include <fstream>

namespace ralf
{

    bool RalfPackageBuilder::extractRalfPkgInfoFromMetadata(const std::string &ralfPkgInfo, std::vector<RalfPkgInfoPair> &ralfPackages)
    {
        return parseRalPkgInfo(ralfPkgInfo, ralfPackages);
    }
    bool RalfPackageBuilder::generateOCIRootfsPackage(const std::string &appInstanceId, const std::vector<RalfPkgInfoPair> &pkgInfoSet, std::string &ociRootfsPath)
    {
        // Let us extract the mount points.
        std::string packageLayers = RALF_GRAPHICS_LAYER_ROOTFS;
        for (const auto &package : pkgInfoSet)
        {
            packageLayers += ":" + package.second; // Append mount paths
        }

        // Create OCI rootfs package based on parsed data
        return generateOCIRootfs(appInstanceId, packageLayers, ociRootfsPath);
    }
    bool RalfPackageBuilder::checkAndReuseExistingConfig(const std::string &configFilePath)
    {
        bool status = false;
        std::ifstream infile(configFilePath.c_str());
        if (infile.good())
        {
            LOGINFO(" Reusing old configuration file: %s\n", configFilePath.c_str());
            status = true;
        }
        infile.close();
        return status;
    }
    bool RalfPackageBuilder::generateRalfPackageConfig(const std::string &configFilePath, const std::vector<RalfPkgInfoPair> &ralfPackages)
    {
        // TODO : Implement this function to generate Ralf package config json file.
        // Step 1. Check whether the config file already exists
        if (checkAndReuseExistingConfig(configFilePath))
        {
            LOGINFO(" Reusing old configuration file: %s\n", configFilePath.c_str());
            return true; // Reused existing config file
        }
        // Step 2, Load the base template stored as resource.
        RalfOCIConfigGenerator ralfOciGen;
        return ralfOciGen.generateRalfOCIConfig(configFilePath, ralfPackages);
    }

    bool RalfPackageBuilder::generateOCIRootfsPackageForAppInstance(const std::string &appInstanceId, const std::string &ralfPkgPath, std::string &ociRootfsPath)
    {
        // Step one extra ct Ralf package details from config
        std::vector<RalfPkgInfoPair> ralfPackages;
        if (!extractRalfPkgInfoFromMetadata(ralfPkgPath, ralfPackages))
        {
            LOGINFO("Failed to extract Ralf package details from config: %s\n", ralfPkgPath.c_str());
            return false;
        }
        LOGDBG("Extracted %d Ralf packages from config\n", (int)ralfPackages.size());
        // Step two: Generate OCI rootfs package
        if (!generateOCIRootfsPackage(appInstanceId, ralfPackages, ociRootfsPath))
        {
            LOGINFO("Failed to generate OCI rootfs package for appInstanceId: %s\n", appInstanceId.c_str());
            return false;
        }
        std::string configFilePath = RALF_APP_ROOTFS_DIR + appInstanceId + "/config.json";
        // Step three: Generate Ralf package config for  OCI rootfs package
        if (!generateRalfPackageConfig(configFilePath, ralfPackages))
        {
            LOGINFO("Failed to generate Ralf package config for OCI rootfs package: %s\n", ociRootfsPath.c_str());
            return false;
        }
        return true;
    }

    bool RalfPackageBuilder::generateRalfDobbySpec(const WPEFramework::Plugin::ApplicationConfiguration &config, const WPEFramework::Exchange::RuntimeConfig &runtimeConfigObject)
    {
        // TODO: Implement this function to generate the dobby specification for a Ralf package.
        return false;
    }
} // namespace ralf