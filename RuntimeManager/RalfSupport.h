
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
#pragma once
#include "Module.h"

#include <string>
#include <vector>
#include <utility>
#include <json/json.h>

#include <iostream>

#include <cstring> //for strerror

#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstdlib>
#include <cerrno>

#include "UtilsLogging.h"

namespace WPEFramework
{
    const std::string OVERLAYFS_TYPE = "overlay";
    const std::string RALF_APP_ROOTFS_DIR = "/tmp/ralf/";

    /**
     * Function to create directories recursively
     * @param path The directory path to create
     * @param mode The permissions mode for the directories
     * @return true on success, false on failure
     */
    bool create_directories(const std::string &path, mode_t mode = 0755)
    {
        if (path.empty())
            return false;

        std::string current_path;
        size_t pos = 0;

        // Skip leading slash if absolute path
        if (path[0] == '/')
        {
            current_path = "/";
            pos = 1;
        }

        while (pos < path.size())
        {
            // Find next slash
            size_t next_pos = path.find('/', pos);
            std::string dir = path.substr(pos, next_pos - pos);

            if (!dir.empty())
            {
                if (!current_path.empty() && current_path.back() != '/')
                    current_path += "/";
                current_path += dir;

                // Try to create directory
                if (mkdir(current_path.c_str(), mode) != 0)
                {
                    if (errno != EEXIST)
                    { // Ignore if already exists
                        LOGINFO("Error creating directory '%s': %s\n", current_path.c_str(), strerror(errno));
                        return false;
                    }
                }
            }

            if (next_pos == std::string::npos)
                break;
            pos = next_pos + 1;
        }

        return true;
    }
    /**
     * Given the Ralf package configuration data, parse and extract package metadata paths and mount paths
     * @param configData The Ralf package configuration data in JSON format
     * @param packages [out parameter] A vector of pairs containing package metadata paths and mount paths
     * @return true on success, false on failure
     */

    bool parseConfig(const std::string &configData, std::vector<std::pair<std::string, std::string> > &packages)
    {
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(configData, root))
        {
            return false;
        }

        for (Json::Value::ArrayIndex i = 0; i < root["packages"].size(); ++i)
        {
            std::string configData = root["packages"][i]["pkgMetaDataPath"].asString();
            std::string mountPath = root["packages"][i]["pkgMountPath"].asString();
            packages.push_back(std::make_pair(configData, mountPath));
        }
        return true;
    }

    /**
     *
     * Function to mount overlay filesystem. This function will create the directories if they do not exist.
     * The directory structure is as follows
     * RALF_APP_ROOTFS_DIR/appInstanceId/ will be the base read/write folder
     * RALF_APP_ROOTFS_DIR/appInstanceId/rootfs will be the merged read only directory
     * returns true on success, false on failure.
     * @param appInstanceId The application instance ID
     * @param pkgmountPaths The colon separated list of package mount paths
     * @param ociRootfsPath [out parameter] The path to the OCI rootfs package
     */
    bool generateOCIRootfs(const std::string appInstanceId, const std::string &pkgmountPaths, std::string &ociRootfsPath)
    {

        // Let us create a directory for app as RALF_APP_ROOTFS_DIR/appInstanceId
        std::string workDir = RALF_APP_ROOTFS_DIR + appInstanceId;
        std::string appRootfsDir = workDir + "/rootfs";
        create_directories(appRootfsDir);

        // Now we can mount the overlay filesystem
        std::string options = "lowerdir=" + pkgmountPaths + ",workdir=" + workDir;
        if (mount(OVERLAYFS_TYPE.c_str(), appRootfsDir.c_str(), OVERLAYFS_TYPE.c_str(), 0, options.c_str()) != 0)
        {
            LOGINFO("Error mounting overlayfs: %s\n", strerror(errno));
            return false;
        }
        ociRootfsPath = appRootfsDir;
        return true;
    }
}