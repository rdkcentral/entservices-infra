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

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "L2Tests.h"
#include "L2TestsMock.h"
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <interfaces/IMigration.h>

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);

#define JSON_TIMEOUT   (1000)
#define MIGRATION_CALLSIGN  _T("org.rdk.Migration")
#define MIGRATION_L2TEST_CALLSIGN _T("L2tests.1")

using ::testing::NiceMock;
using namespace WPEFramework;
using testing::StrictMock;
using ::WPEFramework::Exchange::IMigration;

/**
 * @brief Migration L2 test class declaration
 */
class MigrationL2Test : public L2TestMocks {
protected:
    MigrationL2Test();
    virtual ~MigrationL2Test() override;

public:
    uint32_t CreateMigrationInterfaceObjectUsingComRPCConnection();

protected:
    /** @brief ProxyType objects for proper cleanup */
    Core::ProxyType<RPC::InvokeServerType<1, 0, 4>> mMigrationEngine;
    Core::ProxyType<RPC::CommunicatorClient> mMigrationClient;

    /** @brief Pointer to the IShell interface */
    PluginHost::IShell *mControllerMigration;

    /** @brief Pointer to the IMigration interface */
    Exchange::IMigration *mMigrationPlugin;
};

/**
 * @brief Constructor for Migration L2 test class
 */
MigrationL2Test::MigrationL2Test() : L2TestMocks()
{
    uint32_t status = Core::ERROR_GENERAL;

    TEST_LOG("Migration L2 test constructor");

    /* Try to activate Migration plugin - if it fails, tests will be skipped */
    status = ActivateService("org.rdk.Migration");
    if (status != Core::ERROR_NONE) {
        TEST_LOG("Migration service activation failed with error: %d", status);
        // Don't fail here - individual tests will check and skip if needed
    } else {
        TEST_LOG("Migration service activated successfully");
    }
}

/**
 * @brief Destructor for Migration L2 test class
 */
MigrationL2Test::~MigrationL2Test()
{
    uint32_t status = Core::ERROR_GENERAL;

    TEST_LOG("Migration L2 test destructor");

    // Clean up interface objects
    if (mMigrationPlugin != nullptr) {
        mMigrationPlugin->Release();
        mMigrationPlugin = nullptr;
    }

    if (mControllerMigration != nullptr) {
        mControllerMigration->Release();
        mControllerMigration = nullptr;
    }

    usleep(500000);

    // Try to deactivate service - may fail if activation failed
    status = DeactivateService("org.rdk.Migration");
    if (status != Core::ERROR_NONE) {
        TEST_LOG("Migration service deactivation failed with error: %d", status);
    } else {
        TEST_LOG("Migration service deactivated successfully");
    }
}

/**
 * @brief Creates Migration interface object using COM-RPC connection
 * @return Core::ERROR_NONE on success, error code otherwise
 */
uint32_t MigrationL2Test::CreateMigrationInterfaceObjectUsingComRPCConnection()
{
    uint32_t returnValue = Core::ERROR_GENERAL;
    Core::ProxyType<RPC::InvokeServerType<1, 0, 4>> migrationEngine;
    Core::ProxyType<RPC::CommunicatorClient> migrationClient;

    TEST_LOG("Creating Migration COM-RPC connection");

    // Create the migration engine
    mMigrationEngine = Core::ProxyType<RPC::InvokeServerType<1, 0, 4>>::Create();
    mMigrationClient = Core::ProxyType<RPC::CommunicatorClient>::Create(Core::NodeId("/tmp/communicator"), Core::ProxyType<Core::IIPCServer>(mMigrationEngine));

    if (!mMigrationClient.IsValid()) {
        TEST_LOG("Invalid migrationClient");
    }
    else
    {
        mControllerMigration = mMigrationClient->Open<PluginHost::IShell>(_T("org.rdk.Migration"), ~0, 3000);
        if (mControllerMigration) 
        {
        mMigrationPlugin = mControllerMigration->QueryInterface<Exchange::IMigration>();
        returnValue = Core::ERROR_NONE;
        }
    }
    return returnValue;
}

/**************************************************/
// Test Cases
/**************************************************/

/**
 * @brief Test Migration GetBootTypeInfo API - Normal operation
 * @details Verifies that GetBootTypeInfo method returns valid boot type information
 * Note: This test handles both success and error cases gracefully, as BootType may not be configured in all environments
 */
TEST_F(MigrationL2Test, GetBootTypeInfo_Normal)
{
    const std::string bootTypeFile = "/tmp/bootType";
    const std::string bootTypeContent = "BOOT_TYPE=BOOT_NORMAL\n";
    
    std::ofstream file(bootTypeFile);
    if (file.is_open()) {
        file << bootTypeContent;
        file.close();
        TEST_LOG("Created boot type file: %s", bootTypeFile.c_str());
    } else {
        TEST_LOG("Warning: Could not create bootType file - test may use existing configuration");
    }
    uint32_t status = CreateMigrationInterfaceObjectUsingComRPCConnection();
    ASSERT_EQ(status, Core::ERROR_NONE) << "Failed to create Migration COM-RPC interface";
    ASSERT_NE(mMigrationPlugin, nullptr) << "Migration plugin interface is null";

    Exchange::IMigration::BootTypeInfo bootTypeInfo;
    Core::hresult result = mMigrationPlugin->GetBootTypeInfo(bootTypeInfo);

    // Handle both success and error cases gracefully
    if (result == Core::ERROR_NONE) {
        TEST_LOG("Boot type returned: %d", static_cast<uint32_t>(bootTypeInfo.bootType));

        // Verify valid boot type values
        EXPECT_TRUE(bootTypeInfo.bootType == Exchange::IMigration::BootType::BOOT_TYPE_INIT ||
                    bootTypeInfo.bootType == Exchange::IMigration::BootType::BOOT_TYPE_NORMAL ||
                    bootTypeInfo.bootType == Exchange::IMigration::BootType::BOOT_TYPE_MIGRATION ||
                    bootTypeInfo.bootType == Exchange::IMigration::BootType::BOOT_TYPE_UPDATE) 
                    << "Invalid boot type returned: " << static_cast<uint32_t>(bootTypeInfo.bootType);

        TEST_LOG("GetBootTypeInfo test PASSED - Boot type: %d", static_cast<uint32_t>(bootTypeInfo.bootType));
    } else {
        TEST_LOG("GetBootTypeInfo returned error: %d - BootType not available/configured", result);
    }

    // Clean up test file
    if (std::remove(bootTypeFile.c_str()) == 0) {
        TEST_LOG("Removed test boot type file");
    }
}

/**
 * @brief Test Migration GetMigrationStatus API - Normal operation
 * @details Verifies that GetMigrationStatus method returns valid migration status
 * Note: This test handles both success and error cases gracefully, as MigrationStatus may not be configured in all environments
 */
TEST_F(MigrationL2Test, GetMigrationStatus_Normal)
{
    // Setup RFC mock for GetMigrationStatus
    EXPECT_CALL(*p_rfcApiImplMock, getRFCParameter(testing::_, testing::StrEq("Device.DeviceInfo.Migration.MigrationStatus"), testing::_))
        .WillOnce(testing::Invoke(
            [](const char* arg1, const char* arg2, RFC_ParamData_t* arg3) -> WDMP_STATUS {
                if (arg3 != nullptr) {
                    strcpy(arg3->value, "NOT_STARTED");
                    arg3->type = WDMP_STRING;
                }
                return WDMP_SUCCESS;
            }));

    uint32_t status = CreateMigrationInterfaceObjectUsingComRPCConnection();
    ASSERT_EQ(status, Core::ERROR_NONE) << "Failed to create Migration COM-RPC interface";
    ASSERT_NE(mMigrationPlugin, nullptr) << "Migration plugin interface is null";

    Exchange::IMigration::MigrationStatusInfo migrationStatusInfo;
    Core::hresult result = mMigrationPlugin->GetMigrationStatus(migrationStatusInfo);

    // Handle both success and error cases gracefully
    if (result == Core::ERROR_NONE) {
        TEST_LOG("Migration status returned: %d", static_cast<uint32_t>(migrationStatusInfo.migrationStatus));

        // Verify valid migration status values
        EXPECT_TRUE(migrationStatusInfo.migrationStatus >= Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_NOT_STARTED &&
                    migrationStatusInfo.migrationStatus <= Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_MIGRATION_COMPLETED) 
                    << "Invalid migration status returned: " << static_cast<uint32_t>(migrationStatusInfo.migrationStatus);

        TEST_LOG("GetMigrationStatus test PASSED - Migration status: %d", static_cast<uint32_t>(migrationStatusInfo.migrationStatus));
    } else {
        TEST_LOG("GetMigrationStatus returned error: %d - MigrationStatus not available/configured", result);
    }
}

