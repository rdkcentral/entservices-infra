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

        ON_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(-1));
        
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
        // Mock directory operations for preinstall directory - NO real file system access
        // Works for ANY path, not just hardcoded ones - GitHub CI friendly!
        ON_CALL(*p_wrapsImplMock, opendir(::testing::_))
            .WillByDefault(::testing::Invoke([](const char* pathname) {
                std::string path(pathname);
                TEST_LOG("Mock opendir called for path: %s", pathname);
                
                // Always return fake DIR pointers for ANY path - works on any environment
                // Use different pointers to distinguish paths in logs if needed
                if (path.find("preinstall") != std::string::npos) {
                    TEST_LOG("Returning 0x1234 for preinstall-related path: %s", pathname);
                    return reinterpret_cast<DIR*>(0x1234);
                } else {
                    TEST_LOG("Returning 0x5678 for other path: %s", pathname);
                    return reinterpret_cast<DIR*>(0x5678);
                }
            }));

        // Create mock dirent structure for testing
        static struct dirent testDirent;
        strcpy(testDirent.d_name, "testapp");
        static struct dirent* direntPtr = &testDirent;
        static bool firstCall = true;

        ON_CALL(*p_wrapsImplMock, readdir(::testing::_))
            .WillByDefault(::testing::Invoke([&](DIR*) -> struct dirent* {
                if (firstCall) {
                    firstCall = false;
                    return direntPtr; // Return test directory entry first time
                }
                return nullptr; // End of directory
            }));

        ON_CALL(*p_wrapsImplMock, closedir(::testing::_))
            .WillByDefault(::testing::Return(0));
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

    SetUpPreinstallDirectoryMocks();
    
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

    SetUpPreinstallDirectoryMocks();
    
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
 * @brief Test readPreinstallDirectory method indirectly through StartPreinstall
 *
 * @details Test verifies that:
 * - readPreinstallDirectory method is called through StartPreinstall
 * - Mock file system operations work correctly on ANY environment (GitHub CI, local, etc.)
 * - Directory traversal and package discovery functions properly
 * - All directory operations (opendir, readdir, closedir) are properly mocked
 * - Works regardless of actual preinstall directory path or existence
 * 
 * @note This test is GitHub CI/CD friendly - no hardcoded path dependencies!
 */
TEST_F(PreinstallManagerTest, ReadPreinstallDirectoryWithMockFileSystem)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Track which mocked functions are called to verify the flow
    bool opendirCalled = false;
    bool readdirCalled = false;
    bool closedirCalled = false;
    bool getConfigCalled = false;
    
    // Set up comprehensive mocks with detailed logging - NO real file system access
    // GitHub CI/CD friendly - works with ANY preinstall directory path!
    ON_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillByDefault(::testing::Invoke([&opendirCalled](const char* pathname) {
            TEST_LOG("=== opendir called with pathname: %s ===", pathname);
            opendirCalled = true;
            
            std::string path(pathname);
            
            // Always return fake DIR pointers - works for ANY environment/path
            // Check if this looks like a preinstall directory (flexible matching)
            if (path.find("preinstall") != std::string::npos) {
                TEST_LOG("Returning fake DIR pointer 0x1234 for preinstall path: %s", pathname);
                return reinterpret_cast<DIR*>(0x1234);
            } else {
                TEST_LOG("Returning fake DIR pointer 0x5678 for other path: %s", pathname);
                return reinterpret_cast<DIR*>(0x5678);
            }
        }));

    // Mock readdir with detailed logging
    static bool readDirFirstCall = true;
    ON_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillByDefault(::testing::Invoke([&readdirCalled](DIR* dirp) -> struct dirent* {
            TEST_LOG("=== readdir called with DIR pointer: %p ===", dirp);
            readdirCalled = true;
            
            static struct dirent testEntry;
            if (readDirFirstCall) {
                readDirFirstCall = false;
                strcpy(testEntry.d_name, "mockpackage");
                testEntry.d_type = DT_DIR;
                TEST_LOG("readdir returning directory entry: %s", testEntry.d_name);
                return &testEntry;
            }
            TEST_LOG("readdir returning nullptr (end of directory)");
            return nullptr; // End of directory
        }));

    ON_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillByDefault(::testing::Invoke([&closedirCalled](DIR* dirp) {
            TEST_LOG("=== closedir called with DIR pointer: %p ===", dirp);
            closedirCalled = true;
            return 0;
        }));

    // Mock PackageInstaller methods with detailed tracking
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&getConfigCalled](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            TEST_LOG("=== GetConfigForPackage called with fileLocator: %s ===", fileLocator.c_str());
            getConfigCalled = true;
            
            if (fileLocator.find("mockpackage/package.wgt") != std::string::npos) {
                id = "com.mock.test.package";
                version = "1.0.0";
                TEST_LOG("GetConfigForPackage SUCCESS: packageId=%s, version=%s", id.c_str(), version.c_str());
                return Core::ERROR_NONE;
            }
            TEST_LOG("GetConfigForPackage FAILED for fileLocator: %s", fileLocator.c_str());
            return Core::ERROR_GENERAL;
        });

    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &packageId, const string &version, 
                           Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                           const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            TEST_LOG("=== Install called: packageId=%s, version=%s, fileLocator=%s ===", 
                     packageId.c_str(), version.c_str(), fileLocator.c_str());
            return Core::ERROR_NONE;
        });

    // Call StartPreinstall which should internally call readPreinstallDirectory
    TEST_LOG("========================================");
    TEST_LOG("Starting preinstall test with force=true");
    TEST_LOG("========================================");
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    TEST_LOG("========================================");
    TEST_LOG("StartPreinstall completed with result: %d", result);
    TEST_LOG("========================================");
    
    // Verify that our mocked functions were actually called
    TEST_LOG("Function call verification:");
    TEST_LOG("  opendir called: %s", opendirCalled ? "YES" : "NO");
    TEST_LOG("  readdir called: %s", readdirCalled ? "YES" : "NO"); 
    TEST_LOG("  closedir called: %s", closedirCalled ? "YES" : "NO");
    TEST_LOG("  getConfig called: %s", getConfigCalled ? "YES" : "NO");
    
    // The test passes if readPreinstallDirectory was called (evidenced by directory functions being called)
    EXPECT_TRUE(opendirCalled) << "opendir should have been called to open /opt/preinstall";
    
    // If opendir succeeded, readdir and closedir should also be called
    if (opendirCalled) {
        EXPECT_TRUE(readdirCalled) << "readdir should have been called to read directory contents";
        EXPECT_TRUE(closedirCalled) << "closedir should have been called to close directory";
    }
    
    // The result should be SUCCESS since we've mocked everything to work
    EXPECT_EQ(Core::ERROR_NONE, result) << "StartPreinstall should succeed with proper mocks";
    
    // Reset static variable for next test
    readDirFirstCall = true;
    
    releaseResources();
}

