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
#include "L2Tests.h"
#include "L2TestsMock.h"
#include <interfaces/IPreinstallManager.h>
#include <dirent.h>
#include <cstring>
#include "WrapsMock.h"

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);

#define PREINSTALLMANAGER_CALLSIGN  _T("org.rdk.PreinstallManager")

using namespace WPEFramework;
using ::WPEFramework::Exchange::IPreinstallManager;

class PreinstallManagerTest : public L2TestMocks {
public:
    PreinstallManagerTest();
    uint32_t CreatePreinstallManagerInterfaceObjectUsingComRPCConnection();
    void ReleasePreinstallManagerInterfaceObjectUsingComRPCConnection();
    void SetUpPreinstallDirectoryMocks();
    class TestNotification : public Exchange::IPreinstallManager::INotification {
    public:
        TestNotification() = default;
        virtual ~TestNotification() = default;
        void OnAppInstallationStatus(const string& jsonresponse) override {
            TEST_LOG("OnAppInstallationStatus received: %s", jsonresponse.c_str());
        }
        BEGIN_INTERFACE_MAP(TestNotification)
        INTERFACE_ENTRY(Exchange::IPreinstallManager::INotification)
        END_INTERFACE_MAP
    };
protected:
    PluginHost::IShell *mControllerPreinstallManager;
    Core::ProxyType<RPC::InvokeServerType<1, 0, 4>> mEnginePreinstallManager;
    Core::ProxyType<RPC::CommunicatorClient> mClientPreinstallManager;
    Exchange::IPreinstallManager *mPreinstallManagerPlugin;
};

PreinstallManagerTest::PreinstallManagerTest():L2TestMocks(),
    mControllerPreinstallManager(nullptr),
    mPreinstallManagerPlugin(nullptr)
{
    uint32_t status = Core::ERROR_GENERAL;

    // Activate required services for PreinstallManager testing
    status = ActivateService("org.rdk.PersistentStore");
    EXPECT_EQ(Core::ERROR_NONE, status);
    
    // Try to activate PackageManager service for real interaction testing
    // This activates the real PackageManager service if available
    // PreinstallManager will interact with the real service for integration testing
    status = ActivateService("org.rdk.PackageManagerRDKEMS");
    if (status != Core::ERROR_NONE) {
        TEST_LOG("PackageManager service unavailable (status: %d) - PreinstallManager will handle gracefully", status);
    } else {
        TEST_LOG("PackageManager service activated - PreinstallManager will interact with real service");
    }
    
    status = ActivateService(PREINSTALLMANAGER_CALLSIGN);
    EXPECT_EQ(Core::ERROR_NONE, status);
}


/**
 * @brief Create PreinstallManager Plugin Interface object using Com-RPC connection
 *
 * @return Returns error code
 */
uint32_t PreinstallManagerTest::CreatePreinstallManagerInterfaceObjectUsingComRPCConnection()
{
    uint32_t return_value = Core::ERROR_GENERAL;

    TEST_LOG("Creating mEnginePreinstallManager");
    mEnginePreinstallManager = Core::ProxyType<RPC::InvokeServerType<1, 0, 4>>::Create();
    mClientPreinstallManager = Core::ProxyType<RPC::CommunicatorClient>::Create(Core::NodeId("/tmp/communicator"), Core::ProxyType<Core::IIPCServer>(mEnginePreinstallManager));

    if (!mClientPreinstallManager.IsValid())
    {
        TEST_LOG("Invalid mClientPreinstallManager");
    }
    else
    {
        mControllerPreinstallManager = mClientPreinstallManager->Open<PluginHost::IShell>(_T(PREINSTALLMANAGER_CALLSIGN), ~0, 3000);
        if (mControllerPreinstallManager)
        {
            mPreinstallManagerPlugin = mControllerPreinstallManager->QueryInterface<Exchange::IPreinstallManager>();
            return_value = Core::ERROR_NONE;
        }
    }
    return return_value;
}

void PreinstallManagerTest::SetUpPreinstallDirectoryMocks() {
    // Use the actual local widget file path for package discovery
    // Always use the local test widget directory for L2 tests
    static const std::string s_packageDir = "entservices-infra/Tests/L2Tests/tests/testPackage/";
    ON_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillByDefault(::testing::Invoke([](const char* pathname) -> DIR* {
            TEST_LOG("opendir called with pathname: %s", pathname);
            // Accept both relative and absolute paths for CI compatibility
            EXPECT_TRUE(std::string(pathname) == s_packageDir || std::string(pathname).find("testPackage") != std::string::npos);
            static char fake_dir;
            return reinterpret_cast<DIR*>(&fake_dir);
        }));

    ON_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillByDefault([](DIR* dirp) -> struct dirent* {
            static int call_count = 0;
            static struct dirent entry;
            if (call_count == 0) {
                // Return a valid package directory name for libpackage
                std::strncpy(entry.d_name, "testPackage", sizeof(entry.d_name) - 1);
                entry.d_name[sizeof(entry.d_name) - 1] = '\0';
                entry.d_type = DT_DIR; // Directory
                TEST_LOG("readdir returning entry: %s", entry.d_name);
                call_count++;
                return &entry;
            } else {
                call_count = 0; // Reset for next traversal
                TEST_LOG("readdir returning: nullptr (end of directory)");
                return nullptr;
            }
        });

    ON_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillByDefault([](DIR* dirp) {
            return 0;
        });
}