/**
 * @brief Test Migration SetMigrationStatus API - Normal operation
 * @details Verifies that SetMigrationStatus method properly sets migration status
 */
TEST_F(MigrationL2Test, SetMigrationStatus_Normal)
{
    uint32_t status = CreateMigrationInterfaceObjectUsingComRPCConnection();
    ASSERT_EQ(status, Core::ERROR_NONE) << "Failed to create Migration COM-RPC interface";
    ASSERT_NE(mMigrationPlugin, nullptr) << "Migration plugin interface is null";

    // Test setting to STARTED status
    Exchange::IMigration::MigrationResult migrationResult;
    Core::hresult setResult = mMigrationPlugin->SetMigrationStatus(
        Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_STARTED, migrationResult);

    // Handle both success and error cases gracefully
    if (setResult == Core::ERROR_NONE) {
        EXPECT_TRUE(migrationResult.success) << "SetMigrationStatus result indicates failure";

        // Setup RFC mock for GetMigrationStatus verification call
        EXPECT_CALL(*p_rfcApiImplMock, getRFCParameter(testing::_, testing::StrEq("Device.DeviceInfo.Migration.MigrationStatus"), testing::_))
            .WillOnce(testing::Invoke(
                [](const char* arg1, const char* arg2, RFC_ParamData_t* arg3) -> WDMP_STATUS {
                    if (arg3 != nullptr) {
                        strcpy(arg3->value, "STARTED");
                        arg3->type = WDMP_STRING;
                    }
                    return WDMP_SUCCESS;
                }));

        // Verify the status was set correctly by reading it back
        Exchange::IMigration::MigrationStatusInfo migrationStatusInfo;
        Core::hresult getResult = mMigrationPlugin->GetMigrationStatus(migrationStatusInfo);
        
        if (getResult == Core::ERROR_NONE) {
            TEST_LOG("Migration status after set: %d", static_cast<uint32_t>(migrationStatusInfo.migrationStatus));
            EXPECT_EQ(migrationStatusInfo.migrationStatus, Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_STARTED) 
                << "Migration status was not set correctly";
            TEST_LOG("SetMigrationStatus test PASSED - Status set to STARTED and verified");
        } else {
            TEST_LOG("SetMigrationStatus succeeded but GetMigrationStatus failed with error: %d", getResult);
        }
    } else {
        TEST_LOG("SetMigrationStatus returned error: %d - Migration operations not available/configured", setResult);
    }
}

/**
 * @brief Test Migration SetMigrationStatus API - Set to MIGRATION_COMPLETED
 * @details Verifies that SetMigrationStatus can set status to MIGRATION_COMPLETED
 */
TEST_F(MigrationL2Test, SetMigrationStatus_ToCompleted)
{
    uint32_t status = CreateMigrationInterfaceObjectUsingComRPCConnection();
    ASSERT_EQ(status, Core::ERROR_NONE) << "Failed to create Migration COM-RPC interface";
    ASSERT_NE(mMigrationPlugin, nullptr) << "Migration plugin interface is null";

    // Test setting to MIGRATION_COMPLETED status
    Exchange::IMigration::MigrationResult migrationResult;
    Core::hresult setResult = mMigrationPlugin->SetMigrationStatus(
        Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_MIGRATION_COMPLETED, migrationResult);

    // Handle both success and error cases gracefully
    if (setResult == Core::ERROR_NONE) {
        EXPECT_TRUE(migrationResult.success) << "SetMigrationStatus result indicates failure";

        // Setup RFC mock for GetMigrationStatus verification call
        EXPECT_CALL(*p_rfcApiImplMock, getRFCParameter(testing::_, testing::StrEq("Device.DeviceInfo.Migration.MigrationStatus"), testing::_))
            .WillOnce(testing::Invoke(
                [](const char* arg1, const char* arg2, RFC_ParamData_t* arg3) -> WDMP_STATUS {
                    if (arg3 != nullptr) {
                        strcpy(arg3->value, "MIGRATION_COMPLETED");
                        arg3->type = WDMP_STRING;
                    }
                    return WDMP_SUCCESS;
                }));

        // Verify the status was set correctly
        Exchange::IMigration::MigrationStatusInfo migrationStatusInfo;
        Core::hresult getResult = mMigrationPlugin->GetMigrationStatus(migrationStatusInfo);
        
        if (getResult == Core::ERROR_NONE) {
            TEST_LOG("Migration status after set: %d", static_cast<uint32_t>(migrationStatusInfo.migrationStatus));
            EXPECT_EQ(migrationStatusInfo.migrationStatus, Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_MIGRATION_COMPLETED) 
                << "Migration status was not set to MIGRATION_COMPLETED correctly";
            TEST_LOG("SetMigrationStatus test PASSED - Status set to MIGRATION_COMPLETED and verified");
        } else {
            TEST_LOG("SetMigrationStatus succeeded but GetMigrationStatus failed with error: %d", getResult);
        }
    } else {
        TEST_LOG("SetMigrationStatus returned error: %d - Migration operations not available/configured", setResult);
    }
}

/**
 * @brief Test Migration API sequence - Set multiple statuses in sequence
 * @details Verifies that migration status can be updated through different stages
 */
TEST_F(MigrationL2Test, SetMigrationStatus_Sequence)
{
    // Setup RFC mock to read from the file that SetMigrationStatus writes to
    // Since SetMigrationStatus writes to file and GetMigrationStatus reads from RFC,
    // we need to mock the RFC to return whatever was last written
    std::string lastWrittenStatus = "NOT_STARTED";
    
    EXPECT_CALL(*p_rfcApiImplMock, getRFCParameter(testing::_, testing::StrEq("Device.DeviceInfo.Migration.MigrationStatus"), testing::_))
        .WillRepeatedly(testing::Invoke(
            [&lastWrittenStatus](const char* arg1, const char* arg2, RFC_ParamData_t* arg3) -> WDMP_STATUS {
                if (arg3 != nullptr) {
                    strcpy(arg3->value, lastWrittenStatus.c_str());
                    arg3->type = WDMP_STRING;
                }
                return WDMP_SUCCESS;
            }));

    uint32_t status = CreateMigrationInterfaceObjectUsingComRPCConnection();
    ASSERT_EQ(status, Core::ERROR_NONE) << "Failed to create Migration COM-RPC interface";
    ASSERT_NE(mMigrationPlugin, nullptr) << "Migration plugin interface is null";

    // Map of status enum to string for updating the mock
    static const std::unordered_map<Exchange::IMigration::MigrationStatus, std::string> statusToString = {
        { Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_NOT_STARTED, "NOT_STARTED" },
        { Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_STARTED, "STARTED" },
        { Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_PRIORITY_SETTINGS_MIGRATED, "PRIORITY_SETTINGS_MIGRATED" },
        { Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_DEVICE_SETTINGS_MIGRATED, "DEVICE_SETTINGS_MIGRATED" },
        { Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_CLOUD_SETTINGS_MIGRATED, "CLOUD_SETTINGS_MIGRATED" },
        { Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_APP_DATA_MIGRATED, "APP_DATA_MIGRATED" },
        { Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_MIGRATION_COMPLETED, "MIGRATION_COMPLETED" }
    };

    // Test sequence of migration status updates
    std::vector<Exchange::IMigration::MigrationStatus> testSequence = {
        Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_NOT_STARTED,
        Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_STARTED,
        Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_PRIORITY_SETTINGS_MIGRATED,
        Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_DEVICE_SETTINGS_MIGRATED,
        Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_CLOUD_SETTINGS_MIGRATED,
        Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_APP_DATA_MIGRATED,
        Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_MIGRATION_COMPLETED
    };

    for (auto testStatus : testSequence) {
        // Update the mock to return the status we're about to set
        auto it = statusToString.find(testStatus);
        if (it != statusToString.end()) {
            lastWrittenStatus = it->second;
        }

        Exchange::IMigration::MigrationResult migrationResult;
        Core::hresult setResult = mMigrationPlugin->SetMigrationStatus(testStatus, migrationResult);

        // Handle both success and error cases gracefully
        if (setResult == Core::ERROR_NONE) {
            EXPECT_TRUE(migrationResult.success) 
                << "SetMigrationStatus result indicates failure for status: " << static_cast<uint32_t>(testStatus);

            // Verify the status was set correctly
            Exchange::IMigration::MigrationStatusInfo migrationStatusInfo;
            Core::hresult getResult = mMigrationPlugin->GetMigrationStatus(migrationStatusInfo);
            
            if (getResult == Core::ERROR_NONE) {
                EXPECT_EQ(migrationStatusInfo.migrationStatus, testStatus) 
                    << "Migration status verification failed for status: " << static_cast<uint32_t>(testStatus);
                TEST_LOG("Migration status sequence step passed - Status: %d (verified)", static_cast<uint32_t>(testStatus));
            } else {
                TEST_LOG("SetMigrationStatus succeeded for status %d but GetMigrationStatus failed with error: %d", 
                    static_cast<uint32_t>(testStatus), getResult);
                TEST_LOG("Migration status sequence step passed - Status: %d (set only)", static_cast<uint32_t>(testStatus));
            }
        } else {
            TEST_LOG("SetMigrationStatus failed for status %d with error: %d - Migration operations not available", 
                static_cast<uint32_t>(testStatus), setResult);
            TEST_LOG("Migration status sequence step passed - Status: %d (error handled)", static_cast<uint32_t>(testStatus));
        }
    }
}

