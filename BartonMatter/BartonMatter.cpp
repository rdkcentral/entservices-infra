/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
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
 */

#include "BartonMatter.h"

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 0

namespace WPEFramework
{
	namespace {
		static Plugin::Metadata<Plugin::BartonMatter> metadata(
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

	namespace Plugin
	{
		/*
		 * Register BartonMatter module as wpeframework plugin
		 **/
		SERVICE_REGISTRATION(BartonMatter, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

		BartonMatter::BartonMatter(): _service(nullptr), _connectionId(0), _bartonMatter(nullptr)
		{
			SYSLOG(Logging::Startup, (_T("BartonMatter Constructor")));
		}
		BartonMatter::~BartonMatter()
		{
			SYSLOG(Logging::Shutdown, (string(_T("BartonMatter Destructor"))));
		}

		const string BartonMatter::Initialize(PluginHost::IShell* service)
		{
			string message="";
			ASSERT(nullptr != service);
			ASSERT(nullptr == _service);
			ASSERT(nullptr == _bartonMatter);
			ASSERT(0 == _connectionId);
			SYSLOG(Logging::Startup, (_T("BartonMatter::Initialize: PID=%u"), getpid()));
			_service = service;
			_service->AddRef();
			_bartonMatter = _service->Root<Exchange::IBartonMatter>(_connectionId, 5000, _T("BartonMatterImplementation"));

			if(nullptr != _bartonMatter)
			{
				Exchange::JBartonMatter::Register(*this, _bartonMatter);
			}
			else
			{
				SYSLOG(Logging::Startup, (_T("BartonMatter::Initialize: Failed to initialise BartonMatter plugin")));
				message = _T("BartonMatter plugin could not be initialised");
			}

			if (0 != message.length())
			{
				Deinitialize(service);
			}

			return message;
		}

		void BartonMatter::Deinitialize(PluginHost::IShell* service)
		{
			ASSERT(_service == service);

			SYSLOG(Logging::Shutdown, (string(_T("BartonMatter::Deinitialize"))));

			if (nullptr != _bartonMatter)
			{
				Exchange::JBartonMatter::Unregister(*this);

				// Stop processing:
				RPC::IRemoteConnection* connection = service->RemoteConnection(_connectionId);
				VARIABLE_IS_NOT_USED uint32_t result = _bartonMatter->Release();

				_bartonMatter = nullptr;
				ASSERT(result == Core::ERROR_DESTRUCTION_SUCCEEDED);

				// If this was running in a (container) process...
				if (nullptr != connection)
				{
					// Lets trigger the cleanup sequence for
					// out-of-process code. Which will guard
					// that unwilling processes, get shot if
					// not stopped friendly :-)
					connection->Terminate();
					connection->Release();
				}
			}

			_connectionId = 0;
			_service->Release();
			_service = nullptr;
			SYSLOG(Logging::Shutdown, (string(_T("BartonMatter de-initialised"))));
		}

		string BartonMatter::Information() const
		{
			// No additional info to report
			return (string());
		}

		void BartonMatter::Deactivated(RPC::IRemoteConnection* connection)
		{
			if (connection->Id() == _connectionId) {
				ASSERT(nullptr != _service);
				Core::IWorkerPool::Instance().Submit(PluginHost::IShell::Job::Create(_service, PluginHost::IShell::DEACTIVATED, PluginHost::IShell::FAILURE));
			}
		}


	} //namespace Plugin
}// namespace WPEFramework
