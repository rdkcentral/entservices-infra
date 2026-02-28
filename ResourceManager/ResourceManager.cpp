/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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
 */

#include "ResourceManager.h"
#include "Module.h"
#include <interfaces/IResourceManager.h>
#include <interfaces/json/JResourceManager.h>

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 0

namespace WPEFramework
{
    namespace Plugin
    {
        namespace {
            static Metadata<ResourceManager> metadata(
                // Version (Major, Minor, Patch)
                API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH,
                // Preconditions
                {},
                // Terminations
                {},
                // Controls
                {});
        }

        /*
         *Register ResourceManager module as wpeframework plugin
         */
        SERVICE_REGISTRATION(ResourceManager, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

        ResourceManager::ResourceManager()
            : _service(nullptr)
              , _connectionId(0)
              , _resourceManager(nullptr)
              , _resourceManagerNotification(this)
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
            _service->Register(&_resourceManagerNotification);
            
            _resourceManager = _service->Root<Exchange::IResourceManager>(_connectionId, 5000, _T("ResourceManagerImplementation"));

            if (nullptr != _resourceManager)
            {
                // Invoking Plugin API register to wpeframework
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

            // Make sure the Activated and Deactivated are no longer called before we start cleaning up..
            _service->Unregister(&_resourceManagerNotification);

            if (nullptr != _resourceManager)
            {
                Exchange::JResourceManager::Unregister(*this);

                // Stop processing:
                RPC::IRemoteConnection* connection = service->RemoteConnection(_connectionId);
                VARIABLE_IS_NOT_USED uint32_t result = _resourceManager->Release();
                _resourceManager = nullptr;

                // It should have been the last reference we are releasing,
                // so it should endup in a DESTRUCTION_SUCCEEDED, if not we
                // are leaking...
                ASSERT(result == Core::ERROR_DESTRUCTION_SUCCEEDED);

                // If this was running in a (container) process...
                if (connection != nullptr)
                {
                    // Lets trigger the cleanup sequence for
                    // out-of-process code. Which will guard
                    // that unwilling processes, get shot if
                    // not stopped friendly :-)
                    try
                    {
                        connection->Terminate();
                        TRACE(Trace::Warning, (_T("Connection terminated successfully")));
                    }
                    catch (const std::exception& e)
                    {
                        std::string errorMessage = "Failed to terminate connection: ";
                        errorMessage += e.what();
                        TRACE(Trace::Warning, (_T("%s"), errorMessage.c_str()));
                    }
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

        void ResourceManager::Deactivated(RPC::IRemoteConnection* connection)
        {
            if (connection->Id() == _connectionId) {
                ASSERT(nullptr != _service);
                Core::IWorkerPool::Instance().Submit(PluginHost::IShell::Job::Create(_service, PluginHost::IShell::DEACTIVATED, PluginHost::IShell::FAILURE));
            }
        }

    } // namespace Plugin
} // namespace WPEFramework
