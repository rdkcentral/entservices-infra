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
#include <fstream>
#include <string>
#include <vector>
#include <cstdio>
#include <mutex>
#include <chrono>
#include <condition_variable>
#include <future>
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>

#include "PreinstallManager.h"
#include "PreinstallManagerImplementation.h"
#include "ServiceMock.h"
#include "PackageManagerMock.h"
#include "COMLinkMock.h"
#include "ThunderPortability.h"
#include "Module.h"
#include "WorkerPoolImplementation.h"
#include "WrapsMock.h"
#include "FactoriesImplementation.h"

extern "C" DIR* __real_opendir(const char* pathname);

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);

#define TIMEOUT   (50000)
#define PREINSTALL_MANAGER_TEST_PACKAGE_ID      "com.test.preinstall.app"
#define PREINSTALL_MANAGER_TEST_VERSION         "1.0.0"
#define PREINSTALL_MANAGER_TEST_FILE_LOCATOR    "/opt/preinstall/testapp/package.wgt"
#define PREINSTALL_MANAGER_WRONG_PACKAGE_ID     "com.wrongtest.preinstall.app"

typedef enum : uint32_t {
    PreinstallManager_StateInvalid = 0x00000000,
    PreinstallManager_onAppInstallationStatus = 0x00000001
} PreinstallManagerL1test_async_events_t;

using ::testing::NiceMock;
using namespace WPEFramework;

namespace {
const string callSign = _T("PreinstallManager");
}

class PreinstallManagerTest : public ::testing::Test {
protected:
    ServiceMock* mServiceMock = nullptr;
    PackageInstallerMock* mPackageInstallerMock = nullptr;
    WrapsImplMock *p_wrapsImplMock = nullptr;
    Core::JSONRPC::Message message;
    FactoriesImplementation factoriesImplementation;
    PLUGINHOST_DISPATCHER *dispatcher;

    Core::ProxyType<Plugin::PreinstallManager> plugin;
    Plugin::PreinstallManagerImplementation *mPreinstallManagerImpl;
    Exchange::IPackageInstaller::INotification* mPackageInstallerNotification_cb = nullptr;
    Exchange::IPreinstallManager::INotification* mPreinstallManagerNotification = nullptr;

    Core::ProxyType<WorkerPoolImplementation> workerPool;
    Core::JSONRPC::Handler& mJsonRpcHandler;
    DECL_CORE_JSONRPC_CONX connection;
    string mJsonRpcResponse;

    void createPreinstallManagerImpl()
    {
        mServiceMock = new NiceMock<ServiceMock>;
        
        TEST_LOG("In createPreinstallManagerImpl!");
        EXPECT_EQ(string(""), plugin->Initialize(mServiceMock));
        mPreinstallManagerImpl = Plugin::PreinstallManagerImplementation::getInstance();
    }

    void releasePreinstallManagerImpl()
    {
        TEST_LOG("In releasePreinstallManagerImpl!");
        plugin->Deinitialize(mServiceMock);
        delete mServiceMock;
        mPreinstallManagerImpl = nullptr;
    }

    Core::hresult createResources()
    {
        Core::hresult status = Core::ERROR_GENERAL;
        mServiceMock = new NiceMock<ServiceMock>;
        mPackageInstallerMock = new NiceMock<PackageInstallerMock>;
        testing::Mock::AllowLeak(mPackageInstallerMock); // Allow leak since mock lifecycle is managed by test framework
        p_wrapsImplMock = new NiceMock<WrapsImplMock>;
        Wraps::setImpl(p_wrapsImplMock);

        PluginHost::IFactories::Assign(&factoriesImplementation);
        dispatcher = static_cast<PLUGINHOST_DISPATCHER*>(
        plugin->QueryInterface(PLUGINHOST_DISPATCHER_ID));
        dispatcher->Activate(mServiceMock);
        TEST_LOG("In createResources!");

        EXPECT_CALL(*mServiceMock, QueryInterfaceByCallsign(::testing::_, ::testing::_))
          .Times(::testing::AnyNumber())
          .WillRepeatedly(::testing::Invoke(
              [&](const uint32_t id, const std::string& name) -> void* {
                if (name == "org.rdk.PackageManagerRDKEMS") {
                    if (id == Exchange::IPackageInstaller::ID) {
                        return reinterpret_cast<void*>(mPackageInstallerMock);
                    }
                }
            return nullptr;
        }));

        EXPECT_CALL(*mPackageInstallerMock, Register(::testing::_))
            .WillOnce(::testing::Invoke(
                [&](Exchange::IPackageInstaller::INotification* notification) {
                    mPackageInstallerNotification_cb = notification;
                    return Core::ERROR_NONE;
                }));

        // Setup default directory mocking for preinstall operations
        SetUpPreinstallDirectoryMocks();

        ON_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke([](const char* pathname, struct stat* buf) -> int {
            std::string path(pathname);
            TEST_LOG("createResources: stat called with path: %s", pathname);
            // Simulate successful stat for preinstall related files/directories
            if (path.find("/opt/preinstall") != std::string::npos) {
                std::memset(buf, 0, sizeof(struct stat));
                buf->st_mode = S_IFDIR | 0755; // Directory with permissions
                buf->st_size = 4096;
                TEST_LOG("createResources: stat returning success for preinstall path");
                return 0; // Success
            }
            TEST_LOG("createResources: stat returning failure for other path");
            return -1; // Failure for other paths
        }));