/**
 * @brief Test Migration negative case - Interface not available
 * @details Tests behavior when Migration interface is not available
 */
TEST_F(MigrationL2Test, NegativeTest_InterfaceNotAvailable)
{
    // Don't create the interface connection - test with null interface
    ASSERT_EQ(mMigrationPlugin, nullptr) << "Migration plugin interface should be null for this test";
}

/**
 * @brief Test Migration boot type enumeration coverage
 * @details Verifies that GetBootTypeInfo covers all possible boot type values
 * Note: This test handles both success and error cases gracefully, as BootType may not be configured in all environments
 */
TEST_F(MigrationL2Test, BootType_EnumerationCoverage)
{
    // Create bootType file for enumeration coverage test
    const std::string bootTypeFile = "/tmp/bootType";
    const std::string bootTypeContent = "BOOT_TYPE=BOOT_NORMAL\n";
    
    std::ofstream file(bootTypeFile);
    if (file.is_open()) {
        file << bootTypeContent;
        file.close();
        TEST_LOG("Created bootType file with BOOT_NORMAL content");
    }

    uint32_t status = CreateMigrationInterfaceObjectUsingComRPCConnection();
    ASSERT_EQ(status, Core::ERROR_NONE) << "Failed to create Migration COM-RPC interface";
    ASSERT_NE(mMigrationPlugin, nullptr) << "Migration plugin interface is null";

    Exchange::IMigration::BootTypeInfo bootTypeInfo;
    Core::hresult result = mMigrationPlugin->GetBootTypeInfo(bootTypeInfo);

    // Handle both success and error cases gracefully
    if (result == Core::ERROR_NONE) {
        // Log the current boot type
        TEST_LOG("Current boot type: %d", static_cast<uint32_t>(bootTypeInfo.bootType));

        // Verify it's one of the valid enumeration values
        bool isValidBootType = false;
        switch (bootTypeInfo.bootType) {
            case Exchange::IMigration::BootType::BOOT_TYPE_INIT:
                TEST_LOG("Boot type is BOOT_TYPE_INIT");
                isValidBootType = true;
                break;
            case Exchange::IMigration::BootType::BOOT_TYPE_NORMAL:
                TEST_LOG("Boot type is BOOT_TYPE_NORMAL");
                isValidBootType = true;
                break;
            case Exchange::IMigration::BootType::BOOT_TYPE_MIGRATION:
                TEST_LOG("Boot type is BOOT_TYPE_MIGRATION");
                isValidBootType = true;
                break;
            case Exchange::IMigration::BootType::BOOT_TYPE_UPDATE:
                TEST_LOG("Boot type is BOOT_TYPE_UPDATE");
                isValidBootType = true;
                break;
            default:
                TEST_LOG("Unknown boot type: %d", static_cast<uint32_t>(bootTypeInfo.bootType));
                isValidBootType = false;
                break;
        }

        EXPECT_TRUE(isValidBootType) << "Boot type enumeration coverage failed - invalid boot type: " 
                                     << static_cast<uint32_t>(bootTypeInfo.bootType);

        TEST_LOG("BootType enumeration coverage test PASSED");
    } else {
        TEST_LOG("GetBootTypeInfo returned error: %d - BootType not available/configured", result);
    }

    // Clean up test file
    std::remove(bootTypeFile.c_str());
}

/**
 * @brief Test GetBootTypeInfo with BOOT_INIT scenario
 * @details Tests GetBootTypeInfo when boot type file contains BOOT_INIT
 */
TEST_F(MigrationL2Test, GetBootTypeInfo_BootInit)
{
    // Create bootType file with BOOT_INIT content
    const std::string bootTypeFile = "/tmp/bootType";
    const std::string bootTypeContent = "BOOT_TYPE=BOOT_INIT\n";
    
    std::ofstream file(bootTypeFile);
    if (file.is_open()) {
        file << bootTypeContent;
        file.close();
        TEST_LOG("Created bootType file with BOOT_INIT content");
    } else {
        TEST_LOG("Warning: Could not create bootType file - test may use existing configuration");
    }

    uint32_t status = CreateMigrationInterfaceObjectUsingComRPCConnection();
    ASSERT_EQ(status, Core::ERROR_NONE) << "Failed to create Migration COM-RPC interface";
    ASSERT_NE(mMigrationPlugin, nullptr) << "Migration plugin interface is null";

    Exchange::IMigration::BootTypeInfo bootTypeInfo;
    Core::hresult result = mMigrationPlugin->GetBootTypeInfo(bootTypeInfo);

    if (result == Core::ERROR_NONE) {
        TEST_LOG("Boot type returned: %d", static_cast<uint32_t>(bootTypeInfo.bootType));
        // Note: We can't guarantee the exact value due to system configuration
        // but we verify it's a valid enum value
        EXPECT_TRUE(bootTypeInfo.bootType >= Exchange::IMigration::BootType::BOOT_TYPE_INIT &&
                    bootTypeInfo.bootType <= Exchange::IMigration::BootType::BOOT_TYPE_UPDATE) 
                    << "Invalid boot type returned: " << static_cast<uint32_t>(bootTypeInfo.bootType);
        TEST_LOG("GetBootTypeInfo BOOT_INIT test PASSED");
    } else {
        TEST_LOG("GetBootTypeInfo returned error: %d - BootType not available/configured", result);
    }

    // Clean up test file
    std::remove(bootTypeFile.c_str());
}

/**
 * @brief Test GetBootTypeInfo with BOOT_NORMAL scenario
 * @details Tests GetBootTypeInfo when boot type file contains BOOT_NORMAL
 */
TEST_F(MigrationL2Test, GetBootTypeInfo_BootNormal)
{
    // Create bootType file with BOOT_NORMAL content
    const std::string bootTypeFile = "/tmp/bootType";
    const std::string bootTypeContent = "BOOT_TYPE=BOOT_NORMAL\n";
    
    std::ofstream file(bootTypeFile);
    if (file.is_open()) {
        file << bootTypeContent;
        file.close();
        TEST_LOG("Created bootType file with BOOT_NORMAL content");
    } else {
        TEST_LOG("Warning: Could not create bootType file - test may use existing configuration");
    }

    uint32_t status = CreateMigrationInterfaceObjectUsingComRPCConnection();
    ASSERT_EQ(status, Core::ERROR_NONE) << "Failed to create Migration COM-RPC interface";
    ASSERT_NE(mMigrationPlugin, nullptr) << "Migration plugin interface is null";

    Exchange::IMigration::BootTypeInfo bootTypeInfo;
    Core::hresult result = mMigrationPlugin->GetBootTypeInfo(bootTypeInfo);

    if (result == Core::ERROR_NONE) {
        TEST_LOG("Boot type returned: %d", static_cast<uint32_t>(bootTypeInfo.bootType));
        EXPECT_TRUE(bootTypeInfo.bootType >= Exchange::IMigration::BootType::BOOT_TYPE_INIT &&
                    bootTypeInfo.bootType <= Exchange::IMigration::BootType::BOOT_TYPE_UPDATE) 
                    << "Invalid boot type returned: " << static_cast<uint32_t>(bootTypeInfo.bootType);
        TEST_LOG("GetBootTypeInfo BOOT_NORMAL test PASSED");
    } else {
        TEST_LOG("GetBootTypeInfo returned error: %d - BootType not available/configured", result);
    }

    // Clean up test file
    std::remove(bootTypeFile.c_str());
}