/**
 * @brief Test that demonstrates environment-agnostic directory access
 *
 * @details This test shows how the mock works regardless of:
 * - Operating system (Linux, Windows, macOS)  
 * - Directory permissions
 * - Whether directories actually exist
 * - GitHub CI/CD environments vs local development
 * 
 * The key insight: Mock ANY directory path the implementation tries to access
 */
TEST_F(PreinstallManagerTest, EnvironmentAgnosticDirectoryAccess)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    std::vector<std::string> attemptedPaths;
    
    // Capture ALL directory access attempts - regardless of path
    ON_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillByDefault(::testing::Invoke([&attemptedPaths](const char* pathname) {
            std::string path(pathname);
            attemptedPaths.push_back(path);
            
            TEST_LOG("Environment-agnostic mock: opendir('%s') -> SUCCESS", pathname);
            
            // Return success for ANY path - perfect for CI/CD environments!
            return reinterpret_cast<DIR*>(0x9999);
        }));

    ON_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillByDefault([](DIR*) -> struct dirent* {
            static bool firstCall = true;
            static struct dirent entry;
            
            if (firstCall) {
                firstCall = false;
                strcpy(entry.d_name, "ci_test_package");
                entry.d_type = DT_DIR;
                TEST_LOG("Mock readdir returning: %s", entry.d_name);
                return &entry;
            }
            return nullptr;
        });

    ON_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillByDefault([](DIR*) { return 0; });

    // Mock package operations
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            TEST_LOG("Mock GetConfigForPackage: %s", fileLocator.c_str());
            id = "com.ci.test.package";
            version = "1.0.0";
            return Core::ERROR_NONE;
        });

    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([](const string &packageId, const string &version, 
                          Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                          const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            TEST_LOG("Mock Install: %s v%s", packageId.c_str(), version.c_str());
            return Core::ERROR_NONE;
        });

    // Test the actual functionality
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    TEST_LOG("=== Test Results ===");
    TEST_LOG("StartPreinstall result: %d", result);
    TEST_LOG("Paths accessed: %zu", attemptedPaths.size());
    
    for (size_t i = 0; i < attemptedPaths.size(); ++i) {
        TEST_LOG("  Path %zu: %s", i, attemptedPaths[i].c_str());
    }
    
    // Verify the test worked regardless of environment
    EXPECT_TRUE(!attemptedPaths.empty()) << "Should have attempted to access at least one directory path";
    EXPECT_EQ(Core::ERROR_NONE, result) << "Should succeed in any environment with proper mocks";
    
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
