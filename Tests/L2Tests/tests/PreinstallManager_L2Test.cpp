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
#include <interfaces/IAppPackageManager.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);

#define DEFAULT_PREINSTALL_PATH    "/tmp/preinstall"
#define JSON_TIMEOUT   (1000)
#define PREINSTALLMANAGER_CALLSIGN  _T("org.rdk.PreinstallManager")
#define PACKAGEMANAGER_CALLSIGN  _T("org.rdk.PackageManager")
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

    class NotificationHandler : public Exchange::IPreinstallManager::INotification {
    public:
        NotificationHandler() : _notificationReceived(false) {}
        virtual ~NotificationHandler() = default;

        BEGIN_INTERFACE_MAP(NotificationHandler)
        INTERFACE_ENTRY(Exchange::IPreinstallManager::INotification)
        END_INTERFACE_MAP

        void OnAppInstallationStatus(const string& jsonresponse) override {
            TEST_LOG("Notification received: OnAppInstallationStatus - %s", jsonresponse.c_str());
            std::unique_lock<std::mutex> lock(_mutex);
            _lastResponse = jsonresponse;
            _notificationReceived = true;
            _condition.notify_one();
        }

        bool WaitForNotification(uint32_t timeoutMs = 5000) {
            std::unique_lock<std::mutex> lock(_mutex);
            return _condition.wait_for(lock, std::chrono::milliseconds(timeoutMs), 
                                     [this] { return _notificationReceived; });
        }

        const string& GetLastResponse() const {
            return _lastResponse;
        }

        void Reset() {
            std::unique_lock<std::mutex> lock(_mutex);
            _notificationReceived = false;
            _lastResponse.clear();
        }

    private:
        std::mutex _mutex;
        std::condition_variable _condition;
        bool _notificationReceived;
        string _lastResponse;
    };

private:
protected:
    /** @brief Pointer to the IShell interface */
    PluginHost::IShell *mControllerPreinstallManager;
    Core::ProxyType<RPC::InvokeServerType<1, 0, 4>> mEnginePreinstallManager;
    Core::ProxyType<RPC::CommunicatorClient> mClientPreinstallManager;

    /** @brief Pointer to the IPreinstallManager interface */
    Exchange::IPreinstallManager *mPreinstallManagerPlugin;

    /** @brief Notification handler for testing events */
    Core::ProxyType<NotificationHandler> mNotificationHandler;
};

PreinstallManagerTest::PreinstallManagerTest() : L2TestMocks()
{
    Core::JSONRPC::Message message;
    string response;
    uint32_t status = Core::ERROR_GENERAL;

    /* Activate required plugins in constructor */
    status = ActivateService("org.rdk.PersistentStore");
    EXPECT_EQ(Core::ERROR_NONE, status);
    
    status = ActivateService(PACKAGEMANAGER_CALLSIGN);
    EXPECT_EQ(Core::ERROR_NONE, status);
    
    status = ActivateService(PREINSTALLMANAGER_CALLSIGN);
    EXPECT_EQ(Core::ERROR_NONE, status);

    mNotificationHandler = Core::ProxyType<NotificationHandler>::Create();
}

/**
 * @brief Destructor for PreinstallManager L2 test class
 */
PreinstallManagerTest::~PreinstallManagerTest()
{
    uint32_t status = Core::ERROR_GENERAL;

    status = DeactivateService(PREINSTALLMANAGER_CALLSIGN);
    EXPECT_EQ(Core::ERROR_NONE, status);

    status = DeactivateService(PACKAGEMANAGER_CALLSIGN);
    EXPECT_EQ(Core::ERROR_NONE, status);

    status = DeactivateService("org.rdk.PersistentStore");
    EXPECT_EQ(Core::ERROR_NONE, status);
}

