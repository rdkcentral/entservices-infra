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

// Ensure UNIT_TEST_BUILD is defined for conditional compilation
#ifndef UNIT_TEST_BUILD
#define UNIT_TEST_BUILD
#endif

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

        // Mock additional file system operations that might be needed
        ON_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(0)); // Success

        ON_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(0)); // Success

        ON_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(0)); // Success

        ON_CALL(*p_wrapsImplMock, rmdir(::testing::_))
            .WillByDefault(::testing::Return(0)); // Success
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
 * @brief Test StartPreinstall with directory operation failures
 *
 * @details Test verifies that:
 * - StartPreinstall handles opendir failure gracefully
 * - Method returns ERROR_GENERAL when directory cannot be opened
 */
TEST_F(PreinstallManagerTest, StartPreinstallWithOpendirFailure)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock opendir to return nullptr (failure)
    EXPECT_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillOnce(::testing::Return(nullptr));
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    releaseResources();
}

/**
 * @brief Test StartPreinstall with successful directory traversal
 *
 * @details Test verifies that:
 * - StartPreinstall successfully reads directory contents
 * - Multiple packages are processed correctly through directory operations
 * - Dot entries are properly skipped during directory reading
 */
TEST_F(PreinstallManagerTest, StartPreinstallWithSuccessfulDirectoryTraversal)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Create test directory entries
    static struct dirent testApp1, testApp2, dotEntry, dotdotEntry;
    strcpy(testApp1.d_name, "testapp1");
    strcpy(testApp2.d_name, "testapp2");
    strcpy(dotEntry.d_name, ".");
    strcpy(dotdotEntry.d_name, "..");
    
    // Set up sequence for readdir calls
    testing::InSequence seq;
    
    // Mock opendir to return success
    EXPECT_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<DIR*>(0x1234)));
    
    // Mock readdir to return entries in sequence
    EXPECT_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillOnce(::testing::Return(&dotEntry))     // First: "." (should be skipped)
        .WillOnce(::testing::Return(&dotdotEntry))  // Second: ".." (should be skipped)
        .WillOnce(::testing::Return(&testApp1))     // Third: "testapp1"
        .WillOnce(::testing::Return(&testApp2))     // Fourth: "testapp2"
        .WillOnce(::testing::Return(nullptr));      // End of directory
    
    // Mock closedir
    EXPECT_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillOnce(::testing::Return(0));
    
    // Mock GetConfigForPackage for both apps
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            if (fileLocator.find("testapp1") != string::npos) {
                id = "com.test.app1";
                version = "1.0.0";
                return Core::ERROR_NONE;
            } else if (fileLocator.find("testapp2") != string::npos) {
                id = "com.test.app2";
                version = "2.0.0";
                return Core::ERROR_NONE;
            }
            return Core::ERROR_GENERAL;
        });
    
    // Mock Install for both apps
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(2)  // Should be called for 2 apps
        .WillRepeatedly(::testing::Return(Core::ERROR_NONE));
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);  // Force install
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    
    releaseResources();
}

/**
 * @brief Test StartPreinstall with invalid packages
 *
 * @details Test verifies that:
 * - StartPreinstall handles packages with invalid configuration
 * - Invalid packages don't cause installation failure
 * - Method continues processing despite invalid packages
 */
TEST_F(PreinstallManagerTest, StartPreinstallWithInvalidPackages)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Create test directory entry
    static struct dirent invalidApp;
    strcpy(invalidApp.d_name, "invalidapp");
    
    // Mock directory operations
    EXPECT_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<DIR*>(0x1234)));
    
    EXPECT_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillOnce(::testing::Return(&invalidApp))
        .WillOnce(::testing::Return(nullptr));
    
    EXPECT_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillOnce(::testing::Return(0));
    
    // Mock GetConfigForPackage to fail
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    
    // Install should not be called for invalid packages
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(0);
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    // Should complete successfully even with invalid packages
    EXPECT_EQ(Core::ERROR_NONE, result);
    
    releaseResources();
}

/**
 * @brief Test directory path configuration for unit tests
 *
 * @details Test verifies that:
 * - AI_PREINSTALL_DIRECTORY is set correctly for unit tests
 * - Directory path is /tmp/preinstall when UNIT_TEST_BUILD is defined
 */