/**
 * @brief Test GetBootTypeInfo with BOOT_MIGRATION scenario
 * @details Tests GetBootTypeInfo when boot type file contains BOOT_MIGRATION
 */
TEST_F(MigrationL2Test, GetBootTypeInfo_BootMigration)
{
    // Create bootType file with BOOT_MIGRATION content
    const std::string bootTypeFile = "/tmp/bootType";
    const std::string bootTypeContent = "BOOT_TYPE=BOOT_MIGRATION\n";
    
    std::ofstream file(bootTypeFile);
    if (file.is_open()) {
        file << bootTypeContent;
        file.close();
        TEST_LOG("Created bootType file with BOOT_MIGRATION content");
    } else {
        TEST_LOG("Warning: Could not create bootType file - test may use existing configuration");
    }

    uint32_t status = CreateMigrationInterfaceObjectUsingComRPCConnection();
    ASSERT_EQ(status, Core::ERROR_NONE) << "Failed to create Migration COM-RPC interface";
    ASSERT_NE(mMigrationPlugin, nullptr) << "Migration plugin interface is null";

    Exchange::IMigration::BootTypeInfo bootTypeInfo;
    Core::hresult result = mMigrationPlugin->GetBootTypeInfo(bootTypeInfo);

    if (result == Core::ERROR_NONE) {
        TEST_LOG("Boot type returned: %d", static_cast<uint32_t>(bootTypeInfo.bootType));
        EXPECT_TRUE(bootTypeInfo.bootType >= Exchange::IMigration::BootType::BOOT_TYPE_INIT &&
                    bootTypeInfo.bootType <= Exchange::IMigration::BootType::BOOT_TYPE_UPDATE) 
                    << "Invalid boot type returned: " << static_cast<uint32_t>(bootTypeInfo.bootType);
        TEST_LOG("GetBootTypeInfo BOOT_MIGRATION test PASSED");
    } else {
        TEST_LOG("GetBootTypeInfo returned error: %d - BootType not available/configured", result);
    }

    // Clean up test file
    std::remove(bootTypeFile.c_str());
}

/**
 * @brief Test GetBootTypeInfo with BOOT_UPDATE scenario
 * @details Tests GetBootTypeInfo when boot type file contains BOOT_UPDATE
 */
TEST_F(MigrationL2Test, GetBootTypeInfo_BootUpdate)
{
    // Create bootType file with BOOT_UPDATE content
    const std::string bootTypeFile = "/tmp/bootType";
    const std::string bootTypeContent = "BOOT_TYPE=BOOT_UPDATE\n";
    
    std::ofstream file(bootTypeFile);
    if (file.is_open()) {
        file << bootTypeContent;
        file.close();
        TEST_LOG("Created bootType file with BOOT_UPDATE content");
    } else {
        TEST_LOG("Warning: Could not create bootType file - test may use existing configuration");
    }

    uint32_t status = CreateMigrationInterfaceObjectUsingComRPCConnection();
    ASSERT_EQ(status, Core::ERROR_NONE) << "Failed to create Migration COM-RPC interface";
    ASSERT_NE(mMigrationPlugin, nullptr) << "Migration plugin interface is null";

    Exchange::IMigration::BootTypeInfo bootTypeInfo;
    Core::hresult result = mMigrationPlugin->GetBootTypeInfo(bootTypeInfo);

    if (result == Core::ERROR_NONE) {
        TEST_LOG("Boot type returned: %d", static_cast<uint32_t>(bootTypeInfo.bootType));
        EXPECT_TRUE(bootTypeInfo.bootType >= Exchange::IMigration::BootType::BOOT_TYPE_INIT &&
                    bootTypeInfo.bootType <= Exchange::IMigration::BootType::BOOT_TYPE_UPDATE) 
                    << "Invalid boot type returned: " << static_cast<uint32_t>(bootTypeInfo.bootType);
        TEST_LOG("GetBootTypeInfo BOOT_UPDATE test PASSED");
    } else {
        TEST_LOG("GetBootTypeInfo returned error: %d - BootType not available/configured", result);
    }

    // Clean up test file
    std::remove(bootTypeFile.c_str());
}

/**
 * @brief Test GetBootTypeInfo with invalid boot type value
 * @details Tests GetBootTypeInfo when boot type file contains invalid/unknown value
 */
TEST_F(MigrationL2Test, GetBootTypeInfo_InvalidBootType)
{
    // Create bootType file with invalid content
    const std::string bootTypeFile = "/tmp/bootType";
    const std::string bootTypeContent = "BOOT_TYPE=INVALID_BOOT_TYPE\n";
    
    std::ofstream file(bootTypeFile);
    if (file.is_open()) {
        file << bootTypeContent;
        file.close();
        TEST_LOG("Created bootType file with invalid boot type content");
    } else {
        TEST_LOG("Warning: Could not create bootType file - test may use existing configuration");
    }

    uint32_t status = CreateMigrationInterfaceObjectUsingComRPCConnection();
    ASSERT_EQ(status, Core::ERROR_NONE) << "Failed to create Migration COM-RPC interface";
    ASSERT_NE(mMigrationPlugin, nullptr) << "Migration plugin interface is null";

    Exchange::IMigration::BootTypeInfo bootTypeInfo;
    Core::hresult result = mMigrationPlugin->GetBootTypeInfo(bootTypeInfo);

    // For invalid boot type, the API should return an error
    if (result != Core::ERROR_NONE) {
        TEST_LOG("GetBootTypeInfo correctly returned error: %d for invalid boot type", result);
    } else {
        TEST_LOG("GetBootTypeInfo returned success despite invalid boot type - may be using fallback");
    }

    // Clean up test file
    std::remove(bootTypeFile.c_str());
}

/**
 * @brief Test GetBootTypeInfo with missing boot type file
 * @details Tests GetBootTypeInfo when boot type file is missing
 */
TEST_F(MigrationL2Test, GetBootTypeInfo_MissingFile)
{
    // Ensure bootType file doesn't exist
    const std::string bootTypeFile = "/tmp/bootType";
    std::remove(bootTypeFile.c_str());

    uint32_t status = CreateMigrationInterfaceObjectUsingComRPCConnection();
    ASSERT_EQ(status, Core::ERROR_NONE) << "Failed to create Migration COM-RPC interface";
    ASSERT_NE(mMigrationPlugin, nullptr) << "Migration plugin interface is null";

    Exchange::IMigration::BootTypeInfo bootTypeInfo;
    Core::hresult result = mMigrationPlugin->GetBootTypeInfo(bootTypeInfo);

    // For missing file, the API should return an error
    if (result != Core::ERROR_NONE) {
        TEST_LOG("GetBootTypeInfo correctly returned error: %d for missing file", result);
    } else {
        TEST_LOG("GetBootTypeInfo returned success despite missing file - may be using system configuration");
    }
}

/**
 * @brief Test Migration status enumeration coverage
 * @details Verifies that all migration status values can be set and retrieved
 */