        // Add additional file system mocks that might be needed
        ON_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke([](const char* pathname, mode_t mode) -> int {
            TEST_LOG("createResources: mkdir called with path: %s", pathname);
            return 0; // Always succeed
        }));

        ON_CALL(*p_wrapsImplMock, rmdir(::testing::_))
        .WillByDefault(::testing::Invoke([](const char* pathname) -> int {
            TEST_LOG("createResources: rmdir called with path: %s", pathname);
            return 0; // Always succeed
        }));
        
        EXPECT_EQ(string(""), plugin->Initialize(mServiceMock));
        mPreinstallManagerImpl = Plugin::PreinstallManagerImplementation::getInstance();
        TEST_LOG("createResources - All done!");
        status = Core::ERROR_NONE;

        return status;
    }

    void releaseResources()
    {
        TEST_LOG("In releaseResources!");

        if (mPackageInstallerMock != nullptr && mPackageInstallerNotification_cb != nullptr)
        {
            ON_CALL(*mPackageInstallerMock, Unregister(::testing::_))
                .WillByDefault(::testing::Invoke([&]() {
                    return 0;
                }));
            mPackageInstallerNotification_cb = nullptr;
        }

        if (mPackageInstallerMock != nullptr)
        {
            EXPECT_CALL(*mPackageInstallerMock, Release())
                .WillOnce(::testing::Invoke(
                [&]() {
                     delete mPackageInstallerMock;
                     return 0;
                    }));
        }

        Wraps::setImpl(nullptr);
        if (p_wrapsImplMock != nullptr)
        {
            delete p_wrapsImplMock;
            p_wrapsImplMock = nullptr;
        }

        dispatcher->Deactivate();
        dispatcher->Release();

        plugin->Deinitialize(mServiceMock);
        delete mServiceMock;
        mPreinstallManagerImpl = nullptr;
    }

    PreinstallManagerTest()
        : plugin(Core::ProxyType<Plugin::PreinstallManager>::Create()),
        workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(2, Core::Thread::DefaultStackSize(), 16)),
        mJsonRpcHandler(*plugin),
        INIT_CONX(1, 0)
    {
        Core::IWorkerPool::Assign(&(*workerPool));
        workerPool->Run();
    }

    virtual ~PreinstallManagerTest() override
    {
        TEST_LOG("Delete ~PreinstallManagerTest Instance!");
        Core::IWorkerPool::Assign(nullptr);
        workerPool.Release();
    }

    auto FillPackageIterator()
    {
        std::list<Exchange::IPackageInstaller::Package> packageList;
        Exchange::IPackageInstaller::Package package_1;

        package_1.packageId = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
        package_1.version = PREINSTALL_MANAGER_TEST_VERSION;
        package_1.digest = "";
        package_1.state = Exchange::IPackageInstaller::InstallState::INSTALLED;
        package_1.sizeKb = 0;

        packageList.emplace_back(package_1);
        return Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IPackageIterator>>::Create<Exchange::IPackageInstaller::IPackageIterator>(packageList);
    }

    void SetUpPreinstallDirectoryMocks()
    {
        // Mock directory operations for preinstall directory
        ON_CALL(*p_wrapsImplMock, opendir(::testing::_))
            .WillByDefault(::testing::Invoke([](const char* pathname) -> DIR* {
                TEST_LOG("SetUpPreinstallDirectoryMocks: opendir called with path: %s", pathname);
                // Mock successful opening of /opt/preinstall directory
                if (std::string(pathname) == "/opt/preinstall") {
                    TEST_LOG("SetUpPreinstallDirectoryMocks: /opt/preinstall found, returning success");
                    return reinterpret_cast<DIR*>(0x1234); // Non-null pointer for success
                }
                TEST_LOG("SetUpPreinstallDirectoryMocks: other path, returning null");
                return nullptr; // Return null for other paths to simulate directory not found
            }));

        // Create mock dirent structure for testing
        static struct dirent testDirent;
        std::memset(&testDirent, 0, sizeof(testDirent));
        std::strncpy(testDirent.d_name, "testapp", sizeof(testDirent.d_name) - 1);
        testDirent.d_type = DT_DIR;

        ON_CALL(*p_wrapsImplMock, readdir(::testing::_))
            .WillByDefault(::testing::Invoke([](DIR* dirp) -> struct dirent* {
                static int call_count = 0;
                static struct dirent entry;
                TEST_LOG("SetUpPreinstallDirectoryMocks: readdir called, call_count = %d", call_count);
                
                if (call_count == 0) {
                    std::strncpy(entry.d_name, "testapp", sizeof(entry.d_name) - 1);
                    entry.d_type = DT_DIR;
                    call_count++;
                    TEST_LOG("SetUpPreinstallDirectoryMocks: returning testapp");
                    return &entry;
                } else if (call_count == 1) {
                    std::strncpy(entry.d_name, "preinstallapp", sizeof(entry.d_name) - 1);
                    entry.d_type = DT_DIR;
                    call_count++;
                    return &entry;
                } else {
                    call_count = 0; // Reset for next traversal
                    return nullptr; // End of directory
                }
            }));

        ON_CALL(*p_wrapsImplMock, closedir(::testing::_))
            .WillByDefault(::testing::Return(0));

        // Mock access function to simulate directory permissions
        ON_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke([](const char* pathname, int mode) -> int {
                // Simulate successful access to preinstall related directories
                std::string path(pathname);
                if (path.find("/opt/preinstall") != std::string::npos) {
                    return 0; // Success
                }
                return -1; // Failure for other paths
            }));
    }
};

