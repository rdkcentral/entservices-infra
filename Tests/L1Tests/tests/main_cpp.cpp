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
using ::testing::DoAll;
using ::testing::SetArgReferee;

// Global pointers that will be used by the tests
WPEFramework::Exchange::IUserSettings *InterfacePointer = nullptr;
WPEFramework::Exchange::IUserSettingsInspector *IUserSettingsInspectorPointer = nullptr;

// Global mocks
NiceMock<ServiceMock>* g_serviceMock = nullptr;
Store2Mock* g_storeMock = nullptr;
WPEFramework::Core::ProxyType<WPEFramework::Plugin::UserSettingsImplementation> g_userSettingsImpl;

// Global setup that runs once before all tests
void GlobalSetup() {
    using namespace WPEFramework;
    using namespace WPEFramework::Plugin;
    
    // Create the mocks
    g_serviceMock = new NiceMock<ServiceMock>();
    g_storeMock = new Store2Mock();
    
    // Set up the default mock expectations
    ON_CALL(*g_serviceMock, QueryInterfaceByCallsign(_, _))
        .WillByDefault(Return(g_storeMock));
    
    ON_CALL(*g_storeMock, GetValue(_, _, _, _, _))
        .WillByDefault(Return(Core::ERROR_NONE));
    
    ON_CALL(*g_storeMock, SetValue(_, _, _, _, _))
        .WillByDefault(Return(Core::ERROR_NONE));
    
    // Create the implementation with mocked dependencies
    g_userSettingsImpl = Core::ProxyType<UserSettingsImplementation>::Create();
    g_userSettingsImpl->Configure(g_serviceMock);

    // Set the global pointers for the tests
    InterfacePointer = g_userSettingsImpl.operator->();
    IUserSettingsInspectorPointer = g_userSettingsImpl.operator->();
}

// Global teardown that runs once after all tests
void GlobalTeardown() {
    // Clean up global pointers
    InterfacePointer = nullptr;
    IUserSettingsInspectorPointer = nullptr;
    
    // Clean up mocks
    delete g_storeMock;
    g_storeMock = nullptr;
    
    delete g_serviceMock;
    g_serviceMock = nullptr;
    
    // ProxyType will clean up automatically when the program exits
    // No need to explicitly reset g_userSettingsImpl
}

// Main function that sets up the environment and runs the tests
int main(int argc, char **argv) {
    std::cout << "========== STARTING L1 TESTS WITH MOCKED STORE ==========" << std::endl;
    
    // Initialize Google Test
    ::testing::InitGoogleTest(&argc, argv);
    
    // Set up our global test environment
    GlobalSetup();
    
    // Run the tests
    int result = RUN_ALL_TESTS();
    
    // Clean up
    GlobalTeardown();
    
    std::cout << "========== L1 TESTS COMPLETE ==========" << std::endl;
    return result;
}
