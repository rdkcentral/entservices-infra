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
#include <UtilsLogging.h>
#include "RalfPackageBuilder.h"
#include "RalfOCIConfigGenerator.h"
#include "RalfSupport.h"

#include <fstream>

namespace ralf
{

    bool RalfPackageBuilder::generateOCIRootfsPackage(const std::string &appInstanceId, const int uid, const int gid, std::string &ociRootfsPath)
    {
        // Let us extract the mount points.
        std::string packageLayers = RALF_GRAPHICS_LAYER_ROOTFS;
        for (const auto &package : mRalfPackages)
        {
            packageLayers += ":" + package.second; // Append mount paths
        }
        // Create OCI rootfs package based on parsed data
        return generateOCIRootfs(appInstanceId, packageLayers, uid, gid, ociRootfsPath);
    }

    bool RalfPackageBuilder::generateRalfDobbySpec(const WPEFramework::Plugin::ApplicationConfiguration &config, const WPEFramework::Exchange::RuntimeConfig &runtimeConfigObject, std::string &dobbySpec)
    {
        std::string ralfPkgPath = runtimeConfigObject.ralfPkgPath;

        // Step one: Extract Ralf package details from metadata
        bool status = parseRalPkgInfo(ralfPkgPath, mRalfPackages);
        if (!status)
        {
            LOGERR("Failed to parse Ralf package info from metadata: %s\n", ralfPkgPath.c_str());
            return false;
        }
        LOGDBG("Extracted %d Ralf packages from config\n", (int)mRalfPackages.size());
        // Step two: Generate overlay OCI rootfs package for the application instance
        std::string ociRootfsPath;
        ;
        status = generateOCIRootfsPackage(config.mAppInstanceId, config.mUserId, config.mGroupId, ociRootfsPath);
        if (!status)
        {
            LOGERR("Failed to generate OCI rootfs package for appInstanceId: %s\n", config.mAppInstanceId.c_str());
            return status;
        }
        dobbySpec = ociRootfsPath;
        LOGDBG("Generated OCI rootfs package at path: %s\n", ociRootfsPath.c_str());
        // There is zero chance that a mount path already exists, but there is a chance for config file to exist.
        std::string configFilePath = RALF_APP_ROOTFS_DIR + config.mAppInstanceId + "/config.json";

        RalfOCIConfigGenerator ralfOciGen(configFilePath, mRalfPackages);
        status = ralfOciGen.generateRalfOCIConfig(config, runtimeConfigObject);
        return status;
    }

    bool RalfPackageBuilder::unmountOverlayfsIfExists(const std::string &appInstanceId)
    {
        // We need to unmount the overlayfs mount point if exists
        std::string overlayMountPath = RALF_APP_ROOTFS_DIR + appInstanceId + "/rootfs";
        LOGDBG("Checking and unmounting overlayfs at path: %s\n", overlayMountPath.c_str());
        bool status = checkIfPathExists(overlayMountPath);
        if (status)
        {
            status = unmountOverlayfs(overlayMountPath);
            if (!status)
            {
                LOGERR("Failed to unmount overlayfs at path: %s\n", overlayMountPath.c_str());
                status = false;
            }
            else
            {
                LOGDBG("Successfully unmounted overlayfs at path: %s\n", overlayMountPath.c_str());
                status = true;
            }
        }
        else
        {
            LOGDBG("No overlayfs mount exists at path: %s\n", overlayMountPath.c_str());
            status = false;
        }
        return status;
    }
} // namespace ralf