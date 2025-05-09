/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
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
*/

#include "USBDevice.h"

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 0

namespace WPEFramework
{

    namespace {

        static Plugin::Metadata<Plugin::USBDevice> metadata(
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
     *Register USBDevice module as wpeframework plugin
     **/
    SERVICE_REGISTRATION(USBDevice, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

    USBDevice::USBDevice() : _service(nullptr), _connectionId(0), _usbDeviceImpl(nullptr), _usbDeviceNotification(this)
    {
        SYSLOG(Logging::Startup, (_T("USBDevice Constructor")));
    }

    USBDevice::~USBDevice()
    {
        SYSLOG(Logging::Shutdown, (string(_T("USBDevice Destructor"))));
    }

    const string USBDevice::Initialize(PluginHost::IShell* service)
    {
        string message="";

        ASSERT(nullptr != service);
        ASSERT(nullptr == _service);
        ASSERT(nullptr == _usbDeviceImpl);
        ASSERT(0 == _connectionId);

        SYSLOG(Logging::Startup, (_T("USBDevice::Initialize: PID=%u"), getpid()));

        _service = service;
        _service->AddRef();
        _service->Register(&_usbDeviceNotification);
        _usbDeviceImpl = _service->Root<Exchange::IUSBDevice>(_connectionId, 5000, _T("USBDeviceImplementation"));

        if(nullptr != _usbDeviceImpl)
        {
            // Register for notifications
            _usbDeviceImpl->Register(&_usbDeviceNotification);
            // Invoking Plugin API register to wpeframework
            Exchange::JUSBDevice::Register(*this, _usbDeviceImpl);
        }
        else
        {
            SYSLOG(Logging::Startup, (_T("USBDevice::Initialize: Failed to initialise USBDevice plugin")));
            message = _T("USBDevice plugin could not be initialised");
        }

        if (0 != message.length())
        {
           Deinitialize(service);
        }

        return message;
    }

    void USBDevice::Deinitialize(PluginHost::IShell* service)
    {
        ASSERT(_service == service);

        SYSLOG(Logging::Shutdown, (string(_T("USBDevice::Deinitialize"))));

        if (nullptr != _usbDeviceImpl)
        {
            _usbDeviceImpl->Unregister(&_usbDeviceNotification);
            Exchange::JUSBDevice::Unregister(*this);

            // Stop processing:
            RPC::IRemoteConnection* connection = service->RemoteConnection(_connectionId);
            VARIABLE_IS_NOT_USED uint32_t result = _usbDeviceImpl->Release();

            _usbDeviceImpl = nullptr;

            // It should have been the last reference we are releasing,
            // so it should endup in a DESTRUCTION_SUCCEEDED, if not we
            // are leaking...
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

        if(nullptr != _service)
        {
            // Make sure the Activated and Deactivated are no longer called before we start cleaning up..
            _service->Unregister(&_usbDeviceNotification);
            _service->Release();
            _service = nullptr;
        }
        _connectionId = 0;
        SYSLOG(Logging::Shutdown, (string(_T("USBDevice de-initialised"))));
    }

    string USBDevice::Information() const
    {
        SYSLOG(Logging::Shutdown, (string(_T("USBDevice Information"))));
       // No additional info to report
       return (string());
    }

    void USBDevice::Deactivated(RPC::IRemoteConnection* connection)
    {
        if (connection->Id() == _connectionId)
        {
            SYSLOG(Logging::Shutdown, (string(_T("USBDevice Deactivated"))));
            ASSERT(nullptr != _service)
            Core::IWorkerPool::Instance().Submit(PluginHost::IShell::Job::Create(_service, PluginHost::IShell::DEACTIVATED, PluginHost::IShell::FAILURE));
        }
    }

} // namespace Plugin
} // namespace WPEFramework