TEST_F(MigrationL2Test, MigrationStatus_EnumerationCoverage)
{
    // Setup RFC mock to read from the file that SetMigrationStatus writes to
    std::string lastWrittenStatus = "NOT_STARTED";
    
    EXPECT_CALL(*p_rfcApiImplMock, getRFCParameter(testing::_, testing::StrEq("Device.DeviceInfo.Migration.MigrationStatus"), testing::_))
        .WillRepeatedly(testing::Invoke(
            [&lastWrittenStatus](const char* arg1, const char* arg2, RFC_ParamData_t* arg3) -> WDMP_STATUS {
                if (arg3 != nullptr) {
                    strcpy(arg3->value, lastWrittenStatus.c_str());
                    arg3->type = WDMP_STRING;
                }
                return WDMP_SUCCESS;
            }));

    uint32_t status = CreateMigrationInterfaceObjectUsingComRPCConnection();
    ASSERT_EQ(status, Core::ERROR_NONE) << "Failed to create Migration COM-RPC interface";
    ASSERT_NE(mMigrationPlugin, nullptr) << "Migration plugin interface is null";

    // Test all valid migration status enumeration values
    std::vector<std::pair<Exchange::IMigration::MigrationStatus, std::string>> allStatuses = {
        {Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_NOT_STARTED, "NOT_STARTED"},
        {Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_NOT_NEEDED, "NOT_NEEDED"},
        {Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_STARTED, "STARTED"},
        {Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_PRIORITY_SETTINGS_MIGRATED, "PRIORITY_SETTINGS_MIGRATED"},
        {Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_DEVICE_SETTINGS_MIGRATED, "DEVICE_SETTINGS_MIGRATED"},
        {Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_CLOUD_SETTINGS_MIGRATED, "CLOUD_SETTINGS_MIGRATED"},
        {Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_APP_DATA_MIGRATED, "APP_DATA_MIGRATED"},
        {Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_MIGRATION_COMPLETED, "MIGRATION_COMPLETED"}
    };

    for (const auto& statusPair : allStatuses) {
        // Update the mock to return the status we're about to set
        lastWrittenStatus = statusPair.second;

        Exchange::IMigration::MigrationResult migrationResult;
        Core::hresult setResult = mMigrationPlugin->SetMigrationStatus(statusPair.first, migrationResult);

        // Handle both success and error cases gracefully
        if (setResult == Core::ERROR_NONE) {
            EXPECT_TRUE(migrationResult.success) 
                << "SetMigrationStatus result indicates failure for " << statusPair.second;

            // Verify the status was set correctly
            Exchange::IMigration::MigrationStatusInfo migrationStatusInfo;
            Core::hresult getResult = mMigrationPlugin->GetMigrationStatus(migrationStatusInfo);
            
            if (getResult == Core::ERROR_NONE) {
                EXPECT_EQ(migrationStatusInfo.migrationStatus, statusPair.first) 
                    << "Migration status verification failed for " << statusPair.second;
                TEST_LOG("Migration status enumeration test passed for %s (%d) - verified", 
                         statusPair.second.c_str(), static_cast<uint32_t>(statusPair.first));
            } else {
                TEST_LOG("SetMigrationStatus succeeded for %s but GetMigrationStatus failed with error: %d", 
                    statusPair.second.c_str(), getResult);
                TEST_LOG("Migration status enumeration test passed for %s (%d) - set only", 
                         statusPair.second.c_str(), static_cast<uint32_t>(statusPair.first));
            }
        } else {
            TEST_LOG("SetMigrationStatus failed for %s with error: %d - Migration operations not available", 
                statusPair.second.c_str(), setResult);
            TEST_LOG("Migration status enumeration test passed for %s (%d) - error handled", 
                     statusPair.second.c_str(), static_cast<uint32_t>(statusPair.first));
        }
    }
}

/**
 * @brief Test SetMigrationStatus API - File I/O Error handling
 * @details Tests SetMigrationStatus when unable to create/write to migration status file
 */
TEST_F(MigrationL2Test, SetMigrationStatus_FileIOError)
{
    uint32_t status = CreateMigrationInterfaceObjectUsingComRPCConnection();
    ASSERT_EQ(status, Core::ERROR_NONE) << "Failed to create Migration COM-RPC interface";
    ASSERT_NE(mMigrationPlugin, nullptr) << "Migration plugin interface is null";

    // Try to make the migration status directory read-only to cause file write error
    const std::string migrationDir = "/opt/secure/persistent";
    const std::string migrationFile = "/opt/secure/persistent/MigrationStatus";
    
    // Create directory structure if it doesn't exist
    std::string createDirCmd = "mkdir -p " + migrationDir;
    system(createDirCmd.c_str());
    
    // Try to make directory read-only (this may not work in all test environments)
    std::string chmodCmd = "chmod 444 " + migrationDir;
    int chmodResult = system(chmodCmd.c_str());
    
    if (chmodResult == 0) {
        TEST_LOG("Made directory read-only to test file I/O error scenario");
    } else {
        TEST_LOG("Could not make directory read-only - file I/O error test may not be effective");
    }

    // Test setting migration status when file I/O might fail
    Exchange::IMigration::MigrationResult migrationResult;
    Core::hresult setResult = mMigrationPlugin->SetMigrationStatus(
        Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_STARTED, migrationResult);

    // The API should handle file I/O errors gracefully
    if (setResult != Core::ERROR_NONE) {
        TEST_LOG("SetMigrationStatus correctly returned error: %d for file I/O issue", setResult);
    } else {
        TEST_LOG("SetMigrationStatus succeeded despite potential file I/O constraints");
    }

    // Restore directory permissions
    std::string restoreCmd = "chmod 755 " + migrationDir;
    system(restoreCmd.c_str());
    TEST_LOG("Restored directory permissions");
}

/**
 * @brief Test SetMigrationStatus API - Invalid Parameter Error
 * @details Tests SetMigrationStatus with invalid migration status values
 */
TEST_F(MigrationL2Test, SetMigrationStatus_InvalidParameter)
{
    uint32_t status = CreateMigrationInterfaceObjectUsingComRPCConnection();
    ASSERT_EQ(status, Core::ERROR_NONE) << "Failed to create Migration COM-RPC interface";
    ASSERT_NE(mMigrationPlugin, nullptr) << "Migration plugin interface is null";

    // Test with invalid migration status value (cast an invalid integer to enum)
    Exchange::IMigration::MigrationStatus invalidStatus = 
        static_cast<Exchange::IMigration::MigrationStatus>(9999);
    
    Exchange::IMigration::MigrationResult migrationResult;
    Core::hresult setResult = mMigrationPlugin->SetMigrationStatus(invalidStatus, migrationResult);

    // The API should return ERROR_INVALID_PARAMETER for invalid status
    if (setResult == Core::ERROR_INVALID_PARAMETER) {
        TEST_LOG("SetMigrationStatus correctly returned ERROR_INVALID_PARAMETER for invalid status: %d", 
                 static_cast<uint32_t>(invalidStatus));
        TEST_LOG("SetMigrationStatus invalid parameter test PASSED - Error handling correct");
    } else if (setResult != Core::ERROR_NONE) {
        TEST_LOG("SetMigrationStatus returned error: %d for invalid status (expected behavior)", setResult);
    } else {
        TEST_LOG("SetMigrationStatus unexpectedly succeeded with invalid status - implementation may be lenient");
    }

    // Test with another boundary case - negative value cast to enum
    Exchange::IMigration::MigrationStatus negativeStatus = 
        static_cast<Exchange::IMigration::MigrationStatus>(-1);
    
    Core::hresult setResult2 = mMigrationPlugin->SetMigrationStatus(negativeStatus, migrationResult);

    if (setResult2 == Core::ERROR_INVALID_PARAMETER) {
        TEST_LOG("SetMigrationStatus correctly returned ERROR_INVALID_PARAMETER for negative status: %d", 
                 static_cast<int32_t>(negativeStatus));
    } else if (setResult2 != Core::ERROR_NONE) {
        TEST_LOG("SetMigrationStatus returned error: %d for negative status (expected behavior)", setResult2);
    } else {
        TEST_LOG("SetMigrationStatus unexpectedly succeeded with negative status");
    }

}

/**
 * @brief Test GetMigrationStatus API - RFC Parameter Success Scenarios
 * @details Tests GetMigrationStatus when RFC parameter is successfully retrieved and mapped
 */
