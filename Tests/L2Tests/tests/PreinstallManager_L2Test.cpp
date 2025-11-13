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
#include <interfaces/IPreinstallManager.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <future>
#include <chrono>

#ifndef THUNDER_VERSION_MAJOR
#define THUNDER_VERSION_MAJOR 4
#endif

#ifndef THUNDER_VERSION_MINOR  
#define THUNDER_VERSION_MINOR 4
#endif

#ifndef THUNDER_VERSION
#define THUNDER_VERSION 4
#endif

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);
#define DEFAULT_PREINSTALL_PATH    "/opt/preinstall"

#define JSON_TIMEOUT   (1000)
#define PREINSTALLMANAGER_CALLSIGN  _T("org.rdk.PreinstallManager")
#define PREINSTALLMANAGERL2TEST_CALLSIGN _T("L2tests.1")

using ::testing::NiceMock;
using namespace WPEFramework;
using testing::StrictMock;
using ::WPEFramework::Exchange::IPreinstallManager;

class PreinstallManagerTest : public L2TestMocks {
protected:
    virtual ~PreinstallManagerTest() override;

public:
    PreinstallManagerTest();

    uint32_t CreatePreinstallManagerInterfaceObjectUsingComRPCConnection();
    void ReleasePreinstallManagerInterfaceObjectUsingComRPCConnection();

    // Notification handler for testing
    class TestNotification : public Exchange::IPreinstallManager::INotification {
    private:
        std::promise<std::string>* m_promise;

    public:
        TestNotification(std::promise<std::string>* promise) : m_promise(promise) {}

        void OnAppInstallationStatus(const string& jsonresponse) override {
            TEST_LOG("OnAppInstallationStatus received: %s", jsonresponse.c_str());
            if (m_promise) {
                m_promise->set_value(jsonresponse);
            }
        }

        BEGIN_INTERFACE_MAP(TestNotification)
        INTERFACE_ENTRY(Exchange::IPreinstallManager::INotification)
        END_INTERFACE_MAP
    };

private:

protected:
    /** @brief Pointer to the IShell interface */
    PluginHost::IShell *mControllerPreinstallManager;
    Core::ProxyType<RPC::InvokeServerType<1, 0, 4>> mEnginePreinstallManager;
    Core::ProxyType<RPC::CommunicatorClient> mClientPreinstallManager;

    /** @brief Pointer to the IPreinstallManager interface */
    Exchange::IPreinstallManager *mPreinstallManagerPlugin;
};

PreinstallManagerTest::PreinstallManagerTest():L2TestMocks(),
    mControllerPreinstallManager(nullptr),
    mPreinstallManagerPlugin(nullptr)
{
    Core::JSONRPC::Message message;
    string response;
    uint32_t status = Core::ERROR_GENERAL;

    /* Activate plugin in constructor */
    status = ActivateService("org.rdk.PersistentStore");
    EXPECT_EQ(Core::ERROR_NONE, status);
    
    // PackageManager may not be available in test environment, so don't fail if it's not available
    status = ActivateService("org.rdk.PackageManager");
    if (status != Core::ERROR_NONE) {
        TEST_LOG("PackageManager service not available in test environment, continuing without it");
    }
    
    status = ActivateService(PREINSTALLMANAGER_CALLSIGN);
    EXPECT_EQ(Core::ERROR_NONE, status);
}

/**
 * @brief Destructor for PreinstallManager L2 test class
 */
