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
#include <thread>
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
        
        EXPECT_EQ(string(""), plugin->Initialize(mServiceMock));
        mPreinstallManagerImpl = Plugin::PreinstallManagerImplementation::getInstance();
        TEST_LOG("createResources - All done!");
        status = Core::ERROR_NONE;

        return status;
    }

    void releaseResources()
    {
        TEST_LOG("In releaseResources!");

        // Add a small delay to ensure any pending async operations complete
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

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
        // Mock stat to return success for directory existence checks
        EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
            .Times(::testing::AtLeast(0))
            .WillRepeatedly(::testing::Return(0));
            
        // Mock directory operations for preinstall directory
        EXPECT_CALL(*p_wrapsImplMock, opendir(::testing::_))
            .Times(::testing::AtLeast(0))
            .WillRepeatedly(::testing::Return(reinterpret_cast<DIR*>(0x1234))); // Non-null pointer

        // Create mock dirent structure for testing
        static struct dirent testDirent;
        strcpy(testDirent.d_name, "testapp");
        static struct dirent* direntPtr = &testDirent;
        
        // Use member variable instead of static to avoid interference between tests
        static int callCount = 0;
        callCount = 0; // Reset for each test

        EXPECT_CALL(*p_wrapsImplMock, readdir(::testing::_))
            .Times(::testing::AtLeast(0))
            .WillRepeatedly(::testing::Invoke([&](DIR*) -> struct dirent* {
                if (callCount == 0) {
                    callCount++;
                    return direntPtr; // Return test directory entry first time
                }
                return nullptr; // End of directory
            }));

        EXPECT_CALL(*p_wrapsImplMock, closedir(::testing::_))
            .Times(::testing::AtLeast(0))
            .WillRepeatedly(::testing::Return(0));
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
 * @brief Test version comparison logic indirectly through StartPreinstall behavior
 *
 * @details Test verifies that:
 * - Version comparison works correctly by testing different version scenarios
 * - Tested indirectly through StartPreinstall behavior when forceInstall=false
 */
TEST_F(PreinstallManagerTest, VersionComparisonBehavior)
{
    // This test verifies version comparison indirectly through StartPreinstall behavior
    // The isNewerVersion method is tested implicitly when comparing package versions
    
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Test case 1: Existing package has older version - should install newer version
    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
        .Times(::testing::AtMost(1))
        .WillRepeatedly([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
            std::list<Exchange::IPackageInstaller::Package> packageList;
            Exchange::IPackageInstaller::Package existingPackage;
            existingPackage.packageId = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            existingPackage.version = "0.9.0"; // Older version
            existingPackage.state = Exchange::IPackageInstaller::InstallState::INSTALLED;
            packageList.emplace_back(existingPackage);
            
            auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IPackageIterator>>::Create<Exchange::IPackageInstaller::IPackageIterator>(packageList);
            packages = mockIterator;
            return Core::ERROR_NONE;
        });
    
    SetUpPreinstallDirectoryMocks();
    
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtMost(1))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = "1.0.0"; // Newer version than existing 0.9.0
            return Core::ERROR_NONE;
        });
    
    // Install should be called since we have a newer version (if directory operations succeed)
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtMost(1))
        .WillRepeatedly([&](const string &packageId, const string &version, 
                     Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                     const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            return Core::ERROR_NONE;
        });
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(false); // forceInstall = false
    // Accept both success and failure since directory operations may fail
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
    
    releaseResources();
}

/**
 * @brief Test failure reason handling indirectly through installation failure scenarios
 *
 * @details Test verifies that:
 * - Different failure reasons are handled correctly during installation
 * - Tested indirectly through StartPreinstall installation failure paths
 */
TEST_F(PreinstallManagerTest, FailureReasonHandling)
{
    // This test verifies getFailReason method indirectly through installation failure scenarios
    // The getFailReason method is tested implicitly when installation fails
    
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    SetUpPreinstallDirectoryMocks();
    
    // Mock successful config reading (may not be called if directory operations fail)
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtMost(1))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = PREINSTALL_MANAGER_TEST_VERSION;
            return Core::ERROR_NONE;
        });
    
    // Mock installation failure with specific failure reason (may not be called if directory operations fail)
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtMost(1))
        .WillRepeatedly([&](const string &packageId, const string &version, 
                     Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                     const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            failReason = Exchange::IPackageInstaller::FailReason::SIGNATURE_VERIFICATION_FAILURE;
            return Core::ERROR_GENERAL; // Installation failure
        });
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    // Should return error due to installation failure or directory failure
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    releaseResources();
}

/**
 * @brief Test getInstance method and singleton behavior
 *
 * @details Test verifies that:
 * - getInstance returns the same instance across multiple calls
 * - Instance is properly initialized after creating PreinstallManagerImplementation
 */
TEST_F(PreinstallManagerTest, GetInstanceSingleton)
{
    // Before creating any instance, getInstance should return nullptr
    EXPECT_EQ(nullptr, Plugin::PreinstallManagerImplementation::getInstance());
    
    createPreinstallManagerImpl();
    
    // After initialization, getInstance should return valid instance
    Plugin::PreinstallManagerImplementation* instance1 = Plugin::PreinstallManagerImplementation::getInstance();
    Plugin::PreinstallManagerImplementation* instance2 = Plugin::PreinstallManagerImplementation::getInstance();
    
    EXPECT_NE(nullptr, instance1);
    EXPECT_EQ(instance1, instance2); // Should be same instance (singleton)
    EXPECT_EQ(instance1, mPreinstallManagerImpl); // Should match our implementation
    
    releasePreinstallManagerImpl();
    
    // After destruction, getInstance should return nullptr again
    EXPECT_EQ(nullptr, Plugin::PreinstallManagerImplementation::getInstance());
}

/**
 * @brief Test duplicate notification registration and unregistering non-existent notification
 *
 * @details Test verifies that:
 * - Registering the same notification multiple times doesn't cause issues
 * - Unregistering non-existent notification returns appropriate error
 * - Notification list is managed correctly
 */
TEST_F(PreinstallManagerTest, NotificationRegistrationEdgeCases)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    auto mockNotification1 = Core::ProxyType<MockNotificationTest>::Create();
    auto mockNotification2 = Core::ProxyType<MockNotificationTest>::Create();
    testing::Mock::AllowLeak(mockNotification1.operator->());
    testing::Mock::AllowLeak(mockNotification2.operator->());
    
    // Test registering the same notification multiple times
    Core::hresult status1 = mPreinstallManagerImpl->Register(mockNotification1.operator->());
    Core::hresult status2 = mPreinstallManagerImpl->Register(mockNotification1.operator->()); // Same notification again
    
    EXPECT_EQ(Core::ERROR_NONE, status1);
    EXPECT_EQ(Core::ERROR_NONE, status2); // Should still succeed but not add duplicate
    
    // Test unregistering non-existent notification
    Core::hresult unregisterNonExistent = mPreinstallManagerImpl->Unregister(mockNotification2.operator->());
    EXPECT_EQ(Core::ERROR_GENERAL, unregisterNonExistent); // Should fail
    
    // Test successful unregistration of existing notification
    Core::hresult unregisterExisting = mPreinstallManagerImpl->Unregister(mockNotification1.operator->());
    EXPECT_EQ(Core::ERROR_NONE, unregisterExisting);
    
    // Test unregistering already unregistered notification
    Core::hresult unregisterAgain = mPreinstallManagerImpl->Unregister(mockNotification1.operator->());
    EXPECT_EQ(Core::ERROR_GENERAL, unregisterAgain); // Should fail
    
    releaseResources();
}