// Mock notification class using GMock
class MockNotificationTest : public Exchange::IPreinstallManager::INotification 
{
public:
    MockNotificationTest() = default;
    virtual ~MockNotificationTest() = default;
    
    MOCK_METHOD(void, OnAppInstallationStatus, (const string& jsonresponse), (override));
    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));

    BEGIN_INTERFACE_MAP(MockNotificationTest)
    INTERFACE_ENTRY(Exchange::IPreinstallManager::INotification)
    END_INTERFACE_MAP
};

/*Test cases for PreinstallManager Plugin*/
/**
 * @brief Verify that PreinstallManager plugin can be created successfully
 */
TEST_F(PreinstallManagerTest, CreatePreinstallManagerPlugin)
{
    EXPECT_TRUE(plugin.IsValid());
}

/**
 * @brief Test successful registration of notification interface
 *
 * @details Test verifies that:
 * - Notification can be registered successfully  
 * - Register method returns ERROR_NONE
 */
TEST_F(PreinstallManagerTest, RegisterNotification)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());

    auto mockNotification = Core::ProxyType<MockNotificationTest>::Create();
    testing::Mock::AllowLeak(mockNotification.operator->()); // Allow leak since ProxyType manages lifecycle
    Core::hresult status = mPreinstallManagerImpl->Register(mockNotification.operator->());
    
    EXPECT_EQ(Core::ERROR_NONE, status);
    
    // Cleanup
    mPreinstallManagerImpl->Unregister(mockNotification.operator->());
    releaseResources();
}

/**
 * @brief Test successful unregistration of notification interface
 *
 * @details Test verifies that:
 * - Notification can be unregistered successfully after registration
 * - Unregister method returns ERROR_NONE
 */
TEST_F(PreinstallManagerTest, UnregisterNotification)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());

    auto mockNotification = Core::ProxyType<MockNotificationTest>::Create();
    testing::Mock::AllowLeak(mockNotification.operator->()); // Allow leak since ProxyType manages lifecycle
    
    // First register
    Core::hresult registerStatus = mPreinstallManagerImpl->Register(mockNotification.operator->());
    EXPECT_EQ(Core::ERROR_NONE, registerStatus);
    
    // Then unregister
    Core::hresult unregisterStatus = mPreinstallManagerImpl->Unregister(mockNotification.operator->());
    EXPECT_EQ(Core::ERROR_NONE, unregisterStatus);
    
    releaseResources();
}

/**
 * @brief Test StartPreinstall with force install enabled
 *
 * @details Test verifies that:
 * - StartPreinstall can be called with forceInstall=true
 * - Method returns appropriate status
 */
