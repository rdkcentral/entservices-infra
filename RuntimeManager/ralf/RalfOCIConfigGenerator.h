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
#include <json/json.h>
#include "RalfConstants.h"

namespace ralf
{
    class RalfOCIConfigGenerator
    {
    public:
        RalfOCIConfigGenerator() {}
        virtual ~RalfOCIConfigGenerator() {}
        /**
         * Generates the OCI config JSON for the RALF application instance.
         * @param appInstanceId The application instance identifier.
         * @param ralfPackages The Ralf packages information.
         */
        bool generateRalfOCIConfig(const std::string &configFilePath, const std::vector<RalfPkgInfoPair> &ralfPackages);

    private:
        /**
         * Applies the graphics configuration to the OCI config JSON.
         * @param ociConfigRootNode The root node of the OCI config JSON.
         * @param graphicsConfigNode The graphics configuration JSON node.
         */
        bool applyGraphicsConfigToOCIConfig(Json::Value &ociConfigRootNode, const Json::Value &graphicsConfigNode);

        /**
         * Applies the configuration options from the Ralf package to the OCI config JSON.
         * The following parameters are expected to be applied:
         * entryPoint, permissions and configurations
         * @param ociConfigRootNode The root node of the OCI config JSON.
         * @param ralfPackageConfigNode The Ralf package configuration JSON node.
         * @return true if the configuration options were applied successfully, false otherwise.
         */
        bool applyRalfPackageConfigToOCIConfig(Json::Value &ociConfigRootNode, const Json::Value &ralfPackageConfigNode);
    };
} // namespace ralf