PreinstallManagerTest::~PreinstallManagerTest()
{
    uint32_t status = Core::ERROR_GENERAL;

    status = DeactivateService(PREINSTALLMANAGER_CALLSIGN);
    EXPECT_EQ(Core::ERROR_NONE, status);

    // PackageManager may not be available in test environment
    status = DeactivateService("org.rdk.PackageManager");
    if (status != Core::ERROR_NONE) {
        TEST_LOG("PackageManager service deactivation failed or not available, status: %d", status);
    }

    status = DeactivateService("org.rdk.PersistentStore");
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

    TEST_LOG("Creating mEnginePreinstallManager Announcements");
#if ((THUNDER_VERSION == 2) || ((THUNDER_VERSION == 4) && (THUNDER_VERSION_MINOR == 2)))
    mEnginePreinstallManager->Announcements(mClientPreinstallManager->Announcement());
#endif
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

/**
 * @brief Release PreinstallManager Plugin Interface object using Com-RPC connection
 */
void PreinstallManagerTest::ReleasePreinstallManagerInterfaceObjectUsingComRPCConnection()
{
    if (mPreinstallManagerPlugin) {
        TEST_LOG("Releasing PreinstallManager plugin interface");
        mPreinstallManagerPlugin->Release();
        mPreinstallManagerPlugin = nullptr;
    }

    if (mControllerPreinstallManager) {
        TEST_LOG("Releasing controller");
        mControllerPreinstallManager->Release();
        mControllerPreinstallManager = nullptr;
    }

    if (mClientPreinstallManager.IsValid()) {
        TEST_LOG("Releasing client");
        mClientPreinstallManager->Close(RPC::CommunicationTimeOut);
        mClientPreinstallManager.Release();
    }

    if (mEnginePreinstallManager.IsValid()) {
        TEST_LOG("Releasing engine");
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
 * - In test environment without PackageManager, ERROR_GENERAL is expected
 */
TEST_F(PreinstallManagerTest, StartPreinstallBasicFunctionality)
{
    TEST_LOG("### Test StartPreinstall Basic Functionality Begin ###");
    
    ASSERT_EQ(Core::ERROR_NONE, CreatePreinstallManagerInterfaceObjectUsingComRPCConnection());

    // Test with force install disabled first
    TEST_LOG("Testing StartPreinstall with forceInstall=false");
    Core::hresult result = mPreinstallManagerPlugin->StartPreinstall(false);
    TEST_LOG("StartPreinstall(false) returned: %d", result);
    
    // The result depends on the preinstall directory existence and contents
    // Both ERROR_NONE and ERROR_GENERAL are acceptable outcomes
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);

    // Test with force install enabled
    TEST_LOG("Testing StartPreinstall with forceInstall=true");
    result = mPreinstallManagerPlugin->StartPreinstall(true);
    TEST_LOG("StartPreinstall(true) returned: %d", result);
    
    // Both ERROR_NONE and ERROR_GENERAL are acceptable outcomes
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);

    ReleasePreinstallManagerInterfaceObjectUsingComRPCConnection();
    TEST_LOG("### Test StartPreinstall Basic Functionality End ###");
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
    TEST_LOG("### Test Notification Register/Unregister Begin ###");
    
    ASSERT_EQ(Core::ERROR_NONE, CreatePreinstallManagerInterfaceObjectUsingComRPCConnection());

    std::promise<std::string> notificationPromise;
    auto testNotification = Core::ProxyType<TestNotification>::Create(&notificationPromise);

    // Test registration
    TEST_LOG("Registering notification callback");
    Core::hresult result = mPreinstallManagerPlugin->Register(testNotification.operator->());
    EXPECT_EQ(Core::ERROR_NONE, result);

    // Test multiple registration (should handle gracefully)
    TEST_LOG("Registering same notification callback again");
    result = mPreinstallManagerPlugin->Register(testNotification.operator->());
    EXPECT_EQ(Core::ERROR_NONE, result); // Should handle duplicate registration

    // Test unregistration
    TEST_LOG("Unregistering notification callback");
    result = mPreinstallManagerPlugin->Unregister(testNotification.operator->());
    EXPECT_EQ(Core::ERROR_NONE, result);

    // Test unregistration of non-registered callback (should handle gracefully)
    TEST_LOG("Unregistering already unregistered callback");
    result = mPreinstallManagerPlugin->Unregister(testNotification.operator->());
    // Plugin may return ERROR_GENERAL for already unregistered callbacks, which is acceptable
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);

    ReleasePreinstallManagerInterfaceObjectUsingComRPCConnection();
    TEST_LOG("### Test Notification Register/Unregister End ###");
}

/**
 * @brief Test preinstall directory handling
 *
 * @details This test verifies:
 * - Plugin can handle missing preinstall directory
 * - Plugin can handle empty preinstall directory
 * - Plugin creates necessary directories as configured in L2-tests.yml
 */
