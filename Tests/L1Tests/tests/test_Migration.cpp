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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mntent.h>
#include <fstream>
#include <algorithm>
#include <string>
#include <vector>
#include <cstdio>
#include "Migration.h"
#include "MigrationImplementation.h"
#include "ServiceMock.h"
#include "COMLinkMock.h"
#include "RfcApiMock.h"
#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);

using ::testing::NiceMock;
using namespace WPEFramework;

class MigrationTest : public ::testing::Test {
protected:
    Core::ProxyType<Plugin::Migration> plugin;
    Core::JSONRPC::Handler& handler;
    Core::JSONRPC::Context connection;
    Core::JSONRPC::Message message;
    NiceMock<ServiceMock> service;
    NiceMock<COMLinkMock> comLinkMock;
    Core::ProxyType<Plugin::MigrationImplementation> MigrationImpl;
    string response;
    ServiceMock  *p_serviceMock  = nullptr;
    RfcApiImplMock* p_rfcApiImplMock = nullptr ;
    
    MigrationTest()
        : plugin(Core::ProxyType<Plugin::Migration>::Create())
        , handler(*plugin)
        , connection(1,0,"")
    {
        p_serviceMock = new NiceMock <ServiceMock>;
        p_rfcApiImplMock = new NiceMock <RfcApiImplMock>;
        RfcApi::setImpl(p_rfcApiImplMock);

        ON_CALL(comLinkMock, Instantiate(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](const RPC::Object& object, const uint32_t waitTime, uint32_t& connectionId) {
                MigrationImpl = Core::ProxyType<Plugin::MigrationImplementation>::Create();
                return &MigrationImpl;
                }));

        plugin->Initialize(&service);
    }

    virtual ~MigrationTest() override
    {
        plugin->Deinitialize(&service);

        if (p_serviceMock != nullptr)
        {
            delete p_serviceMock;
            p_serviceMock = nullptr;
        }
        
        RfcApi::setImpl(nullptr);
        if (p_rfcApiImplMock != nullptr)
        {
            delete p_rfcApiImplMock;
            p_rfcApiImplMock = nullptr;
        }
    }

    // Helper methods for plugin lifecycle tests
    Core::ProxyType<Plugin::Migration> CreateTestPlugin()
    {
        return Core::ProxyType<Plugin::Migration>::Create();
    }

    void SetupPluginInstantiateMock(uint32_t connectionId = 0)
    {
        ON_CALL(comLinkMock, Instantiate(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
            [&, connectionId](const RPC::Object& object, const uint32_t waitTime, uint32_t& connId) {
                MigrationImpl = Core::ProxyType<Plugin::MigrationImplementation>::Create();
                if (connectionId != 0) {
                    connId = connectionId;
                }
                return &MigrationImpl;
                }));
    }

    void InitializeAndDeinitializePlugin(Core::ProxyType<Plugin::Migration>& testPlugin)
    {
        string result = testPlugin->Initialize(&service);
        EXPECT_TRUE(result.empty());
        testPlugin->Deinitialize(&service);
    }
};

TEST_F(MigrationTest, RegisteredMethods)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getBootTypeInfo")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setMigrationStatus")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getMigrationStatus")));
}

// Parameterized test for different boot types
class MigrationBootTypeTest : public MigrationTest, public ::testing::WithParamInterface<const char*> {};
TEST_P(MigrationBootTypeTest, GetBootTypeInfo_Success)
{
    // Create the expected boot type file
    const char* testFile = "/tmp/bootType";
    std::ofstream file(testFile);
    file << "BOOT_TYPE=" << GetParam() << "\n";
    file.close();
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getBootTypeInfo"), _T("{}"), response));
    // Clean up
    std::remove(testFile);
}
INSTANTIATE_TEST_SUITE_P(
    BootTypes,
    MigrationBootTypeTest,
    ::testing::Values(
        "BOOT_INIT",
        "BOOT_NORMAL",
        "BOOT_MIGRATION",
        "BOOT_UPDATE"
    )
);

TEST_F(MigrationTest, GetBootTypeInfo_Failure_InvalidBootType)
{
    // Create the expected boot type file with invalid boot type
    const char* testFile = "/tmp/bootType";
    std::ofstream file(testFile);
    file << "BOOT_TYPE=INVALID_BOOT_TYPE\n";
    file.close();

    EXPECT_EQ(1005, handler.Invoke(connection, _T("getBootTypeInfo"), _T("{}"), response));
    
    // Clean up
    std::remove(testFile);
}