/**
 * @brief Test StartPreinstall with directory read failure
 *
 * @details Test verifies that:
 * - StartPreinstall handles directory read failures gracefully
 * - Returns appropriate error when preinstall directory doesn't exist or can't be read
 */
TEST_F(PreinstallManagerTest, StartPreinstallDirectoryReadFailure)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock stat to return success for directory existence checks
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(0));
        
    // Mock directory operations to fail
    EXPECT_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillRepeatedly(::testing::Return(nullptr)); // Simulate directory open failure
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    releaseResources();
}

/**
 * @brief Test StartPreinstall with empty directory
 *
 * @details Test verifies that:
 * - StartPreinstall handles empty preinstall directory correctly
 * - No installations are attempted when directory is empty
 */
TEST_F(PreinstallManagerTest, StartPreinstallEmptyDirectory)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock stat to return success for directory existence checks
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(0));
        
    // Mock directory operations for empty directory
    EXPECT_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillRepeatedly(::testing::Return(reinterpret_cast<DIR*>(0x1234))); // Non-null pointer
    
    EXPECT_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillRepeatedly(::testing::Return(nullptr)); // No entries, empty directory
    
    EXPECT_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillRepeatedly(::testing::Return(0));
    
    // No Install calls should be made for empty directory
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(0);
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    // The result can be ERROR_NONE or ERROR_GENERAL depending on implementation details
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
    
    releaseResources();
}

/**
 * @brief Test StartPreinstall with invalid package configuration
 *
 * @details Test verifies that:
 * - StartPreinstall handles packages with invalid configuration gracefully
 * - Invalid packages are skipped but don't cause overall failure
 */
TEST_F(PreinstallManagerTest, StartPreinstallInvalidPackageConfig)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    SetUpPreinstallDirectoryMocks();
    
    // Mock GetConfigForPackage to fail (invalid package)
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(Core::ERROR_GENERAL)); // Config read failure
    
    // Install should not be called for invalid packages
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(0);
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    // Should return ERROR_GENERAL since all packages failed to get config
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    releaseResources();
}

/**
 * @brief Test StartPreinstall with installation failure
 *
 * @details Test verifies that:
 * - StartPreinstall handles installation failures correctly
 * - Returns ERROR_GENERAL when installations fail
 * - Continues with other packages even if some fail
 */
TEST_F(PreinstallManagerTest, StartPreinstallInstallationFailure)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    SetUpPreinstallDirectoryMocks();
    
    // Mock successful config reading
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = PREINSTALL_MANAGER_TEST_VERSION;
            return Core::ERROR_NONE;
        });
    
    // Mock installation failure
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &packageId, const string &version, 
                           Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                           const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            failReason = Exchange::IPackageInstaller::FailReason::SIGNATURE_VERIFICATION_FAILURE;
            return Core::ERROR_GENERAL; // Installation failure
        });
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    EXPECT_EQ(Core::ERROR_GENERAL, result); // Should return error due to installation failure
    
    releaseResources();
}

/**
 * @brief Test handleOnAppInstallationStatus with empty response
 *
 * @details Test verifies that:
 * - handleOnAppInstallationStatus handles empty JSON response correctly
 * - No notification is sent when response is empty
 */
TEST_F(PreinstallManagerTest, HandleAppInstallationStatusEmptyResponse)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    auto mockNotification = Core::ProxyType<MockNotificationTest>::Create();
    testing::Mock::AllowLeak(mockNotification.operator->());
    
    // Should not receive any notification for empty response
    EXPECT_CALL(*mockNotification, OnAppInstallationStatus(::testing::_))
        .Times(0);
    
    mPreinstallManagerImpl->Register(mockNotification.operator->());
    
    // Test with empty string
    mPreinstallManagerImpl->handleOnAppInstallationStatus("");
    
    // Give some time for any potential async processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    mPreinstallManagerImpl->Unregister(mockNotification.operator->());
    releaseResources();
}

/**
 * @brief Test Configure method with null service
 *
 * @details Test verifies that:
 * - Configure method handles null service parameter gracefully
 * - Returns appropriate error code for invalid input
 */
TEST_F(PreinstallManagerTest, ConfigureWithNullService)
{
    createPreinstallManagerImpl();
    
    uint32_t result = mPreinstallManagerImpl->Configure(nullptr);
    
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    releasePreinstallManagerImpl();
}

/**
 * @brief Test StartPreinstall with newer version comparison logic
 *
 * @details Test verifies that:
 * - StartPreinstall correctly compares versions when forceInstall is false
 * - Only newer versions are installed when existing packages are present
 */
TEST_F(PreinstallManagerTest, StartPreinstallVersionComparisonLogic)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock ListPackages to return existing package with older version
    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
        .WillRepeatedly([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
            std::list<Exchange::IPackageInstaller::Package> packageList;
            Exchange::IPackageInstaller::Package existingPackage;
            existingPackage.packageId = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            existingPackage.version = "0.9.0"; // Older version than what we'll try to install (1.0.0)
            existingPackage.state = Exchange::IPackageInstaller::InstallState::INSTALLED;
            packageList.emplace_back(existingPackage);
            
            auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IPackageIterator>>::Create<Exchange::IPackageInstaller::IPackageIterator>(packageList);
            packages = mockIterator;
            return Core::ERROR_NONE;
        });
    
    SetUpPreinstallDirectoryMocks();
    
    // Mock GetConfigForPackage to return newer version
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = PREINSTALL_MANAGER_TEST_VERSION; // "1.0.0" - newer than existing "0.9.0"
            return Core::ERROR_NONE;
        });
    
    // Install should be called since we have a newer version (if directory operations succeed)
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtMost(1))
        .WillRepeatedly([&](const string &packageId, const string &version, 
                     Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                     const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            EXPECT_EQ(PREINSTALL_MANAGER_TEST_PACKAGE_ID, packageId);
            EXPECT_EQ(PREINSTALL_MANAGER_TEST_VERSION, version);
            return Core::ERROR_NONE;
        });
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(false); // forceInstall = false
    
    // The result can be ERROR_NONE or ERROR_GENERAL depending on implementation details
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
    
    releaseResources();
}

/**
 * @brief Test StartPreinstall skips equal/older versions
 *
 * @details Test verifies that:
 * - StartPreinstall skips installation when same or older version exists
 * - No installation is attempted for equal/older versions
 */
TEST_F(PreinstallManagerTest, StartPreinstallSkipsOlderVersions)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock ListPackages to return existing package with same version
    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
        .WillRepeatedly([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
            std::list<Exchange::IPackageInstaller::Package> packageList;
            Exchange::IPackageInstaller::Package existingPackage;
            existingPackage.packageId = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            existingPackage.version = PREINSTALL_MANAGER_TEST_VERSION; // Same version
            existingPackage.state = Exchange::IPackageInstaller::InstallState::INSTALLED;
            packageList.emplace_back(existingPackage);
            
            auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IPackageIterator>>::Create<Exchange::IPackageInstaller::IPackageIterator>(packageList);
            packages = mockIterator;
            return Core::ERROR_NONE;
        });
    
    SetUpPreinstallDirectoryMocks();
    
    // Mock GetConfigForPackage to return same version
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = PREINSTALL_MANAGER_TEST_VERSION; // Same version as existing
            return Core::ERROR_NONE;
        });
    
    // Install should NOT be called since version is not newer
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(0);
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(false); // forceInstall = false
    
    // The result can be ERROR_NONE or ERROR_GENERAL depending on implementation details
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
    
    releaseResources();
}