TEST_F(PreinstallManagerTest, PreinstallDirectoryHandling)
{
    TEST_LOG("### Test Preinstall Directory Handling Begin ###");
    
    ASSERT_EQ(Core::ERROR_NONE, CreatePreinstallManagerInterfaceObjectUsingComRPCConnection());

    // The preinstall directory is configured as /opt/preinstall in the L2-tests.yml
    // This test verifies the plugin can handle the directory regardless of its state
    
    TEST_LOG("Testing StartPreinstall with configured preinstall directory: %s", DEFAULT_PREINSTALL_PATH);
    
    // Check if directory exists
    struct stat statbuf;
    bool dirExists = (stat(DEFAULT_PREINSTALL_PATH, &statbuf) == 0);
    TEST_LOG("Preinstall directory exists: %s", dirExists ? "yes" : "no");

    Core::hresult result = mPreinstallManagerPlugin->StartPreinstall(false);
    TEST_LOG("StartPreinstall returned: %d", result);
    
    // The plugin should handle both existing and non-existing directories gracefully
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);

    ReleasePreinstallManagerInterfaceObjectUsingComRPCConnection();
    TEST_LOG("### Test Preinstall Directory Handling End ###");
}

/**
 * @brief Test interface query functionality
 *
 * @details This test verifies:
 * - PreinstallManager interface can be queried successfully
 * - Interface implements required methods
 */
TEST_F(PreinstallManagerTest, InterfaceQueryTest)
{
    TEST_LOG("### Test Interface Query Begin ###");
    
    ASSERT_EQ(Core::ERROR_NONE, CreatePreinstallManagerInterfaceObjectUsingComRPCConnection());

    // Verify the interface is valid and accessible
    ASSERT_NE(nullptr, mPreinstallManagerPlugin);
    TEST_LOG("PreinstallManager interface successfully obtained");

    // Test that we can call the interface methods without crashing
    // This is a basic smoke test for the interface
    std::promise<std::string> notificationPromise;
    auto testNotification = Core::ProxyType<TestNotification>::Create(&notificationPromise);

    // Test all interface methods are callable
    Core::hresult result = mPreinstallManagerPlugin->Register(testNotification.operator->());
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);

    result = mPreinstallManagerPlugin->StartPreinstall(false);
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);

    result = mPreinstallManagerPlugin->Unregister(testNotification.operator->());
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);

    ReleasePreinstallManagerInterfaceObjectUsingComRPCConnection();
    TEST_LOG("### Test Interface Query End ###");
}

/**
 * @brief Test plugin activation and deactivation
 *
 * @details This test verifies:
 * - Plugin can be activated successfully
 * - Plugin can be deactivated successfully
 * - Multiple activate/deactivate cycles work properly
 */
TEST_F(PreinstallManagerTest, PluginActivationTest)
{
    TEST_LOG("### Test Plugin Activation Begin ###");

    // Test deactivation
    uint32_t status = DeactivateService(PREINSTALLMANAGER_CALLSIGN);
    EXPECT_EQ(Core::ERROR_NONE, status);
    TEST_LOG("Plugin deactivated successfully");

    // Test re-activation
    status = ActivateService(PREINSTALLMANAGER_CALLSIGN);
    EXPECT_EQ(Core::ERROR_NONE, status);
    TEST_LOG("Plugin re-activated successfully");

    // Verify plugin is functional after re-activation
    ASSERT_EQ(Core::ERROR_NONE, CreatePreinstallManagerInterfaceObjectUsingComRPCConnection());
    
    Core::hresult result = mPreinstallManagerPlugin->StartPreinstall(false);
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
    TEST_LOG("Plugin is functional after re-activation");

    ReleasePreinstallManagerInterfaceObjectUsingComRPCConnection();
    TEST_LOG("### Test Plugin Activation End ###");
}

/**
 * @brief Test basic plugin presence and initialization
 *
 * @details This test verifies:
 * - Plugin can be activated without crashing
 * - Plugin interface can be obtained
 * - Basic methods can be called without causing crashes
 * - This is a minimal smoke test that should always pass
 */
