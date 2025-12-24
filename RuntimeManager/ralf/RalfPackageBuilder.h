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
#include <string>
#include <vector>
#include "RalfConstants.h"
#include "../ApplicationConfiguration.h"
#include <interfaces/IRuntimeManager.h>

namespace ralf
{

    class RalfPackageBuilder
    {
    private:
        /** This function generates an OCI root filesystem package based on the given application instance ID and Ralf package path.
         * @param appInstanceId The application instance ID for which the OCI root filesystem package is to be generated.
         * @param uid The user ID to set as owner of the created directories
         * @param gid The group ID to set as owner of the created directories
         * @param ociRootfsPath Output parameter that will hold the path to the generated OCI root filesystem package.
         * @return true if the OCI root filesystem package was generated successfully, false otherwise.
         */
        bool generateOCIRootfsPackage(const std::string &appInstanceId, const int uid, const int gid, std::string &ociRootfsPath);

        /**
         * The vector of Ralf package details as pairs of mount point and metadata path.
         */
        std::vector<RalfPkgInfoPair> mRalfPackages;
        /**
         * The file containing Ralf package/metadata details in json format.
         */
        std::string mRalfPkgInfo;

    public:
        RalfPackageBuilder(){};
        virtual ~RalfPackageBuilder() {}

        /**
         * This function generates the dobby specification for a Ralf package based on the given application configuration and runtime configuration.
         * We will get details about userid,groupid, mWesterosSocketPath etc from ApplicationConfiguration
         * and Ralf package details from RuntimeConfig
         * @param config The application configuration.
         * @param runtimeConfigObject The runtime configuration.
         * @param dobbySpec Output parameter that will hold the path to the RALF rootfs
         * @return true if the dobby specification was generated successfully, false otherwise.
         */
        bool generateRalfDobbySpec(const WPEFramework::Plugin::ApplicationConfiguration &config, const WPEFramework::Exchange::RuntimeConfig &runtimeConfigObject, std::string &dobbySpec);

        bool unmountOverlayfsIfExists(const std::string &appInstanceId);
        
    };
} // namespace ralf
