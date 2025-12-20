
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

#include "../Module.h" // To make logging work
#include "UtilsLogging.h"
#include "RalfConstants.h"
#include "RalfSupport.h"

#include <iostream>

#include <cstring> //for strerror

#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysmacros.h> // for major() and minor()
#include <unistd.h>
#include <cstdlib>
#include <cerrno>
#include <fstream>
#include <sstream>

namespace ralf
{

    bool create_directories(const std::string &path)
    {

        if (path.empty())
            return false;

        std::string current_path;
        mode_t mode = 0755; // Default permissions
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

    bool parseRalPkgInfo(const std::string &pkgInfoFile, std::vector<RalfPkgInfoPair> &packages)
    {
        Json::Value root;
        JsonFromFile(pkgInfoFile, root);
        if (!root.isMember("packages"))
        {
            LOGERR("Ralf package config JSON does not contain 'packages' field\n");
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
     * Function to read JSON data from a file
     * @param filePath The path to the JSON file
     * @param jsonData [out parameter] The parsed JSON data
     * @return true on success, false on failure
     */
    bool JsonFromFile(const std::string &filePath, Json::Value &rootNode)
    {
        bool status = false;
        std::ifstream file(filePath, std::ios::in);
        if (file.is_open())
        {

            std::string configData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();

            Json::CharReaderBuilder readerBuilder;
            std::string errs;
            std::istringstream s(configData);
            status = Json::parseFromStream(readerBuilder, s, &rootNode, &errs);
            if (!status)
                LOGERR("Failed to parse JSON: %s\n", errs.c_str());
        }
        else
        {
            LOGERR("Failed to open JSON file: %s\n", filePath.c_str());
        }
        LOGDBG("JsonFromFile [%s] ,status: %d\n", filePath.c_str(), status);
        return status;
    }

    bool generateOCIRootfs(const std::string appInstanceId, const std::string &pkgmountPaths, std::string &ociRootfsPath)
    {

        // Let us create a directory for app as RALF_APP_ROOTFS_DIR/appInstanceId
        std::string workDir = RALF_APP_ROOTFS_DIR + appInstanceId;
        std::string appRootfsDir = workDir + "/rootfs";
        create_directories(appRootfsDir);

        // Now we can mount the overlay filesystem
        std::string options = "lowerdir=" + pkgmountPaths + ",workdir=" + workDir;
        LOGDBG("Mounting overlayfs with options: %s\n", options.c_str());
        if (mount(RALF_OVERLAYFS_TYPE.c_str(), appRootfsDir.c_str(), RALF_OVERLAYFS_TYPE.c_str(), 0, options.c_str()) != 0)
        {
            LOGERR("Error mounting overlayfs: %s\n", strerror(errno));
            return false;
        }
        ociRootfsPath = appRootfsDir;
        return true;
    }

    bool getDevNodeMajorMinor(const std::string &devNodePath, unsigned int &majorNum, unsigned int &minorNum, char &devType)
    {
        struct stat st;
        // Get file status
        if (stat(devNodePath.c_str(), &st) == 0) // On success
        {
            majorNum = major(st.st_rdev);
            minorNum = minor(st.st_rdev);
            devType = S_ISBLK(st.st_mode) ? 'b' : (S_ISCHR(st.st_mode) ? 'c' : '\0');
        }
        return true;
    }
} // namespace ralf