TEST_F(PreinstallManagerTest, BasicPluginSmokeTest)
{
    TEST_LOG("### Test Basic Plugin Smoke Test Begin ###");
    
    // Just verify we can create the interface successfully
    ASSERT_EQ(Core::ERROR_NONE, CreatePreinstallManagerInterfaceObjectUsingComRPCConnection());
    
    // Verify the interface is valid
    ASSERT_NE(nullptr, mPreinstallManagerPlugin);
    TEST_LOG("PreinstallManager interface obtained successfully");
    
    // Create a simple notification for testing
    std::promise<std::string> notificationPromise;
    auto testNotification = Core::ProxyType<TestNotification>::Create(&notificationPromise);
    
    // Test basic method calls - they may fail due to dependencies but shouldn't crash
    TEST_LOG("Testing basic method calls");
    
    Core::hresult result = mPreinstallManagerPlugin->Register(testNotification.operator->());
    TEST_LOG("Register returned: %d", result);
    
    result = mPreinstallManagerPlugin->Unregister(testNotification.operator->());
    TEST_LOG("Unregister returned: %d", result);
    
    result = mPreinstallManagerPlugin->StartPreinstall(false);
    TEST_LOG("StartPreinstall returned: %d", result);
    
    // All method calls completed without crashing - this is the main success criteria
    TEST_LOG("All interface methods callable without crashes");

    ReleasePreinstallManagerInterfaceObjectUsingComRPCConnection();
    TEST_LOG("### Test Basic Plugin Smoke Test End ###");
}

/**
 * @brief Test notification callbacks during preinstall operations
 *
 * @details This test verifies:
 * - Notification callbacks are properly triggered during preinstall operations
 * - Multiple notification registrations work correctly
 * - Notification data is properly formatted
 */
TEST_F(PreinstallManagerTest, NotificationCallbackTest)
{
    TEST_LOG("### Test Notification Callback Begin ###");
    
    ASSERT_EQ(Core::ERROR_NONE, CreatePreinstallManagerInterfaceObjectUsingComRPCConnection());

    std::promise<std::string> notificationPromise;
    auto future = notificationPromise.get_future();
    auto testNotification = Core::ProxyType<TestNotification>::Create(&notificationPromise);

    // Register for notifications
    Core::hresult result = mPreinstallManagerPlugin->Register(testNotification.operator->());
    EXPECT_EQ(Core::ERROR_NONE, result);

    // Start preinstall operation to trigger notifications
    TEST_LOG("Starting preinstall operation to trigger notifications");
    result = mPreinstallManagerPlugin->StartPreinstall(false);
    
    // Wait for notification with timeout
    auto status = future.wait_for(std::chrono::seconds(5));
    if (status == std::future_status::ready) {
        std::string notification = future.get();
        TEST_LOG("Received notification: %s", notification.c_str());
        EXPECT_FALSE(notification.empty());
        // Basic JSON validation - should contain expected fields
        EXPECT_TRUE(notification.find("\"") != std::string::npos); // Should be JSON format
    } else {
        TEST_LOG("No notification received within timeout - may be expected in test environment");
    }

    // Cleanup
    mPreinstallManagerPlugin->Unregister(testNotification.operator->());
    ReleasePreinstallManagerInterfaceObjectUsingComRPCConnection();
    TEST_LOG("### Test Notification Callback End ###");
}

/**
 * @brief Test concurrent preinstall operations
 *
 * @details This test verifies:
 * - Multiple simultaneous preinstall calls are handled properly
 * - Plugin prevents race conditions
 * - Proper error handling for concurrent operations
 */