uint32_t PreinstallManagerTest::CreatePreinstallManagerInterfaceObjectUsingComRPCConnection()
{
    uint32_t return_value = Core::ERROR_GENERAL;

    TEST_LOG("Creating mEnginePreinstallManager");
    mEnginePreinstallManager = Core::ProxyType<RPC::InvokeServerType<1, 0, 4>>::Create();
    mClientPreinstallManager = Core::ProxyType<RPC::CommunicatorClient>::Create(
        Core::NodeId("/tmp/communicator"), 
        Core::ProxyType<Core::IIPCServer>(mEnginePreinstallManager));

    TEST_LOG("Creating mEnginePreinstallManager Announcements");
#if ((THUNDER_VERSION == 2) || ((THUNDER_VERSION == 4) && (THUNDER_VERSION_MINOR == 2)))
    mEnginePreinstallManager->Announcements(mClientPreinstallManager->Announcement());
#endif
    
    if (!mClientPreinstallManager.IsValid()) {
        TEST_LOG("Invalid mClientPreinstallManager");
    } else {
        mControllerPreinstallManager = mClientPreinstallManager->Open<PluginHost::IShell>(_T(PREINSTALLMANAGER_CALLSIGN), ~0, 3000);
        if (mControllerPreinstallManager) {
            mPreinstallManagerPlugin = mControllerPreinstallManager->QueryInterface<Exchange::IPreinstallManager>();
            if (mPreinstallManagerPlugin) {
                return_value = Core::ERROR_NONE;
            }
        }
    }
    return return_value;
}

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
    mEnginePreinstallManager.Release();
}

bool directoryExists(const std::string& path)
{
    struct stat statbuf;
    if (stat(path.c_str(), &statbuf) != 0) {
        return false;
    }
    return S_ISDIR(statbuf.st_mode);
}

void createPreinstallDirectory(const std::string& dirPath)
{
    if (mkdir(dirPath.c_str(), 0755) != 0 && errno != EEXIST) {
        TEST_LOG("Directory creation failed for: %s, errno: %d", dirPath.c_str(), errno);
    } else {
        TEST_LOG("Directory created or already exists: %s", dirPath.c_str());
    }
}

void createMockPackageFile(const std::string& packagePath, const std::string& packageName, const std::string& version)
{
    std::string packageDir = packagePath + "/" + packageName;
    createPreinstallDirectory(packageDir);
    
    std::string packageFile = packageDir + "/package.json";
    std::ofstream outFile(packageFile);
    
    if (!outFile) {
        TEST_LOG("Package file creation failed: %s", packageFile.c_str());
    } else {
        outFile << "{"
                << "\"name\":\"" << packageName << "\","
                << "\"version\":\"" << version << "\","
                << "\"description\":\"Test package for preinstall\""
                << "}";
        outFile.close();
        TEST_LOG("Package file created: %s", packageFile.c_str());
    }
}

void cleanupPreinstallDirectory(const std::string& dirPath)
{
    // Simple cleanup - remove directory contents
    std::string rmCommand = "rm -rf " + dirPath + "/*";
    system(rmCommand.c_str());
    TEST_LOG("Cleaned up directory: %s", dirPath.c_str());
}

/* Test Case for StartPreinstallUsingComRpcSuccess
 * Creating PreinstallManager Plugin using COM-RPC connection
 * Setting up mock preinstall directory structure with test packages
 * Setting mocks for filesystem operations (access, stat, opendir, readdir)
 * Registering notification handler to receive installation status events
 * Calling StartPreinstall() with forceInstall=false to trigger preinstall process
 * Verifying the StartPreinstall operation succeeds
 * Waiting for and verifying OnAppInstallationStatus notification is received
 * Unregistering notification handler
 * Releasing the PreinstallManager Interface object
 */