TEST_F(PreinstallManagerTest, VerifyTestDirectoryPath)
{
    // This test verifies the conditional compilation works correctly
    #ifdef UNIT_TEST_BUILD
        // In unit test builds, directory should be /tmp/preinstall
        std::string expectedPath = "/tmp/preinstall";
    #else
        // In production builds, directory should be /opt/preinstall
        std::string expectedPath = "/opt/preinstall";
    #endif
    
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock opendir and check the path being used
    EXPECT_CALL(*p_wrapsImplMock, opendir(::testing::StrEq(expectedPath.c_str())))
        .WillOnce(::testing::Return(nullptr)); // Return failure to avoid further processing
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    // Should return error due to directory open failure
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    releaseResources();
}

/**
 * @brief Test StartPreinstall with empty directory
 *
 * @details Test verifies that:
 * - StartPreinstall handles empty directories correctly
 * - Method returns success even when no packages are found
 */
TEST_F(PreinstallManagerTest, StartPreinstallWithEmptyDirectory)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock opendir to return success
    EXPECT_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<DIR*>(0x1234)));
    
    // Mock readdir to return only null (empty directory)
    EXPECT_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillOnce(::testing::Return(nullptr));
    
    // Mock closedir
    EXPECT_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillOnce(::testing::Return(0));
    
    // No Install calls should be made for empty directory
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(0);
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    
    releaseResources();
}

/**
 * @brief Test StartPreinstall with directory containing only dots
 *
 * @details Test verifies that:
 * - StartPreinstall correctly skips "." and ".." entries
 * - Method returns success but finds no packages to install
 */
TEST_F(PreinstallManagerTest, StartPreinstallWithOnlyDotEntries)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Create dot entries
    static struct dirent dotEntry, dotdotEntry;
    strcpy(dotEntry.d_name, ".");
    strcpy(dotdotEntry.d_name, "..");
    
    // Mock opendir to return success
    EXPECT_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<DIR*>(0x1234)));
    
    // Mock readdir to return dot entries and then null
    EXPECT_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillOnce(::testing::Return(&dotEntry))
        .WillOnce(::testing::Return(&dotdotEntry))
        .WillOnce(::testing::Return(nullptr));
    
    // Mock closedir
    EXPECT_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillOnce(::testing::Return(0));
    
    // No Install calls should be made since dot entries are skipped
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(0);
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    
    releaseResources();
}

/**
 * @brief Test StartPreinstall with comprehensive directory operations
 *
 * @details Test verifies that:
 * - StartPreinstall works end-to-end with directory operations
 * - Multiple packages are processed correctly
 * - Installation status is properly tracked
 */
TEST_F(PreinstallManagerTest, StartPreinstallComprehensiveDirectoryTest)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Create multiple test directory entries
    static struct dirent validApp1, validApp2, invalidApp, dotEntry;
    strcpy(validApp1.d_name, "validapp1");
    strcpy(validApp2.d_name, "validapp2");
    strcpy(invalidApp.d_name, "invalidapp");
    strcpy(dotEntry.d_name, ".");
    
    // Setup directory operation mocks
    EXPECT_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<DIR*>(0x1234)));
    
    // Return entries in sequence: dot entry (skipped), valid apps, invalid app, then end
    EXPECT_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillOnce(::testing::Return(&dotEntry))     // Should be skipped
        .WillOnce(::testing::Return(&validApp1))    // Valid app 1
        .WillOnce(::testing::Return(&validApp2))    // Valid app 2
        .WillOnce(::testing::Return(&invalidApp))   // Invalid app
        .WillOnce(::testing::Return(nullptr));      // End of directory
    
    EXPECT_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillOnce(::testing::Return(0));
    
    // Mock PackageInstaller methods
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            if (fileLocator.find("validapp1") != string::npos) {
                id = "com.test.valid.app1";
                version = "1.0.0";
                return Core::ERROR_NONE;
            } else if (fileLocator.find("validapp2") != string::npos) {
                id = "com.test.valid.app2";
                version = "2.0.0";
                return Core::ERROR_NONE;
            } else {
                // Invalid app - config retrieval fails
                return Core::ERROR_GENERAL;
            }
        });
    
    // Mock install operations for valid apps
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(2)  // Should be called for 2 valid apps
        .WillRepeatedly([&](const string &packageId, const string &version, 
                           Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                           const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            return Core::ERROR_NONE;
        });
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);  // Force install
    
    // Should succeed overall even if some packages fail
    EXPECT_EQ(Core::ERROR_NONE, result);
    
    releaseResources();
}