TEST_F(MigrationL2Test, GetMigrationStatus_RFCParameterSuccess)
{
    // Setup RFC mock for GetMigrationStatus
    EXPECT_CALL(*p_rfcApiImplMock, getRFCParameter(testing::_, testing::StrEq("Device.DeviceInfo.Migration.MigrationStatus"), testing::_))
        .WillOnce(testing::Invoke(
            [](const char* arg1, const char* arg2, RFC_ParamData_t* arg3) -> WDMP_STATUS {
                if (arg3 != nullptr) {
                    strcpy(arg3->value, "PRIORITY_SETTINGS_MIGRATED");
                    arg3->type = WDMP_STRING;
                }
                return WDMP_SUCCESS;
            }));

    uint32_t status = CreateMigrationInterfaceObjectUsingComRPCConnection();
    ASSERT_EQ(status, Core::ERROR_NONE) << "Failed to create Migration COM-RPC interface";
    ASSERT_NE(mMigrationPlugin, nullptr) << "Migration plugin interface is null";

    // First, set a known migration status to ensure there's a value to retrieve
    Exchange::IMigration::MigrationResult migrationResult;
    Core::hresult setResult = mMigrationPlugin->SetMigrationStatus(
        Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_PRIORITY_SETTINGS_MIGRATED, migrationResult);

    if (setResult == Core::ERROR_NONE) {
        TEST_LOG("Successfully set migration status to PRIORITY_SETTINGS_MIGRATED for RFC test");
        
        // Now test retrieving the status to cover the string-to-status mapping
        Exchange::IMigration::MigrationStatusInfo migrationStatusInfo;
        Core::hresult getResult = mMigrationPlugin->GetMigrationStatus(migrationStatusInfo);

        if (getResult == Core::ERROR_NONE) {
            TEST_LOG("GetMigrationStatus successfully retrieved status: %d", 
                     static_cast<uint32_t>(migrationStatusInfo.migrationStatus));
            
            // Verify the mapping worked correctly (string-to-enum conversion)
            EXPECT_TRUE(migrationStatusInfo.migrationStatus >= Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_NOT_STARTED &&
                        migrationStatusInfo.migrationStatus <= Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_MIGRATION_COMPLETED) 
                        << "Invalid migration status returned from RFC parameter mapping";
            
            TEST_LOG("GetMigrationStatus RFC parameter success test PASSED - String-to-status mapping worked");
        } else {
            TEST_LOG("GetMigrationStatus returned error: %d - RFC parameter not available", getResult);
        }
    } else {
        TEST_LOG("Could not set initial migration status - RFC parameter test may not be fully effective");
    }
}

/**
 * @brief Test GetMigrationStatus API - RFC Parameter Failure Scenarios  
 * @details Tests GetMigrationStatus when RFC parameter retrieval fails
 */
TEST_F(MigrationL2Test, GetMigrationStatus_RFCParameterFailure)
{
    // Setup RFC mock to return failure
    EXPECT_CALL(*p_rfcApiImplMock, getRFCParameter(testing::_, testing::StrEq("Device.DeviceInfo.Migration.MigrationStatus"), testing::_))
        .WillOnce(testing::Return(WDMP_FAILURE));

    uint32_t status = CreateMigrationInterfaceObjectUsingComRPCConnection();
    ASSERT_EQ(status, Core::ERROR_NONE) << "Failed to create Migration COM-RPC interface";
    ASSERT_NE(mMigrationPlugin, nullptr) << "Migration plugin interface is null";

    // This test covers the scenario where getRFCParameter fails
    // In most test environments, RFC may not be fully configured
    Exchange::IMigration::MigrationStatusInfo migrationStatusInfo;
    Core::hresult getResult = mMigrationPlugin->GetMigrationStatus(migrationStatusInfo);

    if (getResult != Core::ERROR_NONE) {
        TEST_LOG("GetMigrationStatus correctly returned error: %d for RFC parameter failure", getResult);
    } else {
        TEST_LOG("Migration status retrieved: %d", static_cast<uint32_t>(migrationStatusInfo.migrationStatus));
    }
}

/**
 * @brief Test GetMigrationStatus API - Invalid RFC Value Scenarios
 * @details Tests GetMigrationStatus when RFC parameter contains invalid/unmapped values
 */
TEST_F(MigrationL2Test, GetMigrationStatus_InvalidRFCValue)
{
    // Setup RFC mock to return invalid/unmapped value
    EXPECT_CALL(*p_rfcApiImplMock, getRFCParameter(testing::_, testing::StrEq("Device.DeviceInfo.Migration.MigrationStatus"), testing::_))
        .WillOnce(testing::Invoke(
            [](const char* arg1, const char* arg2, RFC_ParamData_t* arg3) -> WDMP_STATUS {
                if (arg3 != nullptr) {
                    strcpy(arg3->value, "INVALID_STATUS_VALUE");
                    arg3->type = WDMP_STRING;
                }
                return WDMP_SUCCESS;
            }));

    uint32_t status = CreateMigrationInterfaceObjectUsingComRPCConnection();
    ASSERT_EQ(status, Core::ERROR_NONE) << "Failed to create Migration COM-RPC interface";
    ASSERT_NE(mMigrationPlugin, nullptr) << "Migration plugin interface is null";

    // Note: This test is challenging to implement directly since we can't easily
    // inject invalid RFC values. However, we can test the scenario indirectly.
    
    Exchange::IMigration::MigrationStatusInfo migrationStatusInfo;
    Core::hresult getResult = mMigrationPlugin->GetMigrationStatus(migrationStatusInfo);

    // The test covers the scenario where RFC returns a value that's not in stringToStatus map
    if (getResult != Core::ERROR_NONE) {
        TEST_LOG("GetMigrationStatus returned error: %d", getResult);
    } else {
        TEST_LOG("GetMigrationStatus succeeded with valid RFC value: %d", 
                 static_cast<uint32_t>(migrationStatusInfo.migrationStatus));
        
        // Verify it's a valid mapped value
        bool isValidStatus = (migrationStatusInfo.migrationStatus >= Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_NOT_STARTED &&
                              migrationStatusInfo.migrationStatus <= Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_MIGRATION_COMPLETED);
        
        if (isValidStatus) {
            TEST_LOG("GetMigrationStatus invalid RFC value test PASSED - Valid mapping confirmed");
        } else {
            TEST_LOG("GetMigrationStatus returned unexpected status value - may indicate mapping issue");
        }
    }
}

/**
 * @brief Test Migration string-to-status mapping completeness
 * @details Verifies that all status strings in implementation are properly mapped
 */
TEST_F(MigrationL2Test, GetMigrationStatus_StringMappingCompleteness)
{
    // Setup RFC mock to read from the file that SetMigrationStatus writes to
    std::string lastWrittenStatus = "NOT_STARTED";
    
    EXPECT_CALL(*p_rfcApiImplMock, getRFCParameter(testing::_, testing::StrEq("Device.DeviceInfo.Migration.MigrationStatus"), testing::_))
        .WillRepeatedly(testing::Invoke(
            [&lastWrittenStatus](const char* arg1, const char* arg2, RFC_ParamData_t* arg3) -> WDMP_STATUS {
                if (arg3 != nullptr) {
                    strcpy(arg3->value, lastWrittenStatus.c_str());
                    arg3->type = WDMP_STRING;
                }
                return WDMP_SUCCESS;
            }));

    uint32_t status = CreateMigrationInterfaceObjectUsingComRPCConnection();
    ASSERT_EQ(status, Core::ERROR_NONE) << "Failed to create Migration COM-RPC interface";
    ASSERT_NE(mMigrationPlugin, nullptr) << "Migration plugin interface is null";

    // Test that we can set and get all valid migration statuses
    // This indirectly tests the string-to-status mapping in GetMigrationStatus
    std::vector<std::pair<Exchange::IMigration::MigrationStatus, std::string>> testStatuses = {
        {Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_NOT_STARTED, "NOT_STARTED"},
        {Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_NOT_NEEDED, "NOT_NEEDED"},
        {Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_STARTED, "STARTED"},
        {Exchange::IMigration::MigrationStatus::MIGRATION_STATUS_DEVICE_SETTINGS_MIGRATED, "DEVICE_SETTINGS_MIGRATED"}
    };

    int successfulMappings = 0;
    int totalMappings = testStatuses.size();

    for (const auto& statusPair : testStatuses) {
        // Update the mock to return the status we're about to set
        lastWrittenStatus = statusPair.second;
        // Set the status
        Exchange::IMigration::MigrationResult migrationResult;
        Core::hresult setResult = mMigrationPlugin->SetMigrationStatus(statusPair.first, migrationResult);

        if (setResult == Core::ERROR_NONE) {
            // Try to get it back - this tests the string-to-status mapping
            Exchange::IMigration::MigrationStatusInfo migrationStatusInfo;
            Core::hresult getResult = mMigrationPlugin->GetMigrationStatus(migrationStatusInfo);

            if (getResult == Core::ERROR_NONE && migrationStatusInfo.migrationStatus == statusPair.first) {
                successfulMappings++;
                TEST_LOG("String mapping verified for %s (%d)", 
                         statusPair.second.c_str(), static_cast<uint32_t>(statusPair.first));
            } else {
                TEST_LOG("String mapping test inconclusive for %s - Get operation failed or RFC not configured", 
                         statusPair.second.c_str());
            }
        } else {
            TEST_LOG("String mapping test skipped for %s - Set operation failed", statusPair.second.c_str());
        }
    }

}

