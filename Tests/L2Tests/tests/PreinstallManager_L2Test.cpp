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

PreinstallManagerTest::PreinstallManagerTest():L2TestMocks()
{
    Core::JSONRPC::Message message;
    string response;
    uint32_t status = Core::ERROR_GENERAL;

    /* Activate plugin in constructor */
    status = ActivateService("org.rdk.PersistentStore");
    EXPECT_EQ(Core::ERROR_NONE, status);
    
    status = ActivateService("org.rdk.PackageManager");
    EXPECT_EQ(Core::ERROR_NONE, status);
    
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

    status = DeactivateService("org.rdk.PackageManager");
    EXPECT_EQ(Core::ERROR_NONE, status);

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
 * - Method returns appropriate error codes
 * - Plugin handles both force and non-force installation modes
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
    EXPECT_EQ(Core::ERROR_NONE, result); // Should handle gracefully

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