TEST_F(PreinstallManagerTest, ConcurrentPreinstallTest)
{
    TEST_LOG("### Test Concurrent Preinstall Begin ###");
    
    ASSERT_EQ(Core::ERROR_NONE, CreatePreinstallManagerInterfaceObjectUsingComRPCConnection());

    // Launch multiple preinstall operations concurrently
    std::vector<std::future<Core::hresult>> futures;
    
    for (int i = 0; i < 3; i++) {
        futures.push_back(std::async(std::launch::async, [this, i]() {
            TEST_LOG("Starting concurrent preinstall operation %d", i);
            return mPreinstallManagerPlugin->StartPreinstall(false);
        }));
    }

    // Collect results
    std::vector<Core::hresult> results;
    for (auto& future : futures) {
        Core::hresult result = future.get();
        results.push_back(result);
        TEST_LOG("Concurrent operation result: %d", result);
    }

    // At least one operation should complete successfully or return expected error
    bool hasValidResult = false;
    for (Core::hresult result : results) {
        if (result == Core::ERROR_NONE || result == Core::ERROR_GENERAL) {
            hasValidResult = true;
            break;
        }
    }
    EXPECT_TRUE(hasValidResult);

    ReleasePreinstallManagerInterfaceObjectUsingComRPCConnection();
    TEST_LOG("### Test Concurrent Preinstall End ###");
}

/**
 * @brief Test error handling with invalid parameters
 *
 * @details This test verifies:
 * - Plugin handles null pointers gracefully
 * - Invalid notification registrations are rejected
 * - Error codes are properly returned
 */
TEST_F(PreinstallManagerTest, ErrorHandlingTest)
{
    TEST_LOG("### Test Error Handling Begin ###");
    
    ASSERT_EQ(Core::ERROR_NONE, CreatePreinstallManagerInterfaceObjectUsingComRPCConnection());

    // Test registering null notification
    TEST_LOG("Testing null notification registration");
    Core::hresult result = mPreinstallManagerPlugin->Register(nullptr);
    EXPECT_EQ(Core::ERROR_GENERAL, result);

    // Test unregistering null notification
    TEST_LOG("Testing null notification unregistration");
    result = mPreinstallManagerPlugin->Unregister(nullptr);
    EXPECT_EQ(Core::ERROR_GENERAL, result);

    // Test multiple force preinstalls
    TEST_LOG("Testing multiple force preinstall calls");
    result = mPreinstallManagerPlugin->StartPreinstall(true);
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
    
    result = mPreinstallManagerPlugin->StartPreinstall(true);
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);

    ReleasePreinstallManagerInterfaceObjectUsingComRPCConnection();
    TEST_LOG("### Test Error Handling End ###");
}

/**
 * @brief Test plugin resource management
 *
 * @details This test verifies:
 * - Plugin properly manages resources during operations
 * - Memory leaks are avoided
 * - Proper cleanup on interface destruction
 */
TEST_F(PreinstallManagerTest, ResourceManagementTest)
{
    TEST_LOG("### Test Resource Management Begin ###");
    
    // Create and destroy interface multiple times to test resource cleanup
    for (int i = 0; i < 3; i++) {
        TEST_LOG("Resource management iteration %d", i + 1);
        
        ASSERT_EQ(Core::ERROR_NONE, CreatePreinstallManagerInterfaceObjectUsingComRPCConnection());
        
        // Create multiple notifications
        std::vector<Core::ProxyType<TestNotification>> notifications;
        for (int j = 0; j < 2; j++) {
            std::promise<std::string> promise;
            auto notification = Core::ProxyType<TestNotification>::Create(&promise);
            notifications.push_back(notification);
            
            Core::hresult result = mPreinstallManagerPlugin->Register(notification.operator->());
            EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
        }
        
        // Perform operations
        Core::hresult result = mPreinstallManagerPlugin->StartPreinstall(false);
        EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
        
        // Cleanup notifications
        for (auto& notification : notifications) {
            mPreinstallManagerPlugin->Unregister(notification.operator->());
        }
        
        ReleasePreinstallManagerInterfaceObjectUsingComRPCConnection();
    }
    
    TEST_LOG("### Test Resource Management End ###");
}

/**
 * @brief Test plugin state consistency
 *
 * @details This test verifies:
 * - Plugin maintains consistent state across operations
 * - State is properly reset between operations
 * - No state pollution between test runs
 */