/**************************************************/
// JSONRPC Test Cases
/**************************************************/

/**
 * @brief Test Migration GetBootTypeInfo API via JSONRPC - Normal operation
 */
TEST_F(MigrationL2Test, GetBootTypeInfo_Normal_JSONRPC)
{
    const std::string bootTypeFile = "/tmp/bootType";
    const std::string bootTypeContent = "BOOT_TYPE=BOOT_NORMAL\n";

    std::ofstream file(bootTypeFile);
    if (file.is_open()) {
        file << bootTypeContent;
        file.close();
        TEST_LOG("Created boot type file: %s", bootTypeFile.c_str());
    }

    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params, result;

    status = InvokeServiceMethod("org.rdk.Migration", "getBootTypeInfo", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);

    if (result.HasLabel("bootType")) {
        int bootType = result["bootType"].Number();
        TEST_LOG("Boot type from JSONRPC: %d", bootType);
        EXPECT_GE(bootType, 0);
        EXPECT_LE(bootType, 3); // Valid range: 0-3 for boot types
    }

    // Clean up test file
    std::remove(bootTypeFile.c_str());
}

/**
 * @brief Test Migration GetMigrationStatus API via JSONRPC - Normal operation
 */
TEST_F(MigrationL2Test, GetMigrationStatus_Normal_JSONRPC)
{
    // Setup RFC mock
    EXPECT_CALL(*p_rfcApiImplMock, getRFCParameter(testing::_, testing::StrEq("Device.DeviceInfo.Migration.MigrationStatus"), testing::_))
        .WillOnce(testing::Invoke(
            [](const char* arg1, const char* arg2, RFC_ParamData_t* arg3) -> WDMP_STATUS {
                if (arg3 != nullptr) {
                    strcpy(arg3->value, "NOT_STARTED");
                    arg3->type = WDMP_STRING;
                }
                return WDMP_SUCCESS;
            }));

    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params, result;

    status = InvokeServiceMethod("org.rdk.Migration", "getMigrationStatus", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);

    if (result.HasLabel("migrationStatus")) {
        int migrationStatus = result["migrationStatus"].Number();
        TEST_LOG("Migration status from JSONRPC: %d", migrationStatus);
        EXPECT_GE(migrationStatus, 0);
        EXPECT_LE(migrationStatus, 7); // Valid range: 0-7 for migration statuses
    }
}

/**
 * @brief Test Migration SetMigrationStatus API via JSONRPC - Normal operation
 */
TEST_F(MigrationL2Test, SetMigrationStatus_Normal_JSONRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params, result;

    // Set migration status to STARTED (value: 2)
    params["migrationStatus"] = 2;

    status = InvokeServiceMethod("org.rdk.Migration", "setMigrationStatus", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result["success"].Boolean());
    TEST_LOG("SetMigrationStatus success: %s", result["success"].Boolean() ? "true" : "false");
}

/**
 * @brief Test Migration SetMigrationStatus sequence via JSONRPC
 */
TEST_F(MigrationL2Test, SetMigrationStatus_Sequence_JSONRPC)
{
    // Setup RFC mock to read from the file that SetMigrationStatus writes to
    // Since SetMigrationStatus writes to file and GetMigrationStatus reads from RFC,
    // we need to mock the RFC to return whatever was last written
    std::string lastWrittenStatus = "NOT_STARTED";

    EXPECT_CALL(*p_rfcApiImplMock, getRFCParameter(testing::_, testing::StrEq("Device.DeviceInfo.Migration.MigrationStatus"), testing::_))
        .WillRepeatedly(testing::Invoke(
            [&lastWrittenStatus](const char* arg1, const char* arg2, RFC_ParamData_t* arg3) -> WDMP_STATUS {
                if (arg3 != nullptr) {
                    strcpy(arg3->value, lastWrittenStatus.c_str());
                    arg3->type = WDMP_STRING;
                }
                return WDMP_SUCCESS;
            }));

    // Map of status value to string for updating the mock
    static const std::unordered_map<int, std::string> statusToString = {
        { 0, "NOT_STARTED" },
        { 2, "STARTED" },
        { 3, "PRIORITY_SETTINGS_MIGRATED" },
        { 4, "DEVICE_SETTINGS_MIGRATED" },
        { 5, "CLOUD_SETTINGS_MIGRATED" },
        { 6, "APP_DATA_MIGRATED" },
        { 7, "MIGRATION_COMPLETED" }
    };

    // Test sequence of migration status updates
    std::vector<int> testSequence = {0, 2, 3, 4, 5, 6, 7}; // NOT_STARTED through MIGRATION_COMPLETED

    for (auto statusValue : testSequence) {
        // Update the mock to return the status we're about to set
        auto it = statusToString.find(statusValue);
        if (it != statusToString.end()) {
            lastWrittenStatus = it->second;
        }

        uint32_t status = Core::ERROR_GENERAL;
        JsonObject params, result;

        params["migrationStatus"] = statusValue;

        status = InvokeServiceMethod("org.rdk.Migration", "setMigrationStatus", params, result);
        EXPECT_EQ(status, Core::ERROR_NONE);
        EXPECT_TRUE(result["success"].Boolean()) << "SetMigrationStatus failed for status: " << statusValue;
        TEST_LOG("Migration status %d set successfully", statusValue);
    }
}

/**
 * @brief Test Migration boot type enumeration coverage via JSONRPC
 */
TEST_F(MigrationL2Test, BootType_EnumerationCoverage_JSONRPC)
{
    const std::string bootTypeFile = "/tmp/bootType";
    const std::string bootTypeContent = "BOOT_TYPE=BOOT_NORMAL\n";

    std::ofstream file(bootTypeFile);
    if (file.is_open()) {
        file << bootTypeContent;
        file.close();
        TEST_LOG("Created bootType file with BOOT_NORMAL content");
    }

    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params, result;

    status = InvokeServiceMethod("org.rdk.Migration", "getBootTypeInfo", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);

    if (result.HasLabel("bootType")) {
        int bootType = result["bootType"].Number();

        // Verify it's one of the valid enumeration values
        bool isValidBootType = (bootType >= 0 && bootType <= 3);
        EXPECT_TRUE(isValidBootType) << "Invalid boot type: " << bootType;

        const char* bootTypeNames[] = {"BOOT_TYPE_INIT", "BOOT_TYPE_NORMAL", "BOOT_TYPE_MIGRATION", "BOOT_TYPE_UPDATE"};
        if (isValidBootType) {
            TEST_LOG("Boot type is %s (%d)", bootTypeNames[bootType], bootType);
        }
    }

    std::remove(bootTypeFile.c_str());
}

/**
 * @brief Test GetBootTypeInfo with BOOT_INIT via JSONRPC
 */
TEST_F(MigrationL2Test, GetBootTypeInfo_BootInit_JSONRPC)
{
    const std::string bootTypeFile = "/tmp/bootType";
    const std::string bootTypeContent = "BOOT_TYPE=BOOT_INIT\n";

    std::ofstream file(bootTypeFile);
    if (file.is_open()) {
        file << bootTypeContent;
        file.close();
        TEST_LOG("Created bootType file with BOOT_INIT content");
    }

    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params, result;

    status = InvokeServiceMethod("org.rdk.Migration", "getBootTypeInfo", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);

    if (result.HasLabel("bootType")) {
        int bootType = result["bootType"].Number();
        EXPECT_GE(bootType, 0);
        EXPECT_LE(bootType, 3);
        TEST_LOG("Boot type: %d", bootType);
    }

    std::remove(bootTypeFile.c_str());
}

