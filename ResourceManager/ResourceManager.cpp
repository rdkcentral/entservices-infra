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

#include "ResourceManager.h"

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 1

namespace WPEFramework {
    namespace {

        static Plugin::Metadata<Plugin::ResourceManager> metadata(
            // Version (Major, Minor, Patch)
            API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH,
            // Preconditions
            {},
            // Terminations
            {},
            // Controls
            {}
        );
    }

    namespace Plugin {
        SERVICE_REGISTRATION(ResourceManager, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

        ResourceManager::ResourceManager()
            : _service(nullptr)
            , _connectionId(0)
            , _resourceManager(nullptr)
        {
            SYSLOG(Logging::Startup, (_T("ResourceManager Constructor")));
        }

        ResourceManager::~ResourceManager()
        {
            SYSLOG(Logging::Shutdown, (string(_T("ResourceManager Destructor"))));
        }

        const string ResourceManager::Initialize(PluginHost::IShell* service)
        {
            string message = "";

            ASSERT(nullptr != service);
            ASSERT(nullptr == _service);
            ASSERT(nullptr == _resourceManager);
            ASSERT(0 == _connectionId);

            SYSLOG(Logging::Startup, (_T("ResourceManager::Initialize: PID=%u"), getpid()));

            _service = service;
            _service->AddRef();
            _resourceManager = _service->Root<Exchange::IResourceManager>(_connectionId, 5000, _T("ResourceManagerImplementation"));

            if (nullptr != _resourceManager)
            {
                // Register JSON-RPC interface
                Exchange::JResourceManager::Register(*this, _resourceManager);
            }
            else
            {
                SYSLOG(Logging::Startup, (_T("ResourceManager::Initialize: Failed to initialise ResourceManager plugin")));
                message = _T("ResourceManager plugin could not be initialised");
            }
            return message;
        }

        void ResourceManager::Deinitialize(PluginHost::IShell* service)
        {
            ASSERT(_service == service);

            SYSLOG(Logging::Shutdown, (string(_T("ResourceManager::Deinitialize"))));

            if (nullptr != _resourceManager)
            {
                Exchange::JResourceManager::Unregister(*this);

                RPC::IRemoteConnection* connection = service->RemoteConnection(_connectionId);
                VARIABLE_IS_NOT_USED uint32_t result = _resourceManager->Release();
                _resourceManager = nullptr;

                ASSERT(result == Core::ERROR_DESTRUCTION_SUCCEEDED);

                if (connection != nullptr)
                {
                    connection->Terminate();
                    connection->Release();
                }
            }

            _connectionId = 0;
            _service->Release();
            _service = nullptr;
            SYSLOG(Logging::Shutdown, (string(_T("ResourceManager de-initialised"))));
        }

        string ResourceManager::Information() const
        {
            return "Plugin which exposes ResourceManager related methods.";
        }

    } // namespace Plugin
} // namespace WPEFramework