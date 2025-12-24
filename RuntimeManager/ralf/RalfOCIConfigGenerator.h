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
#include "../ApplicationConfiguration.h"
#include <interfaces/IRuntimeManager.h>
namespace ralf
{
    class RalfOCIConfigGenerator
    {
    public:
        RalfOCIConfigGenerator(const std::string &configFilePath, const std::vector<RalfPkgInfoPair> &ralfPackages) : mRalfPackages(ralfPackages),mConfigFilePath(configFilePath) {}
        virtual ~RalfOCIConfigGenerator() {}
        /**
         * Generates the OCI config JSON for the RALF application instance.
         * @param config The application configuration.
         * @param runtimeConfigObject The runtime configuration.
         * @param configFilePath The path where the OCI config file will be saved.
         */
        bool generateRalfOCIConfig(const WPEFramework::Plugin::ApplicationConfiguration &config, const WPEFramework::Exchange::RuntimeConfig &runtimeConfigObject);

    private:
        /**
         * Applies the graphics configuration to the OCI config JSON.
         * @param ociConfigRootNode The root node of the OCI config JSON.
         * @param graphicsConfigNode The graphics configuration JSON node.
         */
        bool applyGraphicsConfigToOCIConfig(Json::Value &ociConfigRootNode, const Json::Value &graphicsConfigNode);


        bool generateHooksForOCIConfig(Json::Value &ociConfigRootNode);
        bool generateHooksForOCIConfig(Json::Value &ociConfigRootNode, const std::string &operation);

        /**
         * Applies the configuration options from the Ralf package to the OCI config JSON.
         * The following parameters are expected to be applied:
         * entryPoint, permissions and configurations
         * @param ociConfigRootNode The root node of the OCI config JSON.
         * @param ralfPackageConfigNode The Ralf package configuration JSON node.
         * @return true if the configuration options were applied successfully, false otherwise.
         */
        bool applyRalfPackageConfigToOCIConfig(Json::Value &ociConfigRootNode, const Json::Value &ralfPackageConfigNode);

        /**
         * Saves the OCI config JSON to mConfigFilePath.
         * @param ociConfigRootNode The root node of the OCI config JSON.
         * @param uid The user ID to set as the owner of the file.
         * @param gid The group ID to set as the owner of the file.
         * @return true if the OCI config was saved successfully, false otherwise.
         */
        bool saveOCIConfigToFile(const Json::Value &ociConfigRootNode, int uid, int gid);

        /**
         * Applies runtime and application configuration to the OCI config JSON.
         * @param ociConfigRootNode The root node of the OCI config JSON.
         * @param runtimeConfigObject The runtime configuration.
         * @param config The application configuration.
         * @return true if the configuration options were applied successfully, false otherwise.
         */
        bool applyRuntimeAndAppConfigToOCIConfig(Json::Value &ociConfigRootNode, const WPEFramework::Exchange::RuntimeConfig &runtimeConfigObject, const WPEFramework::Plugin::ApplicationConfiguration &config);

        /**
         * Adds a mount entry to the OCI config JSON.
         * @param ociConfigRootNode The root node of the OCI config JSON.
         * @param source The source path of the mount.
         * @param destination The destination path of the mount.
         */
         void addMountEntry(Json::Value &ociConfigRootNode, const std::string &source, const std::string &destination);
        /**
         * The vector of Ralf package details as pairs of mount point and metadata path.
         */
        const std::vector<RalfPkgInfoPair> &mRalfPackages;
        /**
         * The OCI config file path to be generated.
         */
        const std::string &mConfigFilePath;
    };
} // namespace ralf