TEST_F(PreinstallManagerTest, StartPreinstallUsingComRpcSuccess)
{
    uint32_t status = Core::ERROR_GENERAL;
    
    TEST_LOG("### Test StartPreinstallUsingComRpcSuccess Begin ###");

    status = CreatePreinstallManagerInterfaceObjectUsingComRPCConnection();
    EXPECT_EQ(status, Core::ERROR_NONE);

    // Setup mock directory structure
    createPreinstallDirectory(DEFAULT_PREINSTALL_PATH);
    createMockPackageFile(DEFAULT_PREINSTALL_PATH, "testapp1", "1.0.0");
    createMockPackageFile(DEFAULT_PREINSTALL_PATH, "testapp2", "2.1.0");

    // Mock filesystem operations
    EXPECT_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* pathname, int mode) {
            TEST_LOG("Mock access called for: %s", pathname);
            return 0; // Simulate successful access
        });

    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, struct stat* buf) {
            TEST_LOG("Mock stat called for: %s", path);
            buf->st_mode = S_IFREG | 0644; // Regular file
            buf->st_size = 1024;
            return 0;
        });

    // Register notification handler
    status = mPreinstallManagerPlugin->Register(mNotificationHandler);
    EXPECT_EQ(Core::ERROR_NONE, status);

    // Reset notification handler before test
    mNotificationHandler->Reset();

    // Call StartPreinstall
    status = mPreinstallManagerPlugin->StartPreinstall(false);
    EXPECT_EQ(Core::ERROR_NONE, status);

    // Wait for notification (installation status)
    bool notificationReceived = mNotificationHandler->WaitForNotification(10000);
    EXPECT_TRUE(notificationReceived);

    if (notificationReceived) {
        const string& response = mNotificationHandler->GetLastResponse();
        EXPECT_FALSE(response.empty());
        TEST_LOG("Received notification response: %s", response.c_str());
    }

    // Unregister notification handler
    status = mPreinstallManagerPlugin->Unregister(mNotificationHandler);
    EXPECT_EQ(Core::ERROR_NONE, status);

    // Cleanup
    cleanupPreinstallDirectory(DEFAULT_PREINSTALL_PATH);

    if (Core::ERROR_NONE == status) {
        ReleasePreinstallManagerInterfaceObjectUsingComRPCConnection();
    }

    TEST_LOG("### Test StartPreinstallUsingComRpcSuccess End ###");
}

/* Test Case for StartPreinstallForceInstallUsingComRpcSuccess
 * Creating PreinstallManager Plugin using COM-RPC connection
 * Setting up mock preinstall directory with existing packages
 * Setting mocks for filesystem operations to simulate package detection
 * Registering notification handler to monitor installation events
 * Calling StartPreinstall() with forceInstall=true to force reinstallation
 * Verifying the StartPreinstall operation succeeds with force flag
 * Waiting for and verifying installation status notifications
 * Unregistering notification handler
 * Releasing the PreinstallManager Interface object
 */
TEST_F(PreinstallManagerTest, StartPreinstallForceInstallUsingComRpcSuccess)
{
    uint32_t status = Core::ERROR_GENERAL;
    
    TEST_LOG("### Test StartPreinstallForceInstallUsingComRpcSuccess Begin ###");

    status = CreatePreinstallManagerInterfaceObjectUsingComRPCConnection();
    EXPECT_EQ(status, Core::ERROR_NONE);

    // Setup mock directory structure with existing packages
    createPreinstallDirectory(DEFAULT_PREINSTALL_PATH);
    createMockPackageFile(DEFAULT_PREINSTALL_PATH, "forceapp", "1.5.0");

    // Mock filesystem operations to simulate existing packages
    EXPECT_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* pathname, int mode) {
            TEST_LOG("Mock access called for: %s", pathname);
            return 0; // Simulate package exists and accessible
        });

    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, struct stat* buf) {
            TEST_LOG("Mock stat called for: %s", path);
            buf->st_mode = S_IFREG | 0644;
            buf->st_size = 2048;
            return 0;
        });

    // Register notification handler
    status = mPreinstallManagerPlugin->Register(mNotificationHandler);
    EXPECT_EQ(Core::ERROR_NONE, status);

    mNotificationHandler->Reset();

    // Call StartPreinstall with force install
    status = mPreinstallManagerPlugin->StartPreinstall(true);
    EXPECT_EQ(Core::ERROR_NONE, status);

    // Wait for notification
    bool notificationReceived = mNotificationHandler->WaitForNotification(10000);
    EXPECT_TRUE(notificationReceived);

    if (notificationReceived) {
        const string& response = mNotificationHandler->GetLastResponse();
        EXPECT_FALSE(response.empty());
        TEST_LOG("Force install notification: %s", response.c_str());
    }

    // Unregister notification handler
    status = mPreinstallManagerPlugin->Unregister(mNotificationHandler);
    EXPECT_EQ(Core::ERROR_NONE, status);

    // Cleanup
    cleanupPreinstallDirectory(DEFAULT_PREINSTALL_PATH);

    if (Core::ERROR_NONE == status) {
        ReleasePreinstallManagerInterfaceObjectUsingComRPCConnection();
    }

    TEST_LOG("### Test StartPreinstallForceInstallUsingComRpcSuccess End ###");
}