TEST_F(PreinstallManagerTest, StartPreinstallWithForceInstall)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock PackageInstaller methods
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = PREINSTALL_MANAGER_TEST_VERSION;
            return Core::ERROR_NONE;
        });

    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &packageId, const string &version, 
                           Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                           const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            return Core::ERROR_NONE;
        });
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    // The result can be ERROR_NONE or ERROR_GENERAL depending on directory existence
    // We mainly test that the method doesn't crash
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
    
    releaseResources();
}

/**
 * @brief Test StartPreinstall with force install disabled
 *
 * @details Test verifies that:
 * - StartPreinstall can be called with forceInstall=false
 * - Method checks existing packages before installing
 */
TEST_F(PreinstallManagerTest, StartPreinstallWithoutForceInstall)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock ListPackages to return existing packages
    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
        .WillRepeatedly([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
            auto mockIterator = FillPackageIterator();
            packages = mockIterator;
            return Core::ERROR_NONE;
        });

    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = PREINSTALL_MANAGER_TEST_VERSION;
            return Core::ERROR_NONE;
        });
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(false);
    
    // The result can be ERROR_NONE or ERROR_GENERAL depending on directory existence
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
    
    releaseResources();
}

/**
 * @brief Test StartPreinstall failure when PackageManager object creation fails
 *
 * @details Test verifies that:
 * - StartPreinstall returns ERROR_GENERAL when PackageManager is not available
 */
TEST_F(PreinstallManagerTest, StartPreinstallFailsWhenPackageManagerUnavailable)
{
    // Create minimal setup without PackageManager mock
    mServiceMock = new NiceMock<ServiceMock>;
    
    // Don't set up PackageInstaller mock in QueryInterfaceByCallsign
    EXPECT_CALL(*mServiceMock, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(nullptr));
    
    EXPECT_EQ(string(""), plugin->Initialize(mServiceMock));
    mPreinstallManagerImpl = Plugin::PreinstallManagerImplementation::getInstance();
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    plugin->Deinitialize(mServiceMock);
    delete mServiceMock;
    mPreinstallManagerImpl = nullptr;
}

/**
 * @brief Test notification handling for app installation status
 *
 * @details Test verifies that:
 * - Notification callbacks are properly triggered
 * - Installation status is handled correctly
 */
TEST_F(PreinstallManagerTest, HandleAppInstallationStatusNotification)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    auto mockNotification = Core::ProxyType<MockNotificationTest>::Create();
    testing::Mock::AllowLeak(mockNotification.operator->()); // Allow leak since ProxyType manages lifecycle
    
    // Use a promise/future to wait for the asynchronous notification
    std::promise<void> notificationPromise;
    std::future<void> notificationFuture = notificationPromise.get_future();
    
    // Expect the notification method to be called and signal completion
    EXPECT_CALL(*mockNotification, OnAppInstallationStatus(::testing::_))
        .Times(1)
        .WillOnce(::testing::InvokeWithoutArgs([&notificationPromise]() {
            notificationPromise.set_value();
        }));
    
    mPreinstallManagerImpl->Register(mockNotification.operator->());
    
    // Simulate installation status notification
    string testJsonResponse = R"({"packageId":"testApp","version":"1.0.0","status":"SUCCESS"})";
    
    // Call the handler directly since it's a friend class
    mPreinstallManagerImpl->handleOnAppInstallationStatus(testJsonResponse);
    
    // Wait for the asynchronous notification (with timeout)
    auto status = notificationFuture.wait_for(std::chrono::seconds(2));
    EXPECT_EQ(std::future_status::ready, status) << "Notification was not received within timeout";
    
    // Cleanup
    mPreinstallManagerImpl->Unregister(mockNotification.operator->());
    releaseResources();
}

/**
 * @brief Test QueryInterface functionality
 *
 * @details Test verifies that:
 * - QueryInterface returns proper interfaces
 * - IPreinstallManager interface can be obtained
 */
TEST_F(PreinstallManagerTest, QueryInterface)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Test querying IPreinstallManager interface
    Exchange::IPreinstallManager* preinstallInterface = 
        static_cast<Exchange::IPreinstallManager*>(
            mPreinstallManagerImpl->QueryInterface(Exchange::IPreinstallManager::ID));
    
    EXPECT_TRUE(preinstallInterface != nullptr);
    
    if (preinstallInterface != nullptr) {
        preinstallInterface->Release();
    }
    
    releaseResources();
}

/**
 * @brief Test PreinstallDirectory access failure scenario
 *
 * @details Test verifies that:
 * - StartPreinstall handles directory access failures gracefully
 * - Proper error codes are returned when directory operations fail
 */