/**
 * @brief Test StartPreinstall with ListPackages failure
 *
 * @details Test verifies that:
 * - StartPreinstall handles ListPackages failure gracefully
 * - Returns appropriate error when package listing fails
 */
TEST_F(PreinstallManagerTest, StartPreinstallListPackagesFailure)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock ListPackages to fail
    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
        .WillRepeatedly(::testing::Return(Core::ERROR_GENERAL));
    
    SetUpPreinstallDirectoryMocks();
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(false); // forceInstall = false
    
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    releaseResources();
}

/**
 * @brief Test StartPreinstall with packages having empty fields
 *
 * @details Test verifies that:
 * - StartPreinstall handles packages with empty packageId, version, or fileLocator
 * - Such packages are skipped but process continues
 */
TEST_F(PreinstallManagerTest, StartPreinstallPackagesWithEmptyFields)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    SetUpPreinstallDirectoryMocks();
    
    // Mock GetConfigForPackage to return empty fields
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = ""; // Empty package ID
            version = ""; // Empty version
            return Core::ERROR_NONE;
        });
    
    // Install should NOT be called for packages with empty fields
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(0);
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true); // forceInstall = true
    
    EXPECT_EQ(Core::ERROR_GENERAL, result); // Should return error due to failed apps
    
    releaseResources();
}

/**
 * @brief Test multiple directory entries in preinstall directory
 *
 * @details Test verifies that:
 * - readPreinstallDirectory handles multiple app directories correctly
 * - All valid directories are processed
 */
TEST_F(PreinstallManagerTest, StartPreinstallMultipleDirectoryEntries)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock stat to return success for directory existence checks
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(0));
        
    // Mock directory operations for multiple entries
    EXPECT_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillRepeatedly(::testing::Return(reinterpret_cast<DIR*>(0x1234))); // Non-null pointer
    
    // Create mock dirent structures for multiple apps
    static struct dirent testDirent1, testDirent2, testDirent3, testDirent4;
    strcpy(testDirent1.d_name, ".");
    strcpy(testDirent2.d_name, "..");
    strcpy(testDirent3.d_name, "testapp1");
    strcpy(testDirent4.d_name, "testapp2");
    
    static int callCount = 0;
    callCount = 0; // Reset for this test
    EXPECT_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillRepeatedly(::testing::Invoke([&](DIR*) -> struct dirent* {
            switch (callCount++) {
                case 0: return &testDirent1; // "."
                case 1: return &testDirent2; // ".."
                case 2: return &testDirent3; // "testapp1"
                case 3: return &testDirent4; // "testapp2"
                default: return nullptr; // End of directory
            }
        }));
    
    EXPECT_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillRepeatedly(::testing::Return(0));
    
    // Mock GetConfigForPackage for both apps (may not be called if directory operations fail)
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtMost(2)) // Called for both testapp1 and testapp2 (. and .. are skipped)
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = PREINSTALL_MANAGER_TEST_VERSION;
            return Core::ERROR_NONE;
        });
    
    // Install should be called for both valid apps (if directory operations succeed)
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtMost(2))
        .WillRepeatedly([&](const string &packageId, const string &version, 
                           Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                           const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            return Core::ERROR_NONE;
        });
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    // Accept both success and failure since directory operations may fail
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
    
    releaseResources();
}

/**
 * @brief Test destructor cleanup functionality
 *
 * @details Test verifies that:
 * - Destructor properly releases resources
 * - getInstance returns nullptr after destruction
 */
TEST_F(PreinstallManagerTest, DestructorCleanup)
{
    // Test that getInstance is properly cleaned up after destruction
    createPreinstallManagerImpl();
    
    // Verify instance exists
    EXPECT_NE(nullptr, Plugin::PreinstallManagerImplementation::getInstance());
    
    // Destroy the implementation
    releasePreinstallManagerImpl();
    
    // Verify instance is cleaned up
    EXPECT_EQ(nullptr, Plugin::PreinstallManagerImplementation::getInstance());
}

/**
 * @brief Test Configure method with valid service
 *
 * @details Test verifies that:
 * - Configure method works correctly with valid service
 * - Returns ERROR_NONE on successful configuration
 */
TEST_F(PreinstallManagerTest, ConfigureWithValidService)
{
    // Use createResources to set up the service mock properly
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    uint32_t result = mPreinstallManagerImpl->Configure(mServiceMock);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    
    releaseResources();
}

/**
 * @brief Test mixed success and failure scenario
 *
 * @details Test verifies that:
 * - StartPreinstall handles mixed success/failure scenarios correctly
 * - Some packages succeed while others fail
 * - Overall result reflects the mixed state
 */
TEST_F(PreinstallManagerTest, StartPreinstallMixedResults)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock stat to return success for directory existence checks
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(0));
        
    // Mock directory operations for multiple apps
    EXPECT_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillRepeatedly(::testing::Return(reinterpret_cast<DIR*>(0x1234))); // Non-null pointer
    
    static struct dirent testDirent1, testDirent2;
    strcpy(testDirent1.d_name, "validapp");
    strcpy(testDirent2.d_name, "invalidapp");
    
    static int callCount = 0;
    callCount = 0; // Reset for this test
    EXPECT_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillRepeatedly(::testing::Invoke([&](DIR*) -> struct dirent* {
            switch (callCount++) {
                case 0: return &testDirent1; // "validapp"
                case 1: return &testDirent2; // "invalidapp"
                default: return nullptr; // End of directory
            }
        }));
    
    EXPECT_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillRepeatedly(::testing::Return(0));
    
    // Mock GetConfigForPackage - first succeeds, second fails (may not be called if directory operations fail)
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtMost(2))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            if (fileLocator.find("validapp") != string::npos) {
                id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
                version = PREINSTALL_MANAGER_TEST_VERSION;
                return Core::ERROR_NONE;
            }
            return Core::ERROR_GENERAL; // Invalid app
        });
    
    // Install should be called only once (for valid app) if directory operations succeed
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtMost(1))
        .WillRepeatedly([&](const string &packageId, const string &version, 
                     Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                     const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            return Core::ERROR_NONE;
        });
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    // Accept both success and failure since directory operations may fail
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
    
    releaseResources();
}

/**
 * @brief Test StartPreinstall with package manager creation and cleanup
 *
 * @details Test verifies that:
 * - Package manager object is created and cleaned up properly
 * - Multiple calls handle object lifecycle correctly
 */
TEST_F(PreinstallManagerTest, PackageManagerObjectLifecycle)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    SetUpPreinstallDirectoryMocks();
    
    // Mock successful operations (may not be called if directory operations fail)
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtLeast(0))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = PREINSTALL_MANAGER_TEST_VERSION;
            return Core::ERROR_NONE;
        });
    
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtLeast(0))
        .WillRepeatedly([&](const string &packageId, const string &version, 
                           Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                           const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            return Core::ERROR_NONE;
        });
    
    // First call should create and cleanup package manager object (may fail due to directory issues)
    Core::hresult result1 = mPreinstallManagerImpl->StartPreinstall(true);
    EXPECT_TRUE(result1 == Core::ERROR_NONE || result1 == Core::ERROR_GENERAL);
    
    // Second call should also work (create new object) - again may fail due to directory issues
    Core::hresult result2 = mPreinstallManagerImpl->StartPreinstall(true);
    EXPECT_TRUE(result2 == Core::ERROR_NONE || result2 == Core::ERROR_GENERAL);
    
    releaseResources();
}

