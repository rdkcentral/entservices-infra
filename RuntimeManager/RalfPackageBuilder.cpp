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
#include "RalfPackageBuilder.h"
#include "RalfSupport.h"

#include <fstream>

namespace WPEFramework
{

    const std::string GRAPHICS_LAYER_ROOTFS = "/usr/share/gpu-layer/rootfs";

    bool RalfPackageBuilder::extractRalfPackagesFromConfig(const std::string &ralfPkgInfo, std::vector<RalfPackagesPair> &ralfPackages)
    {
        // Step 1: Read the configuration data from the file
        std::ifstream file(ralfPkgInfo.c_str(), std::ios::in | std::ios::binary);
        if (!file.is_open())
        {
            return false;
        }

        std::string configData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        // Step 2: Parse the configuration data
        return parseConfig(configData, ralfPackages);
    }
    bool RalfPackageBuilder::generateOCIRootfsPackage(const std::string &appInstanceId, const std::vector<RalfPackagesPair> &pkgInfoSet, std::string &ociRootfsPath)
    {
        // Let us extract the mount points.
        std::string packageLayers = GRAPHICS_LAYER_ROOTFS;
        for (const auto &package : pkgInfoSet)
        {
            packageLayers += ":" + package.second; // Append mount paths
        }

        // Create OCI rootfs package based on parsed data
        return generateOCIRootfs(appInstanceId, packageLayers, ociRootfsPath);
    }
    bool RalfPackageBuilder::generateRalfPackageConfig(const std::string &ociRootfsPath, const std::vector<RalfPackagesPair> &ralfPackages)
    {
        //TODO : Implement this function to generate Ralf package config json file.
        return true;
    }

    bool RalfPackageBuilder::generateOCIRootfsPackageForAppInstance(const std::string &appInstanceId, const std::string &ralfPkgPath, std::string &ociRootfsPath)
    {
        // Step one extra ct Ralf package details from config
        std::vector<RalfPackagesPair> ralfPackages;
        if (!extractRalfPackagesFromConfig(ralfPkgPath, ralfPackages))
        {
            LOGINFO("Failed to extract Ralf package details from config: %s\n", ralfPkgPath.c_str());
            return false;
        }
        // Step two: Generate OCI rootfs package
        if (!generateOCIRootfsPackage(appInstanceId, ralfPackages, ociRootfsPath))
        {
            LOGINFO("Failed to generate OCI rootfs package for appInstanceId: %s\n", appInstanceId.c_str());
            return false;
        }
        // Step three: Generate Ralf package config for  OCI rootfs package
        if (!generateRalfPackageConfig(ociRootfsPath, ralfPackages))
        {
            LOGINFO("Failed to generate Ralf package config for OCI rootfs package: %s\n", ociRootfsPath.c_str());
            return false;
        }
        return true;
    }

} // namespace WPEFramework