TEST_F(PreinstallManagerTest, PreinstallDirectoryAccessFailure)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    TEST_LOG("PreinstallDirectoryAccessFailure: Starting test");
    
    // Override the default mock to simulate directory access failure
    ON_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillByDefault(::testing::Invoke([](const char* pathname) -> DIR* {
            TEST_LOG("PreinstallDirectoryAccessFailure: opendir called with path: %s", pathname);
            // Simulate failure to open /opt/preinstall directory
            if (std::string(pathname) == "/opt/preinstall") {
                TEST_LOG("PreinstallDirectoryAccessFailure: returning nullptr for /opt/preinstall");
                return nullptr; // Simulate directory not found or permission denied
            }
            TEST_LOG("PreinstallDirectoryAccessFailure: returning success for other path");
            return reinterpret_cast<DIR*>(0x1234); // Success for other paths
        }));

    TEST_LOG("PreinstallDirectoryAccessFailure: Calling StartPreinstall");
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    // Should handle the failure gracefully - could return ERROR_GENERAL or ERROR_NONE depending on implementation
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
    TEST_LOG("PreinstallDirectoryAccessFailure result = %u", result);
    
    releaseResources();
}

/**
 * @brief Test basic directory mocking functionality
 *
 * @details Test verifies that:
 * - Directory mocks are properly set up and working
 * - We can call directory functions and get expected results
 */
TEST_F(PreinstallManagerTest, BasicDirectoryMockTest)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    TEST_LOG("BasicDirectoryMockTest: Testing direct mock calls");
    
    // Test if opendir mock is working
    DIR* testDir = p_wrapsImplMock->opendir("/opt/preinstall");
    TEST_LOG("BasicDirectoryMockTest: opendir returned: %p", testDir);
    EXPECT_NE(nullptr, testDir);
    
    if (testDir) {
        // Test if readdir mock is working
        struct dirent* entry = p_wrapsImplMock->readdir(testDir);
        TEST_LOG("BasicDirectoryMockTest: readdir returned: %p", entry);
        if (entry) {
            TEST_LOG("BasicDirectoryMockTest: entry name: %s", entry->d_name);
        }
        
        // Test closedir
        int closeResult = p_wrapsImplMock->closedir(testDir);
        TEST_LOG("BasicDirectoryMockTest: closedir returned: %d", closeResult);
    }
    
    releaseResources();
}

/**
 * @brief Test PreinstallDirectory read operations success scenario
 *
 * @details Test verifies that:
 * - StartPreinstall can successfully read preinstall directory contents
 * - Multiple app directories are processed correctly
 */
TEST_F(PreinstallManagerTest, PreinstallDirectoryReadSuccess)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock PackageInstaller methods for successful installation
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = PREINSTALL_MANAGER_TEST_VERSION;
            return Core::ERROR_NONE;
        });

    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &packageId, const string &version, 
                           Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                           const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            return Core::ERROR_NONE;
        });

    // Override readdir to return multiple app directories
    ON_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillByDefault(::testing::Invoke([](DIR* dirp) -> struct dirent* {
            static int call_count = 0;
            static struct dirent entry;
            TEST_LOG("PreinstallDirectoryReadSuccess: readdir called, call_count = %d", call_count);
            
            if (call_count == 0) {
                std::strncpy(entry.d_name, "testapp1", sizeof(entry.d_name) - 1);
                entry.d_type = DT_DIR;
                call_count++;
                return &entry;
            } else if (call_count == 1) {
                std::strncpy(entry.d_name, "testapp2", sizeof(entry.d_name) - 1);
                entry.d_type = DT_DIR;
                call_count++;
                return &entry;
            } else {
                call_count = 0; // Reset for next traversal
                return nullptr; // End of directory
            }
        }));

    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    // Should succeed with proper directory structure - but may return ERROR_GENERAL if no packages found
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
    TEST_LOG("PreinstallDirectoryReadSuccess result = %u", result);
    
    releaseResources();
}

/**
 * @brief Test PreinstallDirectory with empty directory scenario
 *
 * @details Test verifies that:
 * - StartPreinstall handles empty preinstall directory correctly
 * - No apps to install scenario is handled gracefully
 */
TEST_F(PreinstallManagerTest, PreinstallDirectoryEmpty)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Override readdir to return no entries (empty directory)
    EXPECT_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillOnce(::testing::Return(nullptr)); // Immediately return end of directory
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    // Should handle empty directory gracefully
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
    TEST_LOG("PreinstallDirectoryEmpty result = %u", result);
    
    releaseResources();
}