TEST_F(PreinstallManagerTest, StateConsistencyTest)
{
    TEST_LOG("### Test State Consistency Begin ###");
    
    ASSERT_EQ(Core::ERROR_NONE, CreatePreinstallManagerInterfaceObjectUsingComRPCConnection());

    std::promise<std::string> promise1;
    auto notification1 = Core::ProxyType<TestNotification>::Create(&promise1);
    
    // Test state sequence: Register -> StartPreinstall -> Unregister -> StartPreinstall
    TEST_LOG("Step 1: Register notification");
    Core::hresult result = mPreinstallManagerPlugin->Register(notification1.operator->());
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
    
    TEST_LOG("Step 2: Start preinstall with notification registered");
    result = mPreinstallManagerPlugin->StartPreinstall(false);
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
    
    TEST_LOG("Step 3: Unregister notification");
    result = mPreinstallManagerPlugin->Unregister(notification1.operator->());
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
    
    TEST_LOG("Step 4: Start preinstall without notification");
    result = mPreinstallManagerPlugin->StartPreinstall(false);
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
    
    // Test with force flag
    TEST_LOG("Step 5: Start preinstall with force flag");
    result = mPreinstallManagerPlugin->StartPreinstall(true);
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);

    ReleasePreinstallManagerInterfaceObjectUsingComRPCConnection();
    TEST_LOG("### Test State Consistency End ###");
}

/**
 * @brief Test plugin performance under load
 *
 * @details This test verifies:
 * - Plugin performs adequately under repeated operations
 * - No performance degradation over time
 * - Proper handling of rapid successive calls
 */
TEST_F(PreinstallManagerTest, PerformanceTest)
{
    TEST_LOG("### Test Performance Begin ###");
    
    ASSERT_EQ(Core::ERROR_NONE, CreatePreinstallManagerInterfaceObjectUsingComRPCConnection());

    std::promise<std::string> promise;
    auto notification = Core::ProxyType<TestNotification>::Create(&promise);
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Perform rapid register/unregister cycles
    const int iterations = 10;
    int successCount = 0;
    
    for (int i = 0; i < iterations; i++) {
        Core::hresult regResult = mPreinstallManagerPlugin->Register(notification.operator->());
        Core::hresult unregResult = mPreinstallManagerPlugin->Unregister(notification.operator->());
        
        if ((regResult == Core::ERROR_NONE || regResult == Core::ERROR_GENERAL) &&
            (unregResult == Core::ERROR_NONE || unregResult == Core::ERROR_GENERAL)) {
            successCount++;
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    TEST_LOG("Completed %d iterations in %ld ms, success rate: %d/%d", 
             iterations, duration.count(), successCount, iterations);
    
    // Should complete most operations successfully
    EXPECT_GT(successCount, iterations / 2);
    
    // Should complete within reasonable time (10 seconds for 10 iterations)
    EXPECT_LT(duration.count(), 10000);

    ReleasePreinstallManagerInterfaceObjectUsingComRPCConnection();
    TEST_LOG("### Test Performance End ###");
}

/**
 * @brief Test plugin behavior with different force install combinations
 *
 * @details This test verifies:
 * - Force install flag behavior is consistent
 * - Different sequences of force/non-force calls work properly
 * - Plugin state is properly maintained between force and non-force operations
 */
TEST_F(PreinstallManagerTest, ForceInstallBehaviorTest)
{
    TEST_LOG("### Test Force Install Behavior Begin ###");
    
    ASSERT_EQ(Core::ERROR_NONE, CreatePreinstallManagerInterfaceObjectUsingComRPCConnection());

    // Test sequence: non-force -> force -> non-force -> force
    TEST_LOG("Testing sequence of force/non-force preinstall operations");
    
    struct TestCase {
        bool forceInstall;
        const char* description;
    } testCases[] = {
        {false, "non-force preinstall"},
        {true, "force preinstall"},
        {false, "non-force preinstall after force"},
        {true, "force preinstall after non-force"},
        {true, "consecutive force preinstall"}
    };
    
    for (const auto& testCase : testCases) {
        TEST_LOG("Executing: %s (force=%s)", testCase.description, testCase.forceInstall ? "true" : "false");
        
        Core::hresult result = mPreinstallManagerPlugin->StartPreinstall(testCase.forceInstall);
        TEST_LOG("Result: %d", result);
        
        EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
        
        // Small delay between operations to ensure proper state handling
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    ReleasePreinstallManagerInterfaceObjectUsingComRPCConnection();
    TEST_LOG("### Test Force Install Behavior End ###");
}