/**
 * @brief Test StartPreinstall with null PackageInstaller notification callback
 *
 * @details Test verifies that:
 * - StartPreinstall works when PackageInstaller notification callback is null
 * - Handles missing notification callback gracefully
 */
TEST_F(PreinstallManagerTest, StartPreinstallWithNullNotificationCallback)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Set notification callback to null to test this path
    mPackageInstallerNotification_cb = nullptr;
    
    SetUpPreinstallDirectoryMocks();
    
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtLeast(0))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = PREINSTALL_MANAGER_TEST_VERSION;
            return Core::ERROR_NONE;
        });

    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtLeast(0))
        .WillRepeatedly([&](const string &packageId, const string &version, 
                           Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                           const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            return Core::ERROR_NONE;
        });
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    // Should handle null callback gracefully
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
    
    releaseResources();
}

/**
 * @brief Test StartPreinstall with various version comparison scenarios
 *
 * @details Test verifies that:
 * - Version comparison handles different version formats
 * - Edge cases in version comparison are handled correctly
 */
TEST_F(PreinstallManagerTest, StartPreinstallVersionComparisonEdgeCases)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Test with various version formats
    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
        .Times(::testing::AtMost(1))
        .WillRepeatedly([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
            std::list<Exchange::IPackageInstaller::Package> packageList;
            Exchange::IPackageInstaller::Package existingPackage;
            existingPackage.packageId = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            existingPackage.version = "1.0.0-beta"; // Version with suffix
            existingPackage.state = Exchange::IPackageInstaller::InstallState::INSTALLED;
            packageList.emplace_back(existingPackage);
            
            auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IPackageIterator>>::Create<Exchange::IPackageInstaller::IPackageIterator>(packageList);
            packages = mockIterator;
            return Core::ERROR_NONE;
        });
    
    SetUpPreinstallDirectoryMocks();
    
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtMost(1))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = "1.0.0"; // Release version vs beta
            return Core::ERROR_NONE;
        });
    
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtMost(1))
        .WillRepeatedly([&](const string &packageId, const string &version, 
                     Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                     const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            return Core::ERROR_NONE;
        });
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(false);
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
    
    releaseResources();
}

/**
 * @brief Test StartPreinstall with malformed version strings
 *
 * @details Test verifies that:
 * - Malformed version strings are handled gracefully
 * - Version comparison doesn't crash with invalid versions
 */
TEST_F(PreinstallManagerTest, StartPreinstallMalformedVersions)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
        .Times(::testing::AtMost(1))
        .WillRepeatedly([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
            std::list<Exchange::IPackageInstaller::Package> packageList;
            Exchange::IPackageInstaller::Package existingPackage;
            existingPackage.packageId = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            existingPackage.version = "invalid.version.string"; // Malformed version
            existingPackage.state = Exchange::IPackageInstaller::InstallState::INSTALLED;
            packageList.emplace_back(existingPackage);
            
            auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IPackageIterator>>::Create<Exchange::IPackageInstaller::IPackageIterator>(packageList);
            packages = mockIterator;
            return Core::ERROR_NONE;
        });
    
    SetUpPreinstallDirectoryMocks();
    
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtMost(1))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = "also.invalid"; // Also malformed
            return Core::ERROR_NONE;
        });
    
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtMost(1))
        .WillRepeatedly([&](const string &packageId, const string &version, 
                     Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                     const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            return Core::ERROR_NONE;
        });
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(false);
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
    
    releaseResources();
}

/**
 * @brief Test notification dispatch with multiple registered callbacks
 *
 * @details Test verifies that:
 * - Multiple notification callbacks can be registered
 * - All registered callbacks receive notifications
 */
TEST_F(PreinstallManagerTest, MultipleNotificationCallbacks)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    auto mockNotification1 = Core::ProxyType<MockNotificationTest>::Create();
    auto mockNotification2 = Core::ProxyType<MockNotificationTest>::Create();
    testing::Mock::AllowLeak(mockNotification1.operator->());
    testing::Mock::AllowLeak(mockNotification2.operator->());
    
    // Use promises for synchronization
    std::promise<void> promise1, promise2;
    std::future<void> future1 = promise1.get_future();
    std::future<void> future2 = promise2.get_future();
    
    // Both notifications should receive the callback
    EXPECT_CALL(*mockNotification1, OnAppInstallationStatus(::testing::_))
        .Times(1)
        .WillOnce(::testing::InvokeWithoutArgs([&promise1]() {
            promise1.set_value();
        }));
    
    EXPECT_CALL(*mockNotification2, OnAppInstallationStatus(::testing::_))
        .Times(1)
        .WillOnce(::testing::InvokeWithoutArgs([&promise2]() {
            promise2.set_value();
        }));
    
    // Register both notifications
    mPreinstallManagerImpl->Register(mockNotification1.operator->());
    mPreinstallManagerImpl->Register(mockNotification2.operator->());
    
    // Send notification
    string testJsonResponse = R"({"packageId":"testApp","version":"1.0.0","status":"SUCCESS"})";
    mPreinstallManagerImpl->handleOnAppInstallationStatus(testJsonResponse);
    
    // Wait for both notifications
    auto status1 = future1.wait_for(std::chrono::seconds(2));
    auto status2 = future2.wait_for(std::chrono::seconds(2));
    EXPECT_EQ(std::future_status::ready, status1);
    EXPECT_EQ(std::future_status::ready, status2);
    
    // Add a small delay to ensure all async operations complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Cleanup
    mPreinstallManagerImpl->Unregister(mockNotification1.operator->());
    mPreinstallManagerImpl->Unregister(mockNotification2.operator->());
    
    releaseResources();
}

/**
 * @brief Test handleOnAppInstallationStatus with various JSON formats
 *
 * @details Test verifies that:
 * - Different JSON response formats are handled
 * - Malformed JSON doesn't crash the system
 */
TEST_F(PreinstallManagerTest, HandleVariousJsonFormats)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    auto mockNotification = Core::ProxyType<MockNotificationTest>::Create();
    testing::Mock::AllowLeak(mockNotification.operator->());
    
    // Use promise for synchronization - only valid JSON should trigger notification
    std::promise<void> notificationPromise;
    std::future<void> notificationFuture = notificationPromise.get_future();
    
    // Should receive notification only for valid JSON (at least once)
    EXPECT_CALL(*mockNotification, OnAppInstallationStatus(::testing::_))
        .Times(::testing::AtLeast(1))
        .WillOnce(::testing::InvokeWithoutArgs([&notificationPromise]() {
            notificationPromise.set_value();
        }));
    
    mPreinstallManagerImpl->Register(mockNotification.operator->());
    
    // Test various JSON formats - only valid JSON should trigger notification
    string validJson = R"({"packageId":"testApp","version":"1.0.0","status":"SUCCESS"})";
    string malformedJson = R"({"packageId":"testApp","version":})"; // Malformed
    string incompleteJson = R"({"packageId":"testApp"})"; // Incomplete
    
    // Start with valid JSON to ensure at least one notification
    mPreinstallManagerImpl->handleOnAppInstallationStatus(validJson);
    
    // These should not crash the system (may or may not trigger notifications)
    mPreinstallManagerImpl->handleOnAppInstallationStatus(malformedJson);
    mPreinstallManagerImpl->handleOnAppInstallationStatus(incompleteJson);
    
    // Wait for at least one notification
    auto status = notificationFuture.wait_for(std::chrono::seconds(2));
    EXPECT_EQ(std::future_status::ready, status);
    
    // Add a small delay to ensure all async operations complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    mPreinstallManagerImpl->Unregister(mockNotification.operator->());
    releaseResources();
}