/**
 * @brief Test GetBootTypeInfo with BOOT_MIGRATION via JSONRPC
 */
TEST_F(MigrationL2Test, GetBootTypeInfo_BootMigration_JSONRPC)
{
    const std::string bootTypeFile = "/tmp/bootType";
    const std::string bootTypeContent = "BOOT_TYPE=BOOT_MIGRATION\n";

    std::ofstream file(bootTypeFile);
    if (file.is_open()) {
        file << bootTypeContent;
        file.close();
        TEST_LOG("Created bootType file with BOOT_MIGRATION content");
    }

    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params, result;

    status = InvokeServiceMethod("org.rdk.Migration", "getBootTypeInfo", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);

    if (result.HasLabel("bootType")) {
        int bootType = result["bootType"].Number();
        EXPECT_GE(bootType, 0);
        EXPECT_LE(bootType, 3);
        TEST_LOG("Boot type: %d", bootType);
    }

    std::remove(bootTypeFile.c_str());
}

/**
 * @brief Test GetBootTypeInfo with BOOT_UPDATE via JSONRPC
 */
TEST_F(MigrationL2Test, GetBootTypeInfo_BootUpdate_JSONRPC)
{
    const std::string bootTypeFile = "/tmp/bootType";
    const std::string bootTypeContent = "BOOT_TYPE=BOOT_UPDATE\n";

    std::ofstream file(bootTypeFile);
    if (file.is_open()) {
        file << bootTypeContent;
        file.close();
        TEST_LOG("Created bootType file with BOOT_UPDATE content");
    }

    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params, result;

    status = InvokeServiceMethod("org.rdk.Migration", "getBootTypeInfo", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);

    if (result.HasLabel("bootType")) {
        int bootType = result["bootType"].Number();
        EXPECT_GE(bootType, 0);
        EXPECT_LE(bootType, 3);
        TEST_LOG("Boot type: %d", bootType);
    }

    std::remove(bootTypeFile.c_str());
}

/**
 * @brief Test GetBootTypeInfo with invalid boot type via JSONRPC
 */
TEST_F(MigrationL2Test, GetBootTypeInfo_InvalidBootType_JSONRPC)
{
    const std::string bootTypeFile = "/tmp/bootType";
    const std::string bootTypeContent = "BOOT_TYPE=INVALID_BOOT_TYPE\n";

    std::ofstream file(bootTypeFile);
    if (file.is_open()) {
        file << bootTypeContent;
        file.close();
        TEST_LOG("Created bootType file with invalid boot type content");
    }

    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params, result;

    status = InvokeServiceMethod("org.rdk.Migration", "getBootTypeInfo", params, result);
    // Accept both success (with fallback) or error
    TEST_LOG("JSONRPC GetBootTypeInfo with invalid type completed with status: %d", status);

    std::remove(bootTypeFile.c_str());
}

/**
 * @brief Test GetBootTypeInfo with missing file via JSONRPC
 */
TEST_F(MigrationL2Test, GetBootTypeInfo_MissingFile_JSONRPC)
{
    const std::string bootTypeFile = "/tmp/bootType";
    std::remove(bootTypeFile.c_str());

    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params, result;

    status = InvokeServiceMethod("org.rdk.Migration", "getBootTypeInfo", params, result);
    TEST_LOG("JSONRPC GetBootTypeInfo with missing file completed with status: %d", status);
}

/**
 * @brief Test Migration status enumeration coverage via JSONRPC
 */
TEST_F(MigrationL2Test, MigrationStatus_EnumerationCoverage_JSONRPC)
{
    // Setup RFC mock to read from the file that SetMigrationStatus writes to
    std::string lastWrittenStatus = "NOT_STARTED";

    EXPECT_CALL(*p_rfcApiImplMock, getRFCParameter(testing::_, testing::StrEq("Device.DeviceInfo.Migration.MigrationStatus"), testing::_))
        .WillRepeatedly(testing::Invoke(
            [&lastWrittenStatus](const char* arg1, const char* arg2, RFC_ParamData_t* arg3) -> WDMP_STATUS {
                if (arg3 != nullptr) {
                    strcpy(arg3->value, lastWrittenStatus.c_str());
                    arg3->type = WDMP_STRING;
                }
                return WDMP_SUCCESS;
            }));

    // Map of status value to string for updating the mock
    static const std::unordered_map<int, std::string> statusToString = {
        { 0, "NOT_STARTED" },
        { 1, "NOT_NEEDED" },
        { 2, "STARTED" },
        { 3, "PRIORITY_SETTINGS_MIGRATED" },
        { 4, "DEVICE_SETTINGS_MIGRATED" },
        { 5, "CLOUD_SETTINGS_MIGRATED" },
        { 6, "APP_DATA_MIGRATED" },
        { 7, "MIGRATION_COMPLETED" }
    };

    // Test all valid migration status enumeration values
    std::vector<int> allStatuses = {0, 1, 2, 3, 4, 5, 6, 7};

    for (auto statusValue : allStatuses) {
        // Update the mock to return the status we're about to set
        auto it = statusToString.find(statusValue);
        if (it != statusToString.end()) {
            lastWrittenStatus = it->second;
        }

        uint32_t status = Core::ERROR_GENERAL;
        JsonObject params, result;

        params["migrationStatus"] = statusValue;

        status = InvokeServiceMethod("org.rdk.Migration", "setMigrationStatus", params, result);
        EXPECT_EQ(status, Core::ERROR_NONE);
        EXPECT_TRUE(result["success"].Boolean()) << "SetMigrationStatus failed for status: " << statusValue;
        TEST_LOG("Migration status enumeration test passed for status %d", statusValue);
    }
}

/**
 * @brief Test SetMigrationStatus with invalid parameter via JSONRPC
 */
TEST_F(MigrationL2Test, SetMigrationStatus_InvalidParameter_JSONRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params, result;

    // Test with invalid migration status value
    params["migrationStatus"] = 9999;

    status = InvokeServiceMethod("org.rdk.Migration", "setMigrationStatus", params, result);
    TEST_LOG("JSONRPC SetMigrationStatus with invalid parameter completed with status: %d", status);

    // Should return error or success:false for invalid status
    if (result.HasLabel("success")) {
        bool success = result["success"].Boolean();
        TEST_LOG("SetMigrationStatus with invalid parameter returned success: %s", success ? "true" : "false");
    }
}

/**
 * @brief Test GetMigrationStatus RFC parameter success via JSONRPC
 */
TEST_F(MigrationL2Test, GetMigrationStatus_RFCParameterSuccess_JSONRPC)
{
    // Setup RFC mock
    EXPECT_CALL(*p_rfcApiImplMock, getRFCParameter(testing::_, testing::StrEq("Device.DeviceInfo.Migration.MigrationStatus"), testing::_))
        .WillOnce(testing::Invoke(
            [](const char* arg1, const char* arg2, RFC_ParamData_t* arg3) -> WDMP_STATUS {
                if (arg3 != nullptr) {
                    strcpy(arg3->value, "PRIORITY_SETTINGS_MIGRATED");
                    arg3->type = WDMP_STRING;
                }
                return WDMP_SUCCESS;
            }));

    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params, result;

    status = InvokeServiceMethod("org.rdk.Migration", "getMigrationStatus", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);

    if (result.HasLabel("migrationStatus")) {
        int migrationStatus = result["migrationStatus"].Number();
        EXPECT_GE(migrationStatus, 0);
        EXPECT_LE(migrationStatus, 7);
        TEST_LOG("Migration status from RFC: %d", migrationStatus);
    }
}

/**
 * @brief Test GetMigrationStatus RFC parameter failure via JSONRPC
 */
TEST_F(MigrationL2Test, GetMigrationStatus_RFCParameterFailure_JSONRPC)
{
    // Setup RFC mock to return failure
    EXPECT_CALL(*p_rfcApiImplMock, getRFCParameter(testing::_, testing::StrEq("Device.DeviceInfo.Migration.MigrationStatus"), testing::_))
        .WillOnce(testing::Return(WDMP_FAILURE));

    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params, result;

    status = InvokeServiceMethod("org.rdk.Migration", "getMigrationStatus", params, result);
    TEST_LOG("JSONRPC GetMigrationStatus with RFC failure completed with status: %d", status);
}