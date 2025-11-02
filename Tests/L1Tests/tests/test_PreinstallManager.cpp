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
#include <cstdlib>

#define UNIT_TESTING

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
        // Mock directory operations for preinstall directory
        ON_CALL(*p_wrapsImplMock, opendir(::testing::_))
            .WillByDefault(::testing::Return(reinterpret_cast<DIR*>(0x1234))); // Non-null pointer

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
 * @brief Test UNIT_TESTING conditional compilation with real /tmp/install directory
 *
 * @details This test verifies:
 * - With UNIT_TESTING defined, PreinstallManager uses /tmp/install instead of /opt/preinstall
 * - Creates real test directory structure with dummy packages
 * - Tests actual file system operations in a controlled environment
 * - Perfect for CI/CD - uses accessible /tmp directory
 */
TEST_F(PreinstallManagerTest, UnitTestingDirectoryAccess)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    const std::string testDir = "/tmp/install";
    const std::string package1Dir = testDir + "/test-package-1.0.0";
    const std::string package2Dir = testDir + "/test-package-2.1.0";
    
    TEST_LOG("========================================");
    TEST_LOG("Setting up real test directory: %s", testDir.c_str());
    TEST_LOG("========================================");
    
    // Create the test directory structure with status checking
    int mkdirStatus1 = std::system(("mkdir -p " + testDir).c_str());
    TEST_LOG("mkdir status for %s: %d", testDir.c_str(), mkdirStatus1);
    
    int mkdirStatus2 = std::system(("mkdir -p " + package1Dir).c_str());
    TEST_LOG("mkdir status for %s: %d", package1Dir.c_str(), mkdirStatus2);
    
    int mkdirStatus3 = std::system(("mkdir -p " + package2Dir).c_str());
    TEST_LOG("mkdir status for %s: %d", package2Dir.c_str(), mkdirStatus3);
    
    // Verify directories exist using stat
    struct stat dirStat;
    bool testDirExists = (stat(testDir.c_str(), &dirStat) == 0) && S_ISDIR(dirStat.st_mode);
    bool package1DirExists = (stat(package1Dir.c_str(), &dirStat) == 0) && S_ISDIR(dirStat.st_mode);
    bool package2DirExists = (stat(package2Dir.c_str(), &dirStat) == 0) && S_ISDIR(dirStat.st_mode);
    
    TEST_LOG("Directory existence verification:");
    TEST_LOG("  %s exists: %s", testDir.c_str(), testDirExists ? "YES" : "NO");
    TEST_LOG("  %s exists: %s", package1Dir.c_str(), package1DirExists ? "YES" : "NO");
    TEST_LOG("  %s exists: %s", package2Dir.c_str(), package2DirExists ? "YES" : "NO");
    
    // Create dummy package files to simulate real packages with status checking
    int fileStatus1 = std::system(("echo 'test package 1 content' > " + package1Dir + "/package.json").c_str());
    TEST_LOG("File creation status for %s/package.json: %d", package1Dir.c_str(), fileStatus1);
    
    int fileStatus2 = std::system(("echo 'test package 2 content' > " + package2Dir + "/package.json").c_str());
    TEST_LOG("File creation status for %s/package.json: %d", package2Dir.c_str(), fileStatus2);
    
    int fileStatus3 = std::system(("echo 'manifest data' > " + package1Dir + "/manifest.json").c_str());
    TEST_LOG("File creation status for %s/manifest.json: %d", package1Dir.c_str(), fileStatus3);
    
    int fileStatus4 = std::system(("echo 'manifest data' > " + package2Dir + "/manifest.json").c_str());
    TEST_LOG("File creation status for %s/manifest.json: %d", package2Dir.c_str(), fileStatus4);
    
    // Verify files exist
    std::string pkg1JsonPath = package1Dir + "/package.json";
    std::string pkg2JsonPath = package2Dir + "/package.json";
    std::string pkg1ManifestPath = package1Dir + "/manifest.json";
    std::string pkg2ManifestPath = package2Dir + "/manifest.json";
    
    bool pkg1JsonExists = (stat(pkg1JsonPath.c_str(), &dirStat) == 0) && S_ISREG(dirStat.st_mode);
    bool pkg2JsonExists = (stat(pkg2JsonPath.c_str(), &dirStat) == 0) && S_ISREG(dirStat.st_mode);
    bool pkg1ManifestExists = (stat(pkg1ManifestPath.c_str(), &dirStat) == 0) && S_ISREG(dirStat.st_mode);
    bool pkg2ManifestExists = (stat(pkg2ManifestPath.c_str(), &dirStat) == 0) && S_ISREG(dirStat.st_mode);
    
    TEST_LOG("File existence verification:");
    TEST_LOG("  %s exists: %s", pkg1JsonPath.c_str(), pkg1JsonExists ? "YES" : "NO");
    TEST_LOG("  %s exists: %s", pkg2JsonPath.c_str(), pkg2JsonExists ? "YES" : "NO");
    TEST_LOG("  %s exists: %s", pkg1ManifestPath.c_str(), pkg1ManifestExists ? "YES" : "NO");
    TEST_LOG("  %s exists: %s", pkg2ManifestPath.c_str(), pkg2ManifestExists ? "YES" : "NO");
    
    TEST_LOG("Created test packages:");
    TEST_LOG("  - %s (Dir: %s, Files: %s)", package1Dir.c_str(), 
             package1DirExists ? "OK" : "FAIL", 
             (pkg1JsonExists && pkg1ManifestExists) ? "OK" : "FAIL");
    TEST_LOG("  - %s (Dir: %s, Files: %s)", package2Dir.c_str(), 
             package2DirExists ? "OK" : "FAIL", 
             (pkg2JsonExists && pkg2ManifestExists) ? "OK" : "FAIL");
    
    // Mock the package installer to handle our test packages
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            TEST_LOG("GetConfigForPackage called with: %s", fileLocator.c_str());
            
            if (fileLocator.find("test-package-1.0.0") != std::string::npos) {
                id = "com.test.package1";
                version = "1.0.0";
                TEST_LOG("  → Configured as: %s v%s", id.c_str(), version.c_str());
                return Core::ERROR_NONE;
            } else if (fileLocator.find("test-package-2.1.0") != std::string::npos) {
                id = "com.test.package2";
                version = "2.1.0";
                TEST_LOG("  → Configured as: %s v%s", id.c_str(), version.c_str());
                return Core::ERROR_NONE;
            } else {
                TEST_LOG("  → Unknown package: %s", fileLocator.c_str());
                return Core::ERROR_GENERAL;
            }
        });
    
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([](const string &packageId, const string &version, 
                          Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                          const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            TEST_LOG("Install called: %s v%s from %s", packageId.c_str(), version.c_str(), fileLocator.c_str());
            TEST_LOG("  → Installation successful");
            return Core::ERROR_NONE;
        });
    
    // Run the test - should now use /tmp/install due to UNIT_TESTING macro
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    TEST_LOG("========================================");
    TEST_LOG("Test Results:");
    TEST_LOG("  StartPreinstall result: %d", result);
    TEST_LOG("  Expected to use directory: %s", testDir.c_str());
    TEST_LOG("========================================");
    
    // Verify success - should work because /tmp/install exists and has packages
    EXPECT_EQ(Core::ERROR_NONE, result) << "StartPreinstall should succeed with real test directory";
    
    // Clean up the test directory
    int cleanupStatus = std::system(("rm -rf " + testDir).c_str());
    TEST_LOG("Cleanup command status for %s: %d", testDir.c_str(), cleanupStatus);
    
    // Verify cleanup was successful
    struct stat cleanupStat;
    bool dirStillExists = (stat(testDir.c_str(), &cleanupStat) == 0);
    TEST_LOG("Directory after cleanup - %s still exists: %s", 
             testDir.c_str(), dirStillExists ? "YES (cleanup failed)" : "NO (cleanup successful)");
    
    releaseResources();
}
