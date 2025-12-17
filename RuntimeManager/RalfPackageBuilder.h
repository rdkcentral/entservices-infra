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

namespace WPEFramework
{

    class RalfPackageBuilder
    {

    private:
        typedef std::pair<std::string, std::string> RalfPackagesPair;

        /** This function generates an OCI root filesystem package based on the given application instance ID and Ralf package path.
         * @param appInstanceId The application instance ID for which the OCI root filesystem package is to be generated.
         * @param pkgInfoSet The vector of pairs containing Ralf package mount points and their corresponding metadata in json format.
         * @param ociRootfsPath Output parameter that will hold the path to the generated OCI root filesystem package.
         * @return true if the OCI root filesystem package was generated successfully, false otherwise.
         */
        bool generateOCIRootfsPackage(const std::string &appInstanceId, const std::vector<RalfPackagesPair> &pkgInfoSet, std::string &ociRootfsPath);

        /**
         * This function generates a Ralf package configuration based on the given OCI root filesystem path.
         * The file generated will be ociRootfsPath/config.json
         * @param ociRootfsPath The path to the OCI root filesystem.
         * @param ralfPackages A vector of pairs containing Ralf package mount points and their corresponding metadata in json format.
         * @return true if the Ralf package configuration was generated successfully, false otherwise.
         */
        bool generateRalfPackageConfig(const std::string &ociRootfsPath, const std::vector<RalfPackagesPair> &ralfPackages);

        /**
         * This function extracts Ralf package details from the given configuration data.
         * @param ralfPkgInfo Path to the json configuration data containing Ralf package details.
         * @param ralfPackages Output parameter that will hold the extracted Ralf package details as a vector of pairs.
         * @return true if the Ralf package details were extracted successfully, false otherwise.
         */
        bool extractRalfPackagesFromConfig(const std::string &ralfPkgInfo, std::vector<RalfPackagesPair> &ralfPackages);

    public:
        RalfPackageBuilder() {}
        virtual ~RalfPackageBuilder() {}

        /**
         * This function generates an OCI root filesystem package for a given application instance ID
         * @param appInstanceId The application instance ID for which the OCI root filesystem package is to be generated.
         * @param ralfPkgPath The path to the Ralf package/metadata details in json format.
         * @param ociRootfsPath Output parameter that will hold the path to the generated OCI root filesystem package.
         * @return true if the OCI root filesystem package was generated successfully, false otherwise.
         */
        bool generateOCIRootfsPackageForAppInstance(const std::string &appInstanceId, const std::string &ralfPkgPath, std::string &ociRootfsPath);
    };
} // namespace WPEFramework