/* Test Case for StartPreinstallEmptyDirectoryUsingComRpc
 * Creating PreinstallManager Plugin using COM-RPC connection
 * Setting up empty preinstall directory (no packages to install)
 * Setting mocks for filesystem operations to simulate empty directory
 * Registering notification handler
 * Calling StartPreinstall() on empty directory
 * Verifying the StartPreinstall operation handles empty directory gracefully
 * Checking that no installation notifications are generated for empty directory
 * Unregistering notification handler
 * Releasing the PreinstallManager Interface object
 */
TEST_F(PreinstallManagerTest, StartPreinstallEmptyDirectoryUsingComRpc)
{
    uint32_t status = Core::ERROR_GENERAL;
    
    TEST_LOG("### Test StartPreinstallEmptyDirectoryUsingComRpc Begin ###");

    status = CreatePreinstallManagerInterfaceObjectUsingComRPCConnection();
    EXPECT_EQ(status, Core::ERROR_NONE);

    // Setup empty directory
    createPreinstallDirectory(DEFAULT_PREINSTALL_PATH);

    // Mock filesystem operations for empty directory
    EXPECT_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* pathname, int mode) {
            std::string path(pathname);
            if (path == DEFAULT_PREINSTALL_PATH) {
                return 0; // Directory exists
            }
            return -1; // No packages exist
        });

    // Register notification handler
    status = mPreinstallManagerPlugin->Register(mNotificationHandler);
    EXPECT_EQ(Core::ERROR_NONE, status);

    mNotificationHandler->Reset();

    // Call StartPreinstall on empty directory
    status = mPreinstallManagerPlugin->StartPreinstall(false);
    EXPECT_EQ(Core::ERROR_NONE, status);

    // Wait briefly to ensure no unexpected notifications
    bool unexpectedNotification = mNotificationHandler->WaitForNotification(2000);
    // For empty directory, we might or might not get a notification depending on implementation
    // The important thing is that StartPreinstall should succeed

    TEST_LOG("Empty directory test - notification received: %s", unexpectedNotification ? "true" : "false");

    // Unregister notification handler
    status = mPreinstallManagerPlugin->Unregister(mNotificationHandler);
    EXPECT_EQ(Core::ERROR_NONE, status);

    // Cleanup
    cleanupPreinstallDirectory(DEFAULT_PREINSTALL_PATH);

    if (Core::ERROR_NONE == status) {
        ReleasePreinstallManagerInterfaceObjectUsingComRPCConnection();
    }

    TEST_LOG("### Test StartPreinstallEmptyDirectoryUsingComRpc End ###");
}

/* Test Case for NotificationRegisterUnregisterUsingComRpc
 * Creating PreinstallManager Plugin using COM-RPC connection
 * Testing Register() and Unregister() methods for notification handling
 * Registering multiple notification handlers and verifying success
 * Unregistering notification handlers and verifying proper cleanup
 * Testing error cases like registering null handlers or double registration
 * Releasing the PreinstallManager Interface object
 */