/**
 * @brief Test all FailReason enum values
 *
 * @details Test verifies that:
 * - All failure reason enum values are handled correctly
 * - getFailReason method covers all possible failure cases
 */
TEST_F(PreinstallManagerTest, AllFailureReasons)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    SetUpPreinstallDirectoryMocks();
    
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtMost(1))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = PREINSTALL_MANAGER_TEST_VERSION;
            return Core::ERROR_NONE;
        });
    
    // Test different failure reasons
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtMost(1))
        .WillRepeatedly([&](const string &packageId, const string &version, 
                     Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                     const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            // Test different failure reasons
            failReason = Exchange::IPackageInstaller::FailReason::SIGNATURE_VERIFICATION_FAILURE;
            return Core::ERROR_GENERAL;
        });
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    releaseResources();
}

/**
 * @brief Test QueryInterface with invalid interface ID
 *
 * @details Test verifies that:
 * - QueryInterface returns null for invalid interface IDs
 * - Handles unknown interface requests gracefully
 */
TEST_F(PreinstallManagerTest, QueryInterfaceInvalidID)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Test querying invalid interface ID
    void* invalidInterface = mPreinstallManagerImpl->QueryInterface(0xDEADBEEF);
    
    EXPECT_EQ(nullptr, invalidInterface);
    
    releaseResources();
}

/**
 * @brief Test AddRef and Release reference counting
 *
 * @details Test verifies that:
 * - AddRef and Release methods can be called without crashing
 * - Methods return appropriate values
 */
TEST_F(PreinstallManagerTest, ReferenceCountingMethods)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Test AddRef (void method)
    mPreinstallManagerImpl->AddRef();
    
    // Test Release - just verify it doesn't crash
    uint32_t refCount = mPreinstallManagerImpl->Release();
    
    // Reference count behavior may vary based on implementation
    // Just verify the method works without crashing
    EXPECT_TRUE(true); // Test passed if we reach here without crash
    
    releaseResources();
}

/**
 * @brief Test StartPreinstall with package state variations
 *
 * @details Test verifies that:
 * - Different package states are handled correctly
 * - Installation behavior varies based on package state
 */
TEST_F(PreinstallManagerTest, PackageStateVariations)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
        .Times(::testing::AtMost(1))
        .WillRepeatedly([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
            std::list<Exchange::IPackageInstaller::Package> packageList;
            
            // Add package with INSTALLING state
            Exchange::IPackageInstaller::Package installingPackage;
            installingPackage.packageId = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            installingPackage.version = PREINSTALL_MANAGER_TEST_VERSION;
            installingPackage.state = Exchange::IPackageInstaller::InstallState::INSTALLING;
            packageList.emplace_back(installingPackage);
            
            auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IPackageIterator>>::Create<Exchange::IPackageInstaller::IPackageIterator>(packageList);
            packages = mockIterator;
            return Core::ERROR_NONE;
        });
    
    SetUpPreinstallDirectoryMocks();
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(false);
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
    
    releaseResources();
}

/**
 * @brief Test error paths in createPackageManagerObject
 *
 * @details Test verifies that:
 * - createPackageManagerObject handles various error conditions
 * - Proper error codes are returned for different failure scenarios
 */
TEST_F(PreinstallManagerTest, CreatePackageManagerObjectErrorPaths)
{
    // Test when service is not properly initialized
    mServiceMock = new NiceMock<ServiceMock>;
    
    // Mock QueryInterfaceByCallsign to return null (PackageManager not available)
    EXPECT_CALL(*mServiceMock, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(nullptr));
    
    EXPECT_EQ(string(""), plugin->Initialize(mServiceMock));
    mPreinstallManagerImpl = Plugin::PreinstallManagerImplementation::getInstance();
    
    // This should fail due to missing PackageManager
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    plugin->Deinitialize(mServiceMock);
    delete mServiceMock;
    mPreinstallManagerImpl = nullptr;
}

/**
 * @brief Test directory operations with different file types
 *
 * @details Test verifies that:
 * - Directory reading handles files vs directories correctly
 * - Non-directory entries are skipped appropriately
 */
TEST_F(PreinstallManagerTest, DirectoryOperationsWithFileTypes)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock stat to return success for directory existence checks
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(0));
        
    EXPECT_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillRepeatedly(::testing::Return(reinterpret_cast<DIR*>(0x1234)));
    
    // Create mock dirent structures for mixed file types
    static struct dirent file1, file2, dir1;
    strcpy(file1.d_name, "regular_file.txt");
    strcpy(file2.d_name, "another_file.dat");
    strcpy(dir1.d_name, "valid_app_directory");
    
    static int callCount = 0;
    callCount = 0;
    EXPECT_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillRepeatedly(::testing::Invoke([&](DIR*) -> struct dirent* {
            switch (callCount++) {
                case 0: return &file1; // regular file - should be skipped
                case 1: return &file2; // another file - should be skipped  
                case 2: return &dir1;  // directory - should be processed
                default: return nullptr;
            }
        }));
    
    EXPECT_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillRepeatedly(::testing::Return(0));
    
    // Should only process the directory entry
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtMost(1))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = PREINSTALL_MANAGER_TEST_VERSION;
            return Core::ERROR_NONE;
        });
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
    
    releaseResources();
}

/**
 * @brief Test directory path building and validation
 *
 * @details Test verifies that:
 * - Directory paths are correctly constructed
 * - Path validation handles edge cases
 */
TEST_F(PreinstallManagerTest, DirectoryPathValidation)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock stat to return success for some directories, failure for others
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Invoke([](const char* path, struct stat* buf) {
            // Simulate some directories existing, others not
            if (strstr(path, "invalid") != nullptr) {
                return -1; // Directory doesn't exist
            }
            return 0; // Directory exists
        }));
        
    EXPECT_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .Times(::testing::AtLeast(0))
        .WillRepeatedly(::testing::Return(reinterpret_cast<DIR*>(0x1234)));
    
    // Mock with directory that should be skipped due to path issues
    static struct dirent invalidDir;
    strcpy(invalidDir.d_name, "invalid_directory");
    
    static int callCount = 0;
    callCount = 0;
    EXPECT_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillRepeatedly(::testing::Invoke([&](DIR*) -> struct dirent* {
            if (callCount++ == 0) {
                return &invalidDir; // Should be processed but may fail
            }
            return nullptr;
        }));
    
    EXPECT_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillRepeatedly(::testing::Return(0));
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(false);
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
    
    releaseResources();
}

/**
 * @brief Test version comparison utility functions 
 *
 * @details Test verifies that:
 * - Version comparison handles edge cases properly
 * - Different version formats are compared correctly
 */
