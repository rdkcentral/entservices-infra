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
distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "UserSettingsImplementation.h"
#include <WPEFramework/interfaces/IUserSettings.h>
#include <WPEFramework/interfaces/Ids.h>
#include "ServiceMock.h"
#include "Store2Mock.h"

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::_;

WPEFramework::Exchange::IUserSettings *InterfacePointer = nullptr;
WPEFramework::Exchange::IUserSettingsInspector *IUserSettingsInspectorPointer = nullptr;
Store2Mock* g_storeMock = nullptr; // Global store mock pointer for tests to use

int main(int argc, char **argv) {
    using namespace WPEFramework;
    using namespace WPEFramework::Plugin;
    
    // Create the mocks
    NiceMock<ServiceMock>* serviceMock = new NiceMock<ServiceMock>();
    g_storeMock = new NiceMock<Store2Mock>();
    
    // Set up the mock expectations
    ON_CALL(*serviceMock, QueryInterfaceByCallsign(_, _))
        .WillByDefault(Return(g_storeMock));
    
    // Set default behaviors that individual tests can override with specific expectations
    ON_CALL(*g_storeMock, GetValue(_, _, _, _, _))
        .WillByDefault(Return(Core::ERROR_NONE));
    
    ON_CALL(*g_storeMock, SetValue(_, _, _, _, _))
        .WillByDefault(Return(Core::ERROR_NONE));
    
    // Create the implementation with mocked dependencies
    Core::ProxyType<UserSettingsImplementation> userSettingsImpl = Core::ProxyType<UserSettingsImplementation>::Create();
    userSettingsImpl->Configure(serviceMock);

    // Set the global pointers for the tests
    InterfacePointer = userSettingsImpl.operator->();
    IUserSettingsInspectorPointer = userSettingsImpl.operator->();
    
    std::cout << "========== STARTING L1 TESTS WITH MOCKED STORE ==========" << std::endl;

    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();
    
    std::cout << "========== L1 TESTS COMPLETE ==========" << std::endl;
    
    // Clean up
    // ProxyType will handle deletion of userSettingsImpl
    
    return result;
}