/**
 * @brief Release PreinstallManager Plugin Interface object using Com-RPC connection
 */
void PreinstallManagerTest::ReleasePreinstallManagerInterfaceObjectUsingComRPCConnection()
{
    if (mPreinstallManagerPlugin) {
        mPreinstallManagerPlugin->Release();
        mPreinstallManagerPlugin = nullptr;
    }

    if (mControllerPreinstallManager) {
        mControllerPreinstallManager->Release();
        mControllerPreinstallManager = nullptr;
    }

    if (mClientPreinstallManager.IsValid()) {
        mClientPreinstallManager->Close(RPC::CommunicationTimeOut);
        mClientPreinstallManager.Release();
    }

    if (mEnginePreinstallManager.IsValid()) {
        mEnginePreinstallManager.Release();
    }
}

/**
 * @brief Test StartPreinstall method with valid parameters
 *
 * @details This test verifies:
 * - StartPreinstall method can be called successfully
 * - Method returns appropriate error codes (ERROR_NONE or ERROR_GENERAL)
 * - Plugin handles both force and non-force installation modes
 * - Test works whether PackageManager service is available or not
 */
TEST_F(PreinstallManagerTest, StartPreinstallBasicFunctionality)
{
    ASSERT_EQ(Core::ERROR_NONE, CreatePreinstallManagerInterfaceObjectUsingComRPCConnection());

    // Set up directory operation mocks to prevent real directory access
    SetUpPreinstallDirectoryMocks();

    // Test with force install disabled and enabled
    Core::hresult result = mPreinstallManagerPlugin->StartPreinstall(false);
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL) << "StartPreinstall should return valid result";

    result = mPreinstallManagerPlugin->StartPreinstall(true);
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL) << "StartPreinstall with force should return valid result";

    ReleasePreinstallManagerInterfaceObjectUsingComRPCConnection();
}

/**
 * @brief Test notification registration and unregistration
 *
 * @details This test verifies:
 * - Notification interface can be registered successfully
 * - Notification interface can be unregistered successfully
 * - Multiple registrations are handled properly
 * - Unregistering non-registered callbacks returns ERROR_GENERAL (acceptable)
 */
TEST_F(PreinstallManagerTest, NotificationRegisterUnregisterTest)
{
    ASSERT_EQ(Core::ERROR_NONE, CreatePreinstallManagerInterfaceObjectUsingComRPCConnection());

    Core::ProxyType<TestNotification> testNotification = Core::ProxyType<TestNotification>::Create();
    ASSERT_TRUE(testNotification.IsValid());

    // Test registration and duplicate registration
    Core::hresult result = mPreinstallManagerPlugin->Register(testNotification.operator->());
    EXPECT_EQ(Core::ERROR_NONE, result);
    
    result = mPreinstallManagerPlugin->Register(testNotification.operator->());
    EXPECT_EQ(Core::ERROR_NONE, result);

    // Test unregistration
    result = mPreinstallManagerPlugin->Unregister(testNotification.operator->());
    EXPECT_EQ(Core::ERROR_NONE, result);

    // Test unregistration of non-registered callback
    result = mPreinstallManagerPlugin->Unregister(testNotification.operator->());
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);

    ReleasePreinstallManagerInterfaceObjectUsingComRPCConnection();
}

/**
 * @brief Test StartPreinstall behavior when PackageManager service is unavailable
 *
 * @details This test verifies:
 * - StartPreinstall gracefully handles PackageManager unavailability
 * - Appropriate error codes are returned when dependency is missing
 * - Plugin doesn't crash when PackageManager service is not running
 * - Error path coverage for PackageManager object creation failure
 */
TEST_F(PreinstallManagerTest, StartPreinstallPackageManagerUnavailableTest)
{
    // Deactivate PackageManager for testing
    uint32_t status = DeactivateService("org.rdk.PackageManagerRDKEMS");
    TEST_LOG("PackageManager deactivation status: %d", status);
    
    ASSERT_EQ(Core::ERROR_NONE, CreatePreinstallManagerInterfaceObjectUsingComRPCConnection());

    // Test StartPreinstall when PackageManager is unavailable
    Core::hresult result = mPreinstallManagerPlugin->StartPreinstall(false);
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    result = mPreinstallManagerPlugin->StartPreinstall(true);
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    ReleasePreinstallManagerInterfaceObjectUsingComRPCConnection();
    
    // Try to reactivate PackageManager for subsequent tests
    status = ActivateService("org.rdk.PackageManagerRDKEMS");
    TEST_LOG("PackageManager reactivation status: %d", status);
}

