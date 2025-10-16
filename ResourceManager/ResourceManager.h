/**
 * If not stated otherwise in this file or this component's LICENSE
 * file the following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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
#include <interfaces/IResourceManager.h>
#include <interfaces/json/JResourceManager.h>

namespace WPEFramework
{
    namespace Plugin
    {
        class ResourceManager : public PluginHost::IPlugin, public PluginHost::JSONRPC
        {
            public:
                ResourceManager(const ResourceManager&) = delete;
                ResourceManager& operator=(const ResourceManager&) = delete;

                ResourceManager();
                virtual ~ResourceManager();

                BEGIN_INTERFACE_MAP(ResourceManager)
                    INTERFACE_ENTRY(PluginHost::IPlugin)
                    INTERFACE_ENTRY(PluginHost::IDispatcher)
                    INTERFACE_AGGREGATE(Exchange::IResourceManager, _resourceManager)
                END_INTERFACE_MAP

                //  IPlugin methods
                const string Initialize(PluginHost::IShell* service) override;
                void Deinitialize(PluginHost::IShell* service) override;
                string Information() const override;

            private:
                PluginHost::IShell* _service{};
                uint32_t _connectionId{};
                Exchange::IResourceManager* _resourceManager{};
        };
    } // namespace Plugin
} // namespace WPEFramework