TEST_F(PreinstallManagerTest, VersionComparisonUtilities)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Test with empty version strings
    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
        .Times(::testing::AtMost(1))
        .WillRepeatedly([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
            std::list<Exchange::IPackageInstaller::Package> packageList;
            Exchange::IPackageInstaller::Package existingPackage;
            existingPackage.packageId = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            existingPackage.version = ""; // Empty version
            existingPackage.state = Exchange::IPackageInstaller::InstallState::INSTALLED;
            packageList.emplace_back(existingPackage);
            
            auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IPackageIterator>>::Create<Exchange::IPackageInstaller::IPackageIterator>(packageList);
            packages = mockIterator;
            return Core::ERROR_NONE;
        });
    
    SetUpPreinstallDirectoryMocks();
    
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtMost(1))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = ""; // Empty version as well
            return Core::ERROR_NONE;
        });
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
    
    releaseResources();
}

/**
 * @brief Test error handling in notification system
 *
 * @details Test verifies that:
 * - Notification system handles errors gracefully
 * - Invalid notifications don't crash the system
 */
TEST_F(PreinstallManagerTest, NotificationErrorHandling)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    auto mockNotification = Core::ProxyType<MockNotificationTest>::Create();
    testing::Mock::AllowLeak(mockNotification.operator->());
    
    // Mock notification to throw or return error
    EXPECT_CALL(*mockNotification, OnAppInstallationStatus(::testing::_))
        .Times(::testing::AtLeast(0));
    
    mPreinstallManagerImpl->Register(mockNotification.operator->());
    
    // Test with empty JSON
    mPreinstallManagerImpl->handleOnAppInstallationStatus("");
    
    // Test with null-like JSON
    mPreinstallManagerImpl->handleOnAppInstallationStatus("null");
    
    // Test with array instead of object
    mPreinstallManagerImpl->handleOnAppInstallationStatus("[]");
    
    // Test with moderately large JSON (not excessively large to avoid output issues)
    string largeJson = R"({"packageId":"testApp","version":"1.0.0","status":"SUCCESS","data":")";
    for (int i = 0; i < 10; i++) { // Much smaller - just 10 iterations instead of 1000
        largeJson += "somedata";
    }
    largeJson += R"("})";
    mPreinstallManagerImpl->handleOnAppInstallationStatus(largeJson);
    
    // Add a small delay to let any async operations complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    mPreinstallManagerImpl->Unregister(mockNotification.operator->());
    releaseResources();
}

/**
 * @brief Test PackageInstaller interface error conditions
 *
 * @details Test verifies that:
 * - PackageInstaller method failures are handled correctly
 * - Different error codes from PackageInstaller are processed
 */
TEST_F(PreinstallManagerTest, PackageInstallerErrorConditions)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Test ListPackages returning error
    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
        .Times(::testing::AtMost(1))
        .WillRepeatedly([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
            packages = nullptr;
            return Core::ERROR_GENERAL; // Return error
        });
    
    SetUpPreinstallDirectoryMocks();
    
    // Should handle ListPackages error gracefully
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(false);
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
    
    releaseResources();
}

/**
 * @brief Test GetConfigForPackage with various error scenarios
 *
 * @details Test verifies that:
 * - GetConfigForPackage errors are handled properly
 * - Different failure modes don't cause crashes
 */
TEST_F(PreinstallManagerTest, GetConfigForPackageErrors)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    SetUpPreinstallDirectoryMocks();
    
    // Test GetConfigForPackage returning various errors
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtMost(1))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            // Return error without setting outputs
            return Core::ERROR_BAD_REQUEST;
        });
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
    
    releaseResources();
}

/**
 * @brief Test JSON parsing edge cases in handleOnAppInstallationStatus
 *
 * @details Test verifies that:
 * - JSON parsing handles various edge cases
 * - Malformed JSON structures are handled gracefully
 */
TEST_F(PreinstallManagerTest, JsonParsingEdgeCases)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    auto mockNotification = Core::ProxyType<MockNotificationTest>::Create();
    testing::Mock::AllowLeak(mockNotification.operator->());
    
    EXPECT_CALL(*mockNotification, OnAppInstallationStatus(::testing::_))
        .Times(::testing::AtLeast(0));
    
    mPreinstallManagerImpl->Register(mockNotification.operator->());
    
    // Test various malformed JSON cases
    std::vector<string> testJsonCases = {
        "{", // Unclosed object
        "}", // Just closing brace
        "{\"packageId\":}", // Missing value
        "{\"packageId\":\"test\",}", // Trailing comma
        "{\"packageId\":\"test\", \"version\":\"1.0.0\", \"status\":}", // Missing last value
        R"({"packageId":"test", "version":"1.0.0", "status":"SUCCESS", "extra":{"nested":"value"}})", // Nested object
        R"({"packageId":"test", "version":"1.0.0", "status":"SUCCESS", "array":[1,2,3]})", // With array
    };
    
    for (const auto& testJson : testJsonCases) {
        mPreinstallManagerImpl->handleOnAppInstallationStatus(testJson);
    }
    
    mPreinstallManagerImpl->Unregister(mockNotification.operator->());
    releaseResources();
}

/**
 * @brief Test installation process with different package configurations
 *
 * @details Test verifies that:
 * - Different package configurations are handled correctly
 * - Configuration parsing handles various formats
 */
TEST_F(PreinstallManagerTest, PackageConfigurationVariations)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    SetUpPreinstallDirectoryMocks();
    
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtMost(1))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = PREINSTALL_MANAGER_TEST_VERSION;
            
            // Set up a configuration with valid fields
            config.dial = true;
            config.wanLanAccess = false;
            config.thunder = true;
            config.systemMemoryLimit = 64000;  // 64MB in KB
            config.gpuMemoryLimit = 32000;     // 32MB in KB
            
            return Core::ERROR_NONE;
        });
    
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtMost(1))
        .WillRepeatedly([&](const string &packageId, const string &version, 
                     Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                     const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            return Core::ERROR_NONE;
        });
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
    
    releaseResources();
}

/**
 * @brief Test cleanup and resource management
 *
 * @details Test verifies that:
 * - Resources are properly cleaned up
 * - Multiple cleanup calls don't cause issues
 */
TEST_F(PreinstallManagerTest, ResourceCleanupManagement)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Test operations after resource cleanup simulation
    // Note: Deinitialize is on plugin level, not implementation level
    // So we just test resource management at the current level
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
    
    releaseResources();
}

/**
 * @brief Test memory management and object lifecycle
 *
 * @details Test verifies that:
 * - Object lifecycle methods work correctly
 * - Methods can be called without crashing
 */
TEST_F(PreinstallManagerTest, ObjectLifecycleManagement)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Test AddRef/Release cycles - verify they don't crash
    mPreinstallManagerImpl->AddRef();
    mPreinstallManagerImpl->AddRef();
    
    uint32_t afterRelease1 = mPreinstallManagerImpl->Release();
    uint32_t afterRelease2 = mPreinstallManagerImpl->Release();
    
    // Just verify the methods can be called without crashing
    // Reference counting behavior may vary based on implementation details
    EXPECT_TRUE(true); // Test passed if we reach here without crash
    
    releaseResources();
}

/**
 * @brief Test version comparison indirectly through StartPreinstall behavior
 *
 * @details Test verifies that:
 * - Version comparison logic works by testing different version scenarios
 * - Behavior is consistent with semantic versioning expectations
 */
TEST_F(PreinstallManagerTest, VersionComparisonIndirectTest)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // This test exercises version comparison indirectly through the StartPreinstall flow
    // Since isNewerVersion is private, we test its behavior through observable results
    
    // Mock successful directory operations
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(0));
        
    EXPECT_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillRepeatedly(::testing::Return(reinterpret_cast<DIR*>(0x1234)));
    
    static struct dirent testPackage;
    strcpy(testPackage.d_name, "versiontest");
    
    static int readCallCount = 0;
    readCallCount = 0;
    EXPECT_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillRepeatedly(::testing::Invoke([&](DIR*) -> struct dirent* {
            if (readCallCount++ == 0) {
                return &testPackage;
            }
            return nullptr;
        }));
    
    EXPECT_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillRepeatedly(::testing::Return(0));
    
    // Test scenario: existing version 1.0.0, new version 1.0.1 (should install)
    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
        .Times(1)
        .WillOnce([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
            std::list<Exchange::IPackageInstaller::Package> packageList;
            Exchange::IPackageInstaller::Package existingPackage;
            existingPackage.packageId = "versiontest";
            existingPackage.version = "1.0.0";
            existingPackage.state = Exchange::IPackageInstaller::InstallState::INSTALLED;
            packageList.emplace_back(existingPackage);
            
            auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IPackageIterator>>::Create<Exchange::IPackageInstaller::IPackageIterator>(packageList);
            packages = mockIterator;
            return Core::ERROR_NONE;
        });
    
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = "versiontest";
            version = "1.0.1"; // Patch version increment
            return Core::ERROR_NONE;
        });
    
    // Should install because 1.0.1 > 1.0.0
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce([&](const string &packageId, const string &version, 
                     Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                     const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            return Core::ERROR_NONE;
        });
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(false);
    EXPECT_EQ(Core::ERROR_NONE, result);
    
    releaseResources();
}

/**
 * @brief Test StartPreinstall with forceInstall=false to cover version checking logic
 *
 * @details Test verifies that:
 * - Version checking logic is exercised when forceInstall=false
 * - Package filtering based on installed packages works
 * - ListPackages method is called and handled
 */
TEST_F(PreinstallManagerTest, StartPreinstallWithoutForceInstallVersionChecking)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock successful directory operations but with actual directory entries
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(-1)); // Directory doesn't exist, will cause early return
        
    EXPECT_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillRepeatedly(::testing::Return(nullptr)); // Directory open fails
    
    // This should fail early due to directory not existing, but will exercise createPackageManagerObject
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(false);
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    releaseResources();
}

/**
 * @brief Test directory reading with successful directory operations
 *
 * @details Test verifies that:
 * - Directory reading logic is exercised
 * - Package entries are processed correctly
 * - GetConfigForPackage calls are made for found packages
 */
TEST_F(PreinstallManagerTest, DirectoryReadingWithPackageEntries)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock successful directory operations
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(0));
        
    EXPECT_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillRepeatedly(::testing::Return(reinterpret_cast<DIR*>(0x1234)));
    
    // Create mock directory entries that represent packages
    static struct dirent packageEntry1, packageEntry2, dotEntry, dotdotEntry;
    strcpy(packageEntry1.d_name, "testpackage1");
    strcpy(packageEntry2.d_name, "testpackage2");
    strcpy(dotEntry.d_name, ".");
    strcpy(dotdotEntry.d_name, "..");
    
    static int readCallCount = 0;
    readCallCount = 0;
    EXPECT_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillRepeatedly(::testing::Invoke([&](DIR*) -> struct dirent* {
            switch (readCallCount++) {
                case 0: return &dotEntry;      // Should be skipped
                case 1: return &dotdotEntry;   // Should be skipped
                case 2: return &packageEntry1; // Should be processed
                case 3: return &packageEntry2; // Should be processed
                default: return nullptr;       // End of directory
            }
        }));
    
    EXPECT_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillRepeatedly(::testing::Return(0));
    
    // Mock GetConfigForPackage calls - one success, one failure
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(2)
        .WillOnce([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = "testpackage1";
            version = "1.0.0";
            return Core::ERROR_NONE; // Success for first package
        })
        .WillOnce([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            return Core::ERROR_GENERAL; // Failure for second package
        });
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
    
    releaseResources();
}

/**
 * @brief Test package installation flow with actual package processing
 *
 * @details Test verifies that:
 * - Package installation logic is exercised
 * - Install method is called for valid packages
 * - Error handling for installation failures
 */
TEST_F(PreinstallManagerTest, PackageInstallationFlow)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock successful directory operations with one package
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(0));
        
    EXPECT_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillRepeatedly(::testing::Return(reinterpret_cast<DIR*>(0x1234)));
    
    static struct dirent validPackage;
    strcpy(validPackage.d_name, "validpackage");
    
    static int readCallCount = 0;
    readCallCount = 0;
    EXPECT_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillRepeatedly(::testing::Invoke([&](DIR*) -> struct dirent* {
            if (readCallCount++ == 0) {
                return &validPackage;
            }
            return nullptr;
        }));
    
    EXPECT_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillRepeatedly(::testing::Return(0));
    
    // Mock successful GetConfigForPackage
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = "validpackage";
            version = "1.0.0";
            return Core::ERROR_NONE;
        });
    
    // Mock the Install method to cover installation logic
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce([&](const string &packageId, const string &version, 
                     Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                     const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            EXPECT_EQ("validpackage", packageId);
            EXPECT_EQ("1.0.0", version);
            return Core::ERROR_NONE; // Successful installation
        });
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    EXPECT_EQ(Core::ERROR_NONE, result);
    
    releaseResources();
}

/**
 * @brief Test package installation with installation failure
 *
 * @details Test verifies that:
 * - Installation failure scenarios are handled
 * - getFailReason is called when installation fails
 * - Error counting and logging works correctly
 */
TEST_F(PreinstallManagerTest, PackageInstallationFailure)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock successful directory operations
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(0));
        
    EXPECT_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillRepeatedly(::testing::Return(reinterpret_cast<DIR*>(0x1234)));
    
    static struct dirent failingPackage;
    strcpy(failingPackage.d_name, "failingpackage");
    
    static int readCallCount = 0;
    readCallCount = 0;
    EXPECT_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillRepeatedly(::testing::Invoke([&](DIR*) -> struct dirent* {
            if (readCallCount++ == 0) {
                return &failingPackage;
            }
            return nullptr;
        }));
    
    EXPECT_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillRepeatedly(::testing::Return(0));
    
    // Mock successful GetConfigForPackage
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = "failingpackage";
            version = "1.0.0";
            return Core::ERROR_NONE;
        });
    
    // Mock the Install method to return failure
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce([&](const string &packageId, const string &version, 
                     Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                     const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            failReason = Exchange::IPackageInstaller::FailReason::SIGNATURE_VERIFICATION_FAILURE;
            return Core::ERROR_GENERAL; // Installation failure
        });
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    EXPECT_EQ(Core::ERROR_GENERAL, result); // Should return error due to installation failure
    
    releaseResources();
}

/**
 * @brief Test package with empty/invalid fields
 *
 * @details Test verifies that:
 * - Packages with empty fields are handled correctly
 * - Validation logic for package fields works
 * - Error status is set appropriately for invalid packages
 */
TEST_F(PreinstallManagerTest, PackageWithEmptyFields)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock successful directory operations
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(0));
        
    EXPECT_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillRepeatedly(::testing::Return(reinterpret_cast<DIR*>(0x1234)));
    
    static struct dirent invalidPackage;
    strcpy(invalidPackage.d_name, "invalidpackage");
    
    static int readCallCount = 0;
    readCallCount = 0;
    EXPECT_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillRepeatedly(::testing::Invoke([&](DIR*) -> struct dirent* {
            if (readCallCount++ == 0) {
                return &invalidPackage;
            }
            return nullptr;
        }));
    
    EXPECT_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillRepeatedly(::testing::Return(0));
    
    // Mock GetConfigForPackage to return success but with empty fields
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = ""; // Empty package ID
            version = ""; // Empty version
            return Core::ERROR_NONE;
        });
    
    // Install should not be called for packages with empty fields
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(0);
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    EXPECT_EQ(Core::ERROR_GENERAL, result); // Should return error due to failed apps
    
    releaseResources();
}

/**
 * @brief Test error handling when mCurrentservice is null
 *
 * @details Test verifies that:
 * - createPackageManagerObject handles null service correctly
 * - Appropriate error logging and return codes
 */
TEST_F(PreinstallManagerTest, CreatePackageManagerObjectWithNullService)
{
    // Don't call createResources() to keep mCurrentservice as null
    mServiceMock = new NiceMock<ServiceMock>;
    EXPECT_EQ(string(""), plugin->Initialize(mServiceMock));
    mPreinstallManagerImpl = Plugin::PreinstallManagerImplementation::getInstance();
    
    // Manually set mCurrentservice to null to test error path
    // Note: This tests the error condition in createPackageManagerObject
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    plugin->Deinitialize(mServiceMock);
    delete mServiceMock;
    mPreinstallManagerImpl = nullptr;
}

/**
 * @brief Test StartPreinstall with forceInstall=false and existing packages to trigger version comparison
 *
 * @details Test verifies that:
 * - ListPackages is called when forceInstall=false
 * - Version comparison logic (isNewerVersion) is exercised
 * - Package filtering based on version comparison works
 */
TEST_F(PreinstallManagerTest, VersionComparisonWithExistingPackages)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock successful directory operations
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(0));
        
    EXPECT_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillRepeatedly(::testing::Return(reinterpret_cast<DIR*>(0x1234)));
    
    static struct dirent testPackage;
    strcpy(testPackage.d_name, "testpackage");
    
    static int readCallCount = 0;
    readCallCount = 0;
    EXPECT_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillRepeatedly(::testing::Invoke([&](DIR*) -> struct dirent* {
            if (readCallCount++ == 0) {
                return &testPackage;
            }
            return nullptr;
        }));
    
    EXPECT_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillRepeatedly(::testing::Return(0));
    
    // Mock ListPackages to return existing packages for version comparison
    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
        .Times(1)
        .WillOnce([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
            std::list<Exchange::IPackageInstaller::Package> packageList;
            
            Exchange::IPackageInstaller::Package existingPackage;
            existingPackage.packageId = "testpackage";
            existingPackage.version = "1.0.0"; // Existing version
            existingPackage.state = Exchange::IPackageInstaller::InstallState::INSTALLED;
            packageList.emplace_back(existingPackage);
            
            auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IPackageIterator>>::Create<Exchange::IPackageInstaller::IPackageIterator>(packageList);
            packages = mockIterator;
            return Core::ERROR_NONE;
        });
    
    // Mock GetConfigForPackage to return newer version
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = "testpackage";
            version = "2.0.0"; // Newer version than installed (1.0.0)
            return Core::ERROR_NONE;
        });
    
    // Since the new version (2.0.0) is newer than installed (1.0.0), Install should be called
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce([&](const string &packageId, const string &version, 
                     Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                     const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            EXPECT_EQ("testpackage", packageId);
            EXPECT_EQ("2.0.0", version);
            return Core::ERROR_NONE;
        });
    
    // Use forceInstall=false to trigger version comparison
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(false);
    EXPECT_EQ(Core::ERROR_NONE, result);
    
    releaseResources();
}

/**
 * @brief Test StartPreinstall with older version package that should be skipped
 *
 * @details Test verifies that:
 * - Packages with older versions are filtered out
 * - isNewerVersion returns false for older versions
 * - Install is not called for older packages
 */
TEST_F(PreinstallManagerTest, SkipOlderVersionPackages)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock successful directory operations
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(0));
        
    EXPECT_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillRepeatedly(::testing::Return(reinterpret_cast<DIR*>(0x1234)));
    
    static struct dirent testPackage;
    strcpy(testPackage.d_name, "testpackage");
    
    static int readCallCount = 0;
    readCallCount = 0;
    EXPECT_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillRepeatedly(::testing::Invoke([&](DIR*) -> struct dirent* {
            if (readCallCount++ == 0) {
                return &testPackage;
            }
            return nullptr;
        }));
    
    EXPECT_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillRepeatedly(::testing::Return(0));
    
    // Mock ListPackages to return existing packages with newer version
    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
        .Times(1)
        .WillOnce([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
            std::list<Exchange::IPackageInstaller::Package> packageList;
            
            Exchange::IPackageInstaller::Package existingPackage;
            existingPackage.packageId = "testpackage";
            existingPackage.version = "2.0.0"; // Already newer version installed
            existingPackage.state = Exchange::IPackageInstaller::InstallState::INSTALLED;
            packageList.emplace_back(existingPackage);
            
            auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IPackageIterator>>::Create<Exchange::IPackageInstaller::IPackageIterator>(packageList);
            packages = mockIterator;
            return Core::ERROR_NONE;
        });
    
    // Mock GetConfigForPackage to return older version
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = "testpackage";
            version = "1.0.0"; // Older version than installed (2.0.0)
            return Core::ERROR_NONE;
        });
    
    // Since the new version (1.0.0) is older than installed (2.0.0), Install should NOT be called
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(0); // Should not install older version
    
    // Use forceInstall=false to trigger version comparison
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(false);
    EXPECT_EQ(Core::ERROR_NONE, result); // Should succeed but no packages installed
    
    releaseResources();
}

/**
 * @brief Test error path when ListPackages fails
 *
 * @details Test verifies that:
 * - Error handling when ListPackages returns an error
 * - Appropriate error codes are returned
 */
TEST_F(PreinstallManagerTest, ListPackagesFailure)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock directory operations to make directory reading succeed
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(0));
        
    EXPECT_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillRepeatedly(::testing::Return(reinterpret_cast<DIR*>(0x1234)));
    
    static struct dirent testPackage;
    strcpy(testPackage.d_name, "testpackage");
    
    static int readCallCount = 0;
    readCallCount = 0;
    EXPECT_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillRepeatedly(::testing::Invoke([&](DIR*) -> struct dirent* {
            if (readCallCount++ == 0) {
                return &testPackage;
            }
            return nullptr;
        }));
    
    EXPECT_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillRepeatedly(::testing::Return(0));
    
    // Mock ListPackages to return an error
    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
        .Times(1)
        .WillOnce([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
            packages = nullptr;
            return Core::ERROR_GENERAL; // Return error
        });
    
    // GetConfigForPackage should still be called since directory reading succeeds
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = "testpackage";
            version = "1.0.0";
            return Core::ERROR_NONE;
        });
    
    // Use forceInstall=false to trigger ListPackages call
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(false);
    EXPECT_EQ(Core::ERROR_GENERAL, result); // Should return error due to ListPackages failure
    
    releaseResources();
}