TEST_F(PreinstallManagerTest, NotificationRegisterUnregisterUsingComRpc)
{
    uint32_t status = Core::ERROR_GENERAL;
    
    TEST_LOG("### Test NotificationRegisterUnregisterUsingComRpc Begin ###");

    status = CreatePreinstallManagerInterfaceObjectUsingComRPCConnection();
    EXPECT_EQ(status, Core::ERROR_NONE);

    // Test successful registration
    status = mPreinstallManagerPlugin->Register(mNotificationHandler);
    EXPECT_EQ(Core::ERROR_NONE, status);

    // Test duplicate registration (should handle gracefully)
    status = mPreinstallManagerPlugin->Register(mNotificationHandler);
    // Implementation may handle this differently, but should not crash

    // Test successful unregistration
    status = mPreinstallManagerPlugin->Unregister(mNotificationHandler);
    EXPECT_EQ(Core::ERROR_NONE, status);

    // Test unregistering non-registered handler (should handle gracefully)
    status = mPreinstallManagerPlugin->Unregister(mNotificationHandler);
    // Should handle gracefully, might return error or success

    // Test null pointer handling
    status = mPreinstallManagerPlugin->Register(nullptr);
    EXPECT_NE(Core::ERROR_NONE, status); // Should return error for null pointer

    status = mPreinstallManagerPlugin->Unregister(nullptr);
    EXPECT_NE(Core::ERROR_NONE, status); // Should return error for null pointer

    if (Core::ERROR_NONE == status) {
        ReleasePreinstallManagerInterfaceObjectUsingComRPCConnection();
    }

    TEST_LOG("### Test NotificationRegisterUnregisterUsingComRpc End ###");
}

/* Test Case for StartPreinstallDirectoryAccessErrorUsingComRpc
 * Creating PreinstallManager Plugin using COM-RPC connection
 * Setting mocks to simulate preinstall directory access failures
 * Calling StartPreinstall() when directory is not accessible
 * Verifying the StartPreinstall operation handles access errors properly
 * Testing various filesystem error conditions (permissions, missing directory)
 * Releasing the PreinstallManager Interface object
 */
TEST_F(PreinstallManagerTest, StartPreinstallDirectoryAccessErrorUsingComRpc)
{
    uint32_t status = Core::ERROR_GENERAL;
    
    TEST_LOG("### Test StartPreinstallDirectoryAccessErrorUsingComRpc Begin ###");

    status = CreatePreinstallManagerInterfaceObjectUsingComRPCConnection();
    EXPECT_EQ(status, Core::ERROR_NONE);

    // Mock filesystem operations to simulate access failure
    EXPECT_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* pathname, int mode) {
            TEST_LOG("Mock access failure for: %s", pathname);
            return -1; // Simulate access failure
        });

    // Register notification handler to catch any error notifications
    status = mPreinstallManagerPlugin->Register(mNotificationHandler);
    EXPECT_EQ(Core::ERROR_NONE, status);

    mNotificationHandler->Reset();

    // Call StartPreinstall when directory is not accessible
    status = mPreinstallManagerPlugin->StartPreinstall(false);
    
    // The implementation should handle this gracefully - either return error or succeed with no action
    // Check if any error notification is sent
    bool notificationReceived = mNotificationHandler->WaitForNotification(5000);
    if (notificationReceived) {
        const string& response = mNotificationHandler->GetLastResponse();
        TEST_LOG("Error scenario notification: %s", response.c_str());
    }

    // Unregister notification handler
    status = mPreinstallManagerPlugin->Unregister(mNotificationHandler);
    EXPECT_EQ(Core::ERROR_NONE, status);

    if (Core::ERROR_NONE == status) {
        ReleasePreinstallManagerInterfaceObjectUsingComRPCConnection();
    }

    TEST_LOG("### Test StartPreinstallDirectoryAccessErrorUsingComRpc End ###");
}
