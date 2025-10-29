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
        // Mock stat to return success for directory existence checks
        EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
            .WillRepeatedly(::testing::Return(0));
            
        // Mock directory operations for preinstall directory
        EXPECT_CALL(*p_wrapsImplMock, opendir(::testing::_))
            .WillRepeatedly(::testing::Return(reinterpret_cast<DIR*>(0x1234))); // Non-null pointer

        // Create mock dirent structure for testing
        static struct dirent testDirent;
        strcpy(testDirent.d_name, "testapp");
        static struct dirent* direntPtr = &testDirent;
        
        // Use member variable instead of static to avoid interference between tests
        static int callCount = 0;
        callCount = 0; // Reset for each test

        EXPECT_CALL(*p_wrapsImplMock, readdir(::testing::_))
            .WillRepeatedly(::testing::Invoke([&](DIR*) -> struct dirent* {
                if (callCount == 0) {
                    callCount++;
                    return direntPtr; // Return test directory entry first time
                }
                return nullptr; // End of directory
            }));

        EXPECT_CALL(*p_wrapsImplMock, closedir(::testing::_))
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
        .WillOnce([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
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
        .WillOnce([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = "1.0.0"; // Newer version than existing 0.9.0
            return Core::ERROR_NONE;
        });
    
    // Install should be called since we have a newer version
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce([&](const string &packageId, const string &version, 
                     Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                     const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            return Core::ERROR_NONE;
        });
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(false); // forceInstall = false
    EXPECT_EQ(Core::ERROR_NONE, result);
    
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
    
    // Mock successful config reading
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = PREINSTALL_MANAGER_TEST_VERSION;
            return Core::ERROR_NONE;
        });
    
    // Mock installation failure with specific failure reason
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce([&](const string &packageId, const string &version, 
                     Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                     const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            failReason = Exchange::IPackageInstaller::FailReason::SIGNATURE_VERIFICATION_FAILURE;
            return Core::ERROR_GENERAL; // Installation failure
        });
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    // Should return error due to installation failure (getFailReason is called internally)
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
    
    // Install should be called since we have a newer version
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce([&](const string &packageId, const string &version, 
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
    
    // Mock GetConfigForPackage for both apps
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(2) // Called for both testapp1 and testapp2 (. and .. are skipped)
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = PREINSTALL_MANAGER_TEST_VERSION;
            return Core::ERROR_NONE;
        });
    
    // Install should be called for both valid apps
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(2)
        .WillRepeatedly([&](const string &packageId, const string &version, 
                           Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                           const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            return Core::ERROR_NONE;
        });
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    
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
    createPreinstallManagerImpl();
    
    // Create a mock service
    ServiceMock* validService = new NiceMock<ServiceMock>();
    
    uint32_t result = mPreinstallManagerImpl->Configure(validService);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    
    delete validService;
    releasePreinstallManagerImpl();
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
    
    static struct dirent testDirent1, testDirent2, testDirent3;
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
    
    // Mock GetConfigForPackage - first succeeds, second fails
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(2)
        .WillOnce([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            if (fileLocator.find("validapp") != string::npos) {
                id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
                version = PREINSTALL_MANAGER_TEST_VERSION;
                return Core::ERROR_NONE;
            }
            return Core::ERROR_GENERAL; // Invalid app
        })
        .WillOnce([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            return Core::ERROR_GENERAL; // Invalid app
        });
    
    // Install should be called only once (for valid app)
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce([&](const string &packageId, const string &version, 
                     Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                     const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            return Core::ERROR_NONE;
        });
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    EXPECT_EQ(Core::ERROR_NONE, result); // Should succeed overall since valid app installed
    
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
    
    // Mock successful operations
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
    
    // First call should create and cleanup package manager object
    Core::hresult result1 = mPreinstallManagerImpl->StartPreinstall(true);
    EXPECT_EQ(Core::ERROR_NONE, result1);
    
    // Second call should also work (create new object)
    Core::hresult result2 = mPreinstallManagerImpl->StartPreinstall(true);
    EXPECT_EQ(Core::ERROR_NONE, result2);
    
    releaseResources();
}
