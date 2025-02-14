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

#include "../Module.h"
#include <gmock/gmock.h>

class ServiceMock : public WPEFramework::PluginHost::IShell {
public:
    ~ServiceMock() override = default;
    MOCK_METHOD(string, Versions, (), (const, override));
    MOCK_METHOD(string, Locator, (), (const, override));
    MOCK_METHOD(string, ClassName, (), (const, override));
    MOCK_METHOD(string, Callsign, (), (const, override));
    MOCK_METHOD(string, WebPrefix, (), (const, override));
    MOCK_METHOD(string, ConfigLine, (), (const, override));
    MOCK_METHOD(string, PersistentPath, (), (const, override));
    MOCK_METHOD(string, VolatilePath, (), (const, override));
    MOCK_METHOD(string, DataPath, (), (const, override));
    MOCK_METHOD(state, State, (), (const, override));
    MOCK_METHOD(bool, Resumed, (), (const, override));
    MOCK_METHOD(bool, IsSupported, (const uint8_t), (const, override));
    MOCK_METHOD(void, EnableWebServer, (const string&, const string&), (override));
    MOCK_METHOD(void, DisableWebServer, (), (override));
    MOCK_METHOD(WPEFramework::PluginHost::ISubSystem*, SubSystems, (), (override));
    MOCK_METHOD(uint32_t, Submit, (const uint32_t, const WPEFramework::Core::ProxyType<WPEFramework::Core::JSON::IElement>&), (override));
    MOCK_METHOD(void, Notify, (const string&, const string&), (override));
    MOCK_METHOD(void*, QueryInterfaceByCallsign, (const uint32_t, const string&), (override));
    MOCK_METHOD(void, Register, (WPEFramework::PluginHost::IPlugin::INotification*), (override));
    MOCK_METHOD(void, Unregister, (WPEFramework::PluginHost::IPlugin::INotification*), (override));
    MOCK_METHOD(string, Model, (), (const, override));
    MOCK_METHOD(bool, Background, (), (const, override));
    MOCK_METHOD(string, Accessor, (), (const, override));
    MOCK_METHOD(string, ProxyStubPath, (), (const, override));
    MOCK_METHOD(string, HashKey, (), (const, override));
    MOCK_METHOD(string, Substitute, (const string&), (const, override));
    MOCK_METHOD(uint32_t, Activate, (const reason), (override));
    MOCK_METHOD(uint32_t, Deactivate, (const reason), (override));
    MOCK_METHOD(uint32_t, Unavailable, (const reason), (override));
    MOCK_METHOD(reason, Reason, (), (const, override));
    MOCK_METHOD(uint32_t, ConfigLine, (const string&), (override));
    MOCK_METHOD(string, SystemRootPath, (), (const, override));
    MOCK_METHOD(uint32_t, SystemRootPath, (const string&), (override));
    MOCK_METHOD(string, SystemPath, (), (const, override));
    MOCK_METHOD(string, PluginPath, (), (const, override));
    MOCK_METHOD(WPEFramework::PluginHost::IShell::startmode, StartMode, (), (const, override));
    MOCK_METHOD(WPEFramework::Core::hresult, StartMode, (const startmode), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, Resumed, (const bool), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, Metadata, (string&), (const, override));
    MOCK_METHOD(WPEFramework::Core::hresult, Hibernate, (const uint32_t), (override));
    MOCK_METHOD(WPEFramework::RPC::IStringIterator*, GetLibrarySearchPaths, (const string&), (const, override));
    BEGIN_INTERFACE_MAP(ServiceMock)
    INTERFACE_ENTRY(IShell)
    END_INTERFACE_MAP
};