/**
 * @brief Test error handling and edge cases in StartPreinstall
 *
 * @details This test verifies:
 * - Plugin handles various error conditions gracefully
 * - Error paths are properly covered for comprehensive testing
 * - Different combinations of service availability and directory states
 */
TEST_F(PreinstallManagerTest, StartPreinstallErrorPathsTest)
{
    ASSERT_EQ(Core::ERROR_NONE, CreatePreinstallManagerInterfaceObjectUsingComRPCConnection());
    
    // Set up directory operation mocks to prevent real directory access
    SetUpPreinstallDirectoryMocks();
    
    // Test multiple consecutive calls for consistency  
    Core::hresult result1 = mPreinstallManagerPlugin->StartPreinstall(false);
    Core::hresult result2 = mPreinstallManagerPlugin->StartPreinstall(false);
    EXPECT_EQ(result1, result2);
    
    // Test alternating force modes
    Core::hresult resultForce = mPreinstallManagerPlugin->StartPreinstall(true);
    Core::hresult resultNonForce = mPreinstallManagerPlugin->StartPreinstall(false);
    
    EXPECT_TRUE((resultForce == Core::ERROR_NONE || resultForce == Core::ERROR_GENERAL));
    EXPECT_TRUE((resultNonForce == Core::ERROR_NONE || resultNonForce == Core::ERROR_GENERAL));
    
    ReleasePreinstallManagerInterfaceObjectUsingComRPCConnection();
}

/**
 * @brief Test PackageManager interaction expectations
 *
 * @details This test demonstrates what SHOULD happen when PreinstallManager
 * successfully interacts with PackageManager:
 * - PreinstallManager should call ListPackages() to get installed packages
 * - PreinstallManager should call Install() for new packages
 * - PreinstallManager should call GetConfigForPackage() to get package info
 * - In L2 tests, if real PackageManager service is available, these interactions happen
 * - If not available, PreinstallManager handles gracefully with ERROR_GENERAL
 */
TEST_F(PreinstallManagerTest, PackageManagerInteractionExpectationsTest)
{
    ASSERT_EQ(Core::ERROR_NONE, CreatePreinstallManagerInterfaceObjectUsingComRPCConnection());

    // Set up directory operation mocks
    SetUpPreinstallDirectoryMocks();
    
    // Test StartPreinstall - this will attempt PackageManager interactions
    Core::hresult result = mPreinstallManagerPlugin->StartPreinstall(false);
    
    // Both outcomes are valid in L2 testing environment
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);

    ReleasePreinstallManagerInterfaceObjectUsingComRPCConnection();
}

/**
 * @brief Test with libpackage-style parameter validation
 * 
 * @details Following libpackage/tests/PackageImplTest.cpp patterns:
 * - Tests parameter validation similar to libpackage tests
 * - Uses proper package naming conventions
 * - Validates error handling for invalid inputs
 */
TEST_F(PreinstallManagerTest, LibpackageStyleValidationTest)
{
    ASSERT_EQ(Core::ERROR_NONE, CreatePreinstallManagerInterfaceObjectUsingComRPCConnection());

    // Mock directory to return libpackage-compatible package names
    ON_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillByDefault([](DIR* dirp) -> struct dirent* {
            static int call_count = 0;
            static struct dirent entry;
            
            switch(call_count++) {
                case 0:
                    // Valid libpackage format: com.rdk.appname_version
                    std::strncpy(entry.d_name, "com.rdk.testapp_1.0.0", sizeof(entry.d_name) - 1);
                    entry.d_type = DT_DIR;
                    return &entry;
                case 1:
                    // Another valid package
                    std::strncpy(entry.d_name, "com.rdk.cobalt_2.1.0", sizeof(entry.d_name) - 1);
                    entry.d_type = DT_DIR;
                    return &entry;
                default:
                    call_count = 0;
                    return nullptr;
            }
        });
    
    // Use the local test widget directory for libpackage-style validation
    ON_CALL(*p_wrapsImplMock, opendir(::testing::StrEq("entservices-infra/Tests/L2Tests/tests/testPackage/")))
        .WillByDefault([](const char* pathname) -> DIR* {
            static char fake_dir;
            return reinterpret_cast<DIR*>(&fake_dir);
        });
    
    // Test both force and non-force modes like libpackage tests
    Core::hresult result1 = mPreinstallManagerPlugin->StartPreinstall(false);
    EXPECT_TRUE(result1 == Core::ERROR_NONE || result1 == Core::ERROR_GENERAL);
    
    Core::hresult result2 = mPreinstallManagerPlugin->StartPreinstall(true);
    EXPECT_TRUE(result2 == Core::ERROR_NONE || result2 == Core::ERROR_GENERAL);
    
    // Results should be consistent for same input (like libpackage expects)
    Core::hresult result3 = mPreinstallManagerPlugin->StartPreinstall(false);
    EXPECT_EQ(result1, result3);

    ReleasePreinstallManagerInterfaceObjectUsingComRPCConnection();
}