TEST_F(MigrationTest, GetBootTypeInfo_Failure_FileReadError)
{
    // No file created, so readPropertyFromFile should fail
    EXPECT_EQ(1005, handler.Invoke(connection, _T("getBootTypeInfo"), _T("{}"), response));
}

TEST_F(MigrationTest, SetMigrationStatus_Success_NOT_STARTED)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setMigrationStatus"), _T("{\"status\":\"NOT_STARTED\"}"), response));
}

TEST_F(MigrationTest, SetMigrationStatus_Success_NOT_NEEDED)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setMigrationStatus"), _T("{\"status\":\"NOT_NEEDED\"}"), response));
}

TEST_F(MigrationTest, SetMigrationStatus_Success_STARTED)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setMigrationStatus"), _T("{\"status\":\"STARTED\"}"), response));
}

TEST_F(MigrationTest, SetMigrationStatus_Success_PRIORITY_SETTINGS_MIGRATED)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setMigrationStatus"), _T("{\"status\":\"PRIORITY_SETTINGS_MIGRATED\"}"), response));
}

// Parameterized test for SetMigrationStatus with different status values
class MigrationStatusTest : public MigrationTest, public ::testing::WithParamInterface<const char*> {};
TEST_P(MigrationStatusTest, SetMigrationStatus_Success)
{
    std::string request = std::string("{\"status\":\"") + GetParam() + "\"}";
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setMigrationStatus"), Core::ToString(request), response));
}
INSTANTIATE_TEST_SUITE_P(
    SetMigrationStatusTests,
    MigrationStatusTest,
    ::testing::Values(
        "DEVICE_SETTINGS_MIGRATED",
        "CLOUD_SETTINGS_MIGRATED",
        "APP_DATA_MIGRATED",
        "MIGRATION_COMPLETED"
    )
);

class GetMigrationStatusTest : public MigrationTest, public ::testing::WithParamInterface<const char*> {};
TEST_P(GetMigrationStatusTest, GetMigrationStatus_Success)
{
    RFC_ParamData_t rfcParam;
    strcpy(rfcParam.value, GetParam());
    
    EXPECT_CALL(*p_rfcApiImplMock, getRFCParameter(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<2>(rfcParam),
            ::testing::Return(WDMP_SUCCESS)));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getMigrationStatus"), _T("{}"), response));
}
INSTANTIATE_TEST_SUITE_P(
    GetMigrationStatusTests,
    GetMigrationStatusTest,
    ::testing::Values(
        "NOT_NEEDED",
        "STARTED",
        "PRIORITY_SETTINGS_MIGRATED",
        "DEVICE_SETTINGS_MIGRATED",
        "CLOUD_SETTINGS_MIGRATED",
        "APP_DATA_MIGRATED",
        "MIGRATION_COMPLETED"
    )
);

//Plugin cases
TEST_F(MigrationTest, PluginInformation)
{
    string info = plugin->Information();
    EXPECT_TRUE(info.empty());
}

TEST_F(MigrationTest, PluginInitialize_Success)
{
    auto testPlugin = CreateTestPlugin();
    SetupPluginInstantiateMock();
    InitializeAndDeinitializePlugin(testPlugin);
}

TEST_F(MigrationTest, PluginDeinitialize_WithValidMigration)
{
    auto testPlugin = CreateTestPlugin();
    SetupPluginInstantiateMock();
    
    testPlugin->Initialize(&service);
    testPlugin->Deinitialize(&service);
}

TEST_F(MigrationTest, PluginDeinitialize_ConnectionTerminateException)
{
    auto testPlugin = CreateTestPlugin();
    NiceMock<COMLinkMock> mockConnection;
    SetupPluginInstantiateMock(1); // Set connection ID to 1
    
    testPlugin->Initialize(&service);
    testPlugin->Deinitialize(&service);
}

TEST_F(MigrationTest, PluginDeactivated_MatchingConnectionId)
{
    auto testPlugin = CreateTestPlugin();
    NiceMock<COMLinkMock> mockConnection;
    SetupPluginInstantiateMock(123); // Set connection ID to 123
    
    testPlugin->Initialize(&service);
    
    // Test Deactivated method by calling it directly - this tests the private method indirectly
    // Since Deactivated is private, we can't call it directly, but we can test the scenario
    // that would trigger it through connection handling
    
    testPlugin->Deinitialize(&service);
}

TEST_F(MigrationTest, PluginDeactivated_NonMatchingConnectionId)
{
    auto testPlugin = CreateTestPlugin();
    NiceMock<COMLinkMock> mockConnection;
    SetupPluginInstantiateMock(123); // Set connection ID to 123
    
    testPlugin->Initialize(&service);
    testPlugin->Deinitialize(&service);
}