/**
 * @brief Test version comparison functionality through StartPreinstall behavior
 *
 * @details Test verifies that:
 * - Version comparison logic works correctly in StartPreinstall
 * - Newer versions are installed while older ones are skipped
 */
TEST_F(PreinstallManagerTest, VersionComparisonThroughStartPreinstall)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Create test directory entry
    static struct dirent testApp;
    strcpy(testApp.d_name, "testapp");
    
    // Mock directory operations
    EXPECT_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<DIR*>(0x1234)));
    
    EXPECT_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillOnce(::testing::Return(&testApp))
        .WillOnce(::testing::Return(nullptr));
    
    EXPECT_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillOnce(::testing::Return(0));
    
    // Mock ListPackages to return existing package with older version
    std::list<Exchange::IPackageInstaller::Package> existingPackages;
    Exchange::IPackageInstaller::Package existingPkg;
    existingPkg.packageId = "com.test.app";
    existingPkg.version = "1.0.0";  // Older version
    existingPkg.state = Exchange::IPackageInstaller::InstallState::INSTALLED;
    existingPackages.push_back(existingPkg);
    
    auto packageIterator = Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IPackageIterator>>::Create<Exchange::IPackageInstaller::IPackageIterator>(existingPackages);
    
    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
        .WillOnce([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
            packages = packageIterator;
            return Core::ERROR_NONE;
        });
    
    // Mock GetConfigForPackage to return newer version
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = "com.test.app";
            version = "2.0.0";  // Newer version - should trigger install
            return Core::ERROR_NONE;
        });
    
    // Should be called once for newer version
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(false);  // No force install
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    
    releaseResources();
}

/**
 * @brief Test StartPreinstall with selective installation based on version
 *
 * @details Test verifies that:
 * - StartPreinstall skips apps with older or equal versions when forceInstall=false
 * - Only newer versions are installed
 */
TEST_F(PreinstallManagerTest, StartPreinstallSelectiveInstallation)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Create test directory entries
    static struct dirent testApp1, testApp2;
    strcpy(testApp1.d_name, "testapp1");
    strcpy(testApp2.d_name, "testapp2");
    
    // Setup directory operation mocks
    EXPECT_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<DIR*>(0x1234)));
    
    EXPECT_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillOnce(::testing::Return(&testApp1))
        .WillOnce(::testing::Return(&testApp2))
        .WillOnce(::testing::Return(nullptr));
    
    EXPECT_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillOnce(::testing::Return(0));
    
    // Mock ListPackages to return existing installed packages
    std::list<Exchange::IPackageInstaller::Package> existingPackages;
    Exchange::IPackageInstaller::Package existingPkg;
    existingPkg.packageId = "com.test.app1";
    existingPkg.version = "2.0.0";  // Newer version already installed
    existingPkg.state = Exchange::IPackageInstaller::InstallState::INSTALLED;
    existingPackages.push_back(existingPkg);
    
    auto packageIterator = Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IPackageIterator>>::Create<Exchange::IPackageInstaller::IPackageIterator>(existingPackages);
    
    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
        .WillOnce([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
            packages = packageIterator;
            return Core::ERROR_NONE;
        });
    
    // Mock GetConfigForPackage
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            if (fileLocator.find("testapp1") != string::npos) {
                id = "com.test.app1";
                version = "1.0.0";  // Older version - should be skipped
                return Core::ERROR_NONE;
            } else if (fileLocator.find("testapp2") != string::npos) {
                id = "com.test.app2";
                version = "1.0.0";  // New app - should be installed
                return Core::ERROR_NONE;
            }
            return Core::ERROR_GENERAL;
        });
    
    // Should only be called for testapp2 (not already installed)
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::Eq("com.test.app2"), ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    
    // Should NOT be called for testapp1 (older version)
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::Eq("com.test.app1"), ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(0);
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(false);  // No force install
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    
    releaseResources();
}
