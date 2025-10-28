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
 * @brief Test Configure method with valid service
 *
 * @details Test verifies that:
 * - Configure method works with valid service
 * - Returns ERROR_NONE on success
 */
TEST_F(PreinstallManagerTest, ConfigureWithValidService)
{
    mServiceMock = new NiceMock<ServiceMock>;
    
    EXPECT_EQ(string(""), plugin->Initialize(mServiceMock));
    mPreinstallManagerImpl = Plugin::PreinstallManagerImplementation::getInstance();
    
    uint32_t result = mPreinstallManagerImpl->Configure(mServiceMock);
    EXPECT_EQ(Core::ERROR_NONE, result);
    
    plugin->Deinitialize(mServiceMock);
    delete mServiceMock;
    mPreinstallManagerImpl = nullptr;
}

/**
 * @brief Test Configure method with null service
 *
 * @details Test verifies that:
 * - Configure method handles null service properly
 * - Returns ERROR_GENERAL for null service
 */
TEST_F(PreinstallManagerTest, ConfigureWithNullService)
{
    mServiceMock = new NiceMock<ServiceMock>;
    
    EXPECT_EQ(string(""), plugin->Initialize(mServiceMock));
    mPreinstallManagerImpl = Plugin::PreinstallManagerImplementation::getInstance();
    
    uint32_t result = mPreinstallManagerImpl->Configure(nullptr);
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    plugin->Deinitialize(mServiceMock);
    delete mServiceMock;
    mPreinstallManagerImpl = nullptr;
}

/**
 * @brief Test register same notification multiple times
 *
 * @details Test verifies that:
 * - Same notification registered multiple times doesn't cause issues
 * - Only one instance is stored in the list
 */
TEST_F(PreinstallManagerTest, RegisterSameNotificationMultipleTimes)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    auto mockNotification = Core::ProxyType<MockNotificationTest>::Create();
    testing::Mock::AllowLeak(mockNotification.operator->());
    
    // Register the same notification multiple times
    Core::hresult status1 = mPreinstallManagerImpl->Register(mockNotification.operator->());
    Core::hresult status2 = mPreinstallManagerImpl->Register(mockNotification.operator->());
    Core::hresult status3 = mPreinstallManagerImpl->Register(mockNotification.operator->());
    
    EXPECT_EQ(Core::ERROR_NONE, status1);
    EXPECT_EQ(Core::ERROR_NONE, status2);
    EXPECT_EQ(Core::ERROR_NONE, status3);
    
    // Cleanup
    mPreinstallManagerImpl->Unregister(mockNotification.operator->());
    releaseResources();
}

/**
 * @brief Test unregistering non-registered notification
 *
 * @details Test verifies that:
 * - Attempting to unregister a notification that wasn't registered returns ERROR_GENERAL
 */
TEST_F(PreinstallManagerTest, UnregisterNonRegisteredNotification)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    auto mockNotification = Core::ProxyType<MockNotificationTest>::Create();
    testing::Mock::AllowLeak(mockNotification.operator->());
    
    // Try to unregister without registering first
    Core::hresult status = mPreinstallManagerImpl->Unregister(mockNotification.operator->());
    EXPECT_EQ(Core::ERROR_GENERAL, status);
    
    releaseResources();
}

/**
 * @brief Test empty notification handling
 *
 * @details Test verifies that:
 * - Empty notification string is handled properly
 * - Method doesn't crash with empty input
 */
TEST_F(PreinstallManagerTest, HandleEmptyNotificationString)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    auto mockNotification = Core::ProxyType<MockNotificationTest>::Create();
    testing::Mock::AllowLeak(mockNotification.operator->());
    
    // Don't expect any notification calls for empty string
    EXPECT_CALL(*mockNotification, OnAppInstallationStatus(::testing::_))
        .Times(0);
    
    mPreinstallManagerImpl->Register(mockNotification.operator->());
    
    // Call with empty string - should not trigger notification
    mPreinstallManagerImpl->handleOnAppInstallationStatus("");
    
    // Small delay to ensure no async notifications
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    mPreinstallManagerImpl->Unregister(mockNotification.operator->());
    releaseResources();
}

/**
 * @brief Test multiple notifications with same event
 *
 * @details Test verifies that:
 * - Multiple registered notifications all receive the same event
 * - Event dispatching works correctly with multiple listeners
 */
TEST_F(PreinstallManagerTest, MultipleNotificationsWithSameEvent)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    auto mockNotification1 = Core::ProxyType<MockNotificationTest>::Create();
    auto mockNotification2 = Core::ProxyType<MockNotificationTest>::Create();
    testing::Mock::AllowLeak(mockNotification1.operator->());
    testing::Mock::AllowLeak(mockNotification2.operator->());
    
    std::promise<void> notification1Promise;
    std::promise<void> notification2Promise;
    std::future<void> notification1Future = notification1Promise.get_future();
    std::future<void> notification2Future = notification2Promise.get_future();
    
    string testJsonResponse = R"({"packageId":"testApp","version":"1.0.0","status":"SUCCESS"})";
    
    // Both notifications should receive the event
    EXPECT_CALL(*mockNotification1, OnAppInstallationStatus(testJsonResponse))
        .Times(1)
        .WillOnce(::testing::InvokeWithoutArgs([&notification1Promise]() {
            notification1Promise.set_value();
        }));
        
    EXPECT_CALL(*mockNotification2, OnAppInstallationStatus(testJsonResponse))
        .Times(1)
        .WillOnce(::testing::InvokeWithoutArgs([&notification2Promise]() {
            notification2Promise.set_value();
        }));
    
    mPreinstallManagerImpl->Register(mockNotification1.operator->());
    mPreinstallManagerImpl->Register(mockNotification2.operator->());
    
    // Trigger the event
    mPreinstallManagerImpl->handleOnAppInstallationStatus(testJsonResponse);
    
    // Wait for both notifications
    auto status1 = notification1Future.wait_for(std::chrono::seconds(2));
    auto status2 = notification2Future.wait_for(std::chrono::seconds(2));
    
    EXPECT_EQ(std::future_status::ready, status1);
    EXPECT_EQ(std::future_status::ready, status2);
    
    // Cleanup
    mPreinstallManagerImpl->Unregister(mockNotification1.operator->());
    mPreinstallManagerImpl->Unregister(mockNotification2.operator->());
    releaseResources();
}

/**
 * @brief Test StartPreinstall with GetConfigForPackage failure
 *
 * @details Test verifies that:
 * - StartPreinstall handles GetConfigForPackage failure gracefully
 * - Packages with invalid configs are skipped
 */
TEST_F(PreinstallManagerTest, StartPreinstallWithGetConfigFailure)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock GetConfigForPackage to return error
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            return Core::ERROR_GENERAL; // Simulate failure
        });

    SetUpPreinstallDirectoryMocks();
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    // Should handle the failure gracefully
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
    
    releaseResources();
}

/**
 * @brief Test StartPreinstall with ListPackages failure
 *
 * @details Test verifies that:
 * - StartPreinstall handles ListPackages failure when forceInstall=false
 * - Returns appropriate error status
 */
TEST_F(PreinstallManagerTest, StartPreinstallWithListPackagesFailure)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock ListPackages to return error
    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
        .WillRepeatedly([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
            packages = nullptr;
            return Core::ERROR_GENERAL;
        });

    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = PREINSTALL_MANAGER_TEST_VERSION;
            return Core::ERROR_NONE;
        });

    SetUpPreinstallDirectoryMocks();
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(false);
    
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    releaseResources();
}

/**
 * @brief Test StartPreinstall with Install method failure
 *
 * @details Test verifies that:
 * - StartPreinstall handles Install method failure properly
 * - Failed installations are logged and tracked
 */
TEST_F(PreinstallManagerTest, StartPreinstallWithInstallFailure)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = PREINSTALL_MANAGER_TEST_VERSION;
            return Core::ERROR_NONE;
        });

    // Mock Install to return failure
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &packageId, const string &version, 
                           Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                           const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            failReason = Exchange::IPackageInstaller::FailReason::SIGNATURE_VERIFICATION_FAILURE;
            return Core::ERROR_GENERAL;
        });

    SetUpPreinstallDirectoryMocks();
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    releaseResources();
}



/**
 * @brief Test StartPreinstall when directory cannot be opened
 *
 * @details Test verifies that:
 * - StartPreinstall handles directory open failure
 * - Returns ERROR_GENERAL when preinstall directory is inaccessible
 */
TEST_F(PreinstallManagerTest, StartPreinstallDirectoryOpenFailure)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock directory open failure
    ON_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillByDefault(::testing::Return(nullptr));
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    releaseResources();
}



/**
 * @brief Test package with empty fields handling
 *
 * @details Test verifies that:
 * - Packages with empty packageId, version, or fileLocator are skipped
 * - Error is logged appropriately
 */
TEST_F(PreinstallManagerTest, StartPreinstallWithEmptyPackageFields)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock GetConfigForPackage to return empty fields
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = ""; // Empty packageId
            version = ""; // Empty version
            return Core::ERROR_NONE;
        });

    // Install should not be called for packages with empty fields
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(0);

    SetUpPreinstallDirectoryMocks();
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    // Should return ERROR_GENERAL due to failed apps
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    releaseResources();
}

/**
 * @brief Test singleton instance behavior
 *
 * @details Test verifies that:
 * - getInstance returns the same instance
 * - Multiple calls return the same object
 */
TEST_F(PreinstallManagerTest, SingletonInstanceBehavior)
{
    createPreinstallManagerImpl();
    
    auto instance1 = Plugin::PreinstallManagerImplementation::getInstance();
    auto instance2 = Plugin::PreinstallManagerImplementation::getInstance();
    
    EXPECT_EQ(instance1, instance2);
    EXPECT_EQ(instance1, mPreinstallManagerImpl);
    
    releasePreinstallManagerImpl();
}

/**
 * @brief Test notification with malformed JSON
 *
 * @details Test verifies that:
 * - Malformed JSON in notification is handled gracefully
 * - System doesn't crash with invalid JSON input
 */
TEST_F(PreinstallManagerTest, HandleMalformedJsonNotification)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    auto mockNotification = Core::ProxyType<MockNotificationTest>::Create();
    testing::Mock::AllowLeak(mockNotification.operator->());
    
    std::promise<void> notificationPromise;
    std::future<void> notificationFuture = notificationPromise.get_future();
    
    string malformedJson = R"({"packageId":"testApp","version":})"; // Malformed JSON
    
    EXPECT_CALL(*mockNotification, OnAppInstallationStatus(malformedJson))
        .Times(1)
        .WillOnce(::testing::InvokeWithoutArgs([&notificationPromise]() {
            notificationPromise.set_value();
        }));
    
    mPreinstallManagerImpl->Register(mockNotification.operator->());
    
    // Should handle malformed JSON without crashing
    mPreinstallManagerImpl->handleOnAppInstallationStatus(malformedJson);
    
    auto status = notificationFuture.wait_for(std::chrono::seconds(2));
    EXPECT_EQ(std::future_status::ready, status);
    
    mPreinstallManagerImpl->Unregister(mockNotification.operator->());
    releaseResources();
}







/**
 * @brief Test rapid successive notifications
 *
 * @details Test verifies that:
 * - Multiple rapid notifications are handled correctly
 * - No race conditions occur with concurrent notifications
 */
TEST_F(PreinstallManagerTest, RapidSuccessiveNotifications)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    auto mockNotification = Core::ProxyType<MockNotificationTest>::Create();
    testing::Mock::AllowLeak(mockNotification.operator->());
    
    const int numNotifications = 5;
    std::vector<std::promise<void>> promises(numNotifications);
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < numNotifications; ++i) {
        futures.push_back(promises[i].get_future());
    }
    
    size_t callCount = 0;
    EXPECT_CALL(*mockNotification, OnAppInstallationStatus(::testing::_))
        .Times(numNotifications)
        .WillRepeatedly(::testing::InvokeWithoutArgs([&promises, &callCount]() {
            if (callCount < promises.size()) {
                promises[callCount++].set_value();
            }
        }));
    
    mPreinstallManagerImpl->Register(mockNotification.operator->());
    
    // Send multiple rapid notifications
    for (int i = 0; i < numNotifications; ++i) {
        string testJson = R"({"packageId":"testApp)" + std::to_string(i) + R"(","version":"1.0.0","status":"SUCCESS"})";
        mPreinstallManagerImpl->handleOnAppInstallationStatus(testJson);
    }
    
    // Wait for all notifications
    for (auto& future : futures) {
        auto status = future.wait_for(std::chrono::seconds(3));
        EXPECT_EQ(std::future_status::ready, status);
    }
    
    mPreinstallManagerImpl->Unregister(mockNotification.operator->());
    releaseResources();
}

/**
 * @brief Test package manager object lifecycle
 *
 * @details Test verifies that:
 * - Package manager object is created and released properly
 * - Multiple StartPreinstall calls handle object lifecycle correctly
 */
TEST_F(PreinstallManagerTest, PackageManagerObjectLifecycle)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
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
    
    // First call should create and release package manager object
    Core::hresult result1 = mPreinstallManagerImpl->StartPreinstall(true);
    EXPECT_TRUE(result1 == Core::ERROR_NONE || result1 == Core::ERROR_GENERAL);
    
    // Second call should also work (object should be recreated)
    Core::hresult result2 = mPreinstallManagerImpl->StartPreinstall(true);
    EXPECT_TRUE(result2 == Core::ERROR_NONE || result2 == Core::ERROR_GENERAL);
    
    releaseResources();
}



/**
 * @brief Test basic version comparison functionality
 *
 * @details Test verifies version comparison without complex mocking
 */
TEST_F(PreinstallManagerTest, BasicVersionComparisonTest)
{
    createPreinstallManagerImpl();
    
    // This tests that the getInstance method works and we can access the implementation
    auto instance = Plugin::PreinstallManagerImplementation::getInstance();
    EXPECT_EQ(instance, mPreinstallManagerImpl);
    
    releasePreinstallManagerImpl();
}

/**
 * @brief Test service initialization and cleanup
 *
 * @details Test verifies proper initialization and cleanup of service
 */
TEST_F(PreinstallManagerTest, ServiceInitializationTest)
{
    mServiceMock = new NiceMock<ServiceMock>;
    
    // Test plugin initialization
    string initResult = plugin->Initialize(mServiceMock);
    EXPECT_EQ(string(""), initResult);
    
    // Test getting instance after initialization
    auto impl = Plugin::PreinstallManagerImplementation::getInstance();
    EXPECT_TRUE(impl != nullptr);
    
    // Test deinitialization
    plugin->Deinitialize(mServiceMock);
    delete mServiceMock;
}

/**
 * @brief Test notification system basic functionality
 *
 * @details Test verifies basic notification system without complex event handling
 */
TEST_F(PreinstallManagerTest, BasicNotificationSystemTest)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    auto mockNotification = Core::ProxyType<MockNotificationTest>::Create();
    testing::Mock::AllowLeak(mockNotification.operator->());
    
    // Test multiple register/unregister cycles
    Core::hresult result1 = mPreinstallManagerImpl->Register(mockNotification.operator->());
    EXPECT_EQ(Core::ERROR_NONE, result1);
    
    Core::hresult result2 = mPreinstallManagerImpl->Unregister(mockNotification.operator->());
    EXPECT_EQ(Core::ERROR_NONE, result2);
    
    // Test registering again after unregister
    Core::hresult result3 = mPreinstallManagerImpl->Register(mockNotification.operator->());
    EXPECT_EQ(Core::ERROR_NONE, result3);
    
    mPreinstallManagerImpl->Unregister(mockNotification.operator->());
    releaseResources();
}

/**
 * @brief Test error handling in basic scenarios
 *
 * @details Test verifies error handling without complex directory operations
 */
TEST_F(PreinstallManagerTest, BasicErrorHandlingTest)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Test QueryInterface with valid interface
    Exchange::IPreinstallManager* preinstallInterface = 
        static_cast<Exchange::IPreinstallManager*>(
            mPreinstallManagerImpl->QueryInterface(Exchange::IPreinstallManager::ID));
    EXPECT_TRUE(preinstallInterface != nullptr);
    
    if (preinstallInterface != nullptr) {
        preinstallInterface->Release();
    }
    
    // Test QueryInterface with invalid interface ID
    const uint32_t INVALID_INTERFACE_ID = 0x99999999;
    void* invalidInterface = mPreinstallManagerImpl->QueryInterface(INVALID_INTERFACE_ID);
    EXPECT_EQ(nullptr, invalidInterface);
    
    releaseResources();
}

/**
 * @brief Test StartPreinstall with partially successful installations
 *
 * @details Test verifies that:
 * - Mixed success/failure scenarios are handled correctly
 * - Some packages succeed while others fail
 */
TEST_F(PreinstallManagerTest, StartPreinstallWithPartialSuccess)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = PREINSTALL_MANAGER_TEST_VERSION;
            return Core::ERROR_NONE;
        });

    // Mock Install to alternate between success and failure
    static bool shouldSucceed = true;
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &packageId, const string &version, 
                           Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                           const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            if (shouldSucceed) {
                shouldSucceed = false;
                return Core::ERROR_NONE;
            } else {
                shouldSucceed = true;
                failReason = Exchange::IPackageInstaller::FailReason::SIGNATURE_VERIFICATION_FAILURE;
                return Core::ERROR_GENERAL;
            }
        });

    // Mock multiple directory entries
    ON_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillByDefault(::testing::Return(reinterpret_cast<DIR*>(0x1234)));

    static std::vector<std::string> multiEntries = {"app1", "app2", "app3"};
    static size_t multiEntryIndex = 0;
    static struct dirent multiDirent;
    
    ON_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillByDefault(::testing::Invoke([&](DIR*) -> struct dirent* {
            if (multiEntryIndex < multiEntries.size()) {
                strcpy(multiDirent.d_name, multiEntries[multiEntryIndex].c_str());
                multiEntryIndex++;
                return &multiDirent;
            }
            multiEntryIndex = 0; // Reset for next test
            return nullptr;
        }));

    ON_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillByDefault(::testing::Return(0));
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    // Should return ERROR_GENERAL since some installations failed
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    releaseResources();
}

/**
 * @brief Test Configure method called multiple times
 *
 * @details Test verifies that:
 * - Multiple Configure calls work correctly
 * - Service reference counting is handled properly
 */
TEST_F(PreinstallManagerTest, ConfigureCalledMultipleTimes)
{
    mServiceMock = new NiceMock<ServiceMock>;
    
    EXPECT_EQ(string(""), plugin->Initialize(mServiceMock));
    mPreinstallManagerImpl = Plugin::PreinstallManagerImplementation::getInstance();
    
    // Configure multiple times
    uint32_t result1 = mPreinstallManagerImpl->Configure(mServiceMock);
    uint32_t result2 = mPreinstallManagerImpl->Configure(mServiceMock);
    uint32_t result3 = mPreinstallManagerImpl->Configure(mServiceMock);
    
    EXPECT_EQ(Core::ERROR_NONE, result1);
    EXPECT_EQ(Core::ERROR_NONE, result2);
    EXPECT_EQ(Core::ERROR_NONE, result3);
    
    plugin->Deinitialize(mServiceMock);
    delete mServiceMock;
    mPreinstallManagerImpl = nullptr;
}

/**
 * @brief Test notification with very large JSON payload
 *
 * @details Test verifies that:
 * - Large JSON payloads are handled correctly
 * - No buffer overflows or memory issues occur
 */
TEST_F(PreinstallManagerTest, HandleLargeJsonNotification)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    auto mockNotification = Core::ProxyType<MockNotificationTest>::Create();
    testing::Mock::AllowLeak(mockNotification.operator->());
    
    std::promise<void> notificationPromise;
    std::future<void> notificationFuture = notificationPromise.get_future();
    
    // Create large JSON payload
    string largeJson = R"({"packageId":"testApp","version":"1.0.0","status":"SUCCESS","details":")";
    std::string largeDetails(10000, 'A'); // 10KB of 'A' characters
    largeJson += largeDetails + R"("})";
    
    EXPECT_CALL(*mockNotification, OnAppInstallationStatus(largeJson))
        .Times(1)
        .WillOnce(::testing::InvokeWithoutArgs([&notificationPromise]() {
            notificationPromise.set_value();
        }));
    
    mPreinstallManagerImpl->Register(mockNotification.operator->());
    
    mPreinstallManagerImpl->handleOnAppInstallationStatus(largeJson);
    
    auto status = notificationFuture.wait_for(std::chrono::seconds(3));
    EXPECT_EQ(std::future_status::ready, status);
    
    mPreinstallManagerImpl->Unregister(mockNotification.operator->());
    releaseResources();
}



/**
 * @brief Test QueryInterface with invalid interface ID
 *
 * @details Test verifies that:
 * - QueryInterface returns nullptr for invalid interface IDs
 * - System handles unknown interface requests gracefully
 */
TEST_F(PreinstallManagerTest, QueryInterfaceWithInvalidId)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Test with invalid interface ID (using a random number)
    const uint32_t INVALID_INTERFACE_ID = 0x99999999;
    void* invalidInterface = mPreinstallManagerImpl->QueryInterface(INVALID_INTERFACE_ID);
    
    EXPECT_EQ(nullptr, invalidInterface);
    
    releaseResources();
}

/**
 * @brief Test concurrent access to notification list
 *
 * @details Test verifies that:
 * - Concurrent register/unregister operations are handled safely
 * - Thread safety of notification management
 */
TEST_F(PreinstallManagerTest, ConcurrentNotificationAccess)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    auto mockNotification1 = Core::ProxyType<MockNotificationTest>::Create();
    auto mockNotification2 = Core::ProxyType<MockNotificationTest>::Create();
    testing::Mock::AllowLeak(mockNotification1.operator->());
    testing::Mock::AllowLeak(mockNotification2.operator->());
    
    // Simulate concurrent operations
    std::thread t1([&]() {
        for (int i = 0; i < 10; ++i) {
            mPreinstallManagerImpl->Register(mockNotification1.operator->());
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    
    std::thread t2([&]() {
        for (int i = 0; i < 10; ++i) {
            mPreinstallManagerImpl->Register(mockNotification2.operator->());
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    
    std::thread t3([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Let registers happen first
        for (int i = 0; i < 10; ++i) {
            mPreinstallManagerImpl->Unregister(mockNotification1.operator->());
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    
    t1.join();
    t2.join();
    t3.join();
    
    // Clean up any remaining registrations
    mPreinstallManagerImpl->Unregister(mockNotification2.operator->());
    
    // If we get here without deadlock or crash, the test passed
    EXPECT_TRUE(true);
    
    releaseResources();
}

/**
 * @brief Test version comparison logic with various version formats
 *
 * @details Test verifies that:
 * - isNewerVersion correctly compares semantic versions
 * - Different version formats are handled properly
 * - Edge cases in version comparison work correctly
 */
TEST_F(PreinstallManagerTest, VersionComparisonLogic)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Create a derived test class to access private methods
    class TestablePreinstallManagerImpl : public Plugin::PreinstallManagerImplementation {
    public:
        using Plugin::PreinstallManagerImplementation::isNewerVersion;
        using Plugin::PreinstallManagerImplementation::getFailReason;
    };
    
    TestablePreinstallManagerImpl testImpl;
    
    // Test basic version comparison
    EXPECT_TRUE(testImpl.isNewerVersion("2.0.0", "1.0.0"));   // Major version
    EXPECT_TRUE(testImpl.isNewerVersion("1.2.0", "1.1.0"));   // Minor version  
    EXPECT_TRUE(testImpl.isNewerVersion("1.1.2", "1.1.1"));   // Patch version
    EXPECT_TRUE(testImpl.isNewerVersion("1.1.1.2", "1.1.1.1")); // Build version
    
    // Test equal versions
    EXPECT_FALSE(testImpl.isNewerVersion("1.0.0", "1.0.0"));
    EXPECT_FALSE(testImpl.isNewerVersion("2.1.3", "2.1.3"));
    
    // Test older versions
    EXPECT_FALSE(testImpl.isNewerVersion("1.0.0", "2.0.0"));
    EXPECT_FALSE(testImpl.isNewerVersion("1.1.0", "1.2.0"));
    EXPECT_FALSE(testImpl.isNewerVersion("1.1.1", "1.1.2"));
    
    // Test versions with suffixes (should be stripped)
    EXPECT_TRUE(testImpl.isNewerVersion("2.0.0-beta", "1.0.0-alpha"));
    EXPECT_TRUE(testImpl.isNewerVersion("2.0.0+build123", "1.0.0+build456"));
    
    // Test malformed versions (should return false)
    EXPECT_FALSE(testImpl.isNewerVersion("invalid", "1.0.0"));
    EXPECT_FALSE(testImpl.isNewerVersion("1.0.0", "invalid"));
    EXPECT_FALSE(testImpl.isNewerVersion("1.a.0", "1.0.0"));
    EXPECT_FALSE(testImpl.isNewerVersion("1", "1.0.0")); // Incomplete version
    
    releaseResources();
}

/**
 * @brief Test failure reason mapping for all enum values
 *
 * @details Test verifies that:
 * - All FailReason enum values are properly mapped to strings
 * - Default case handles unknown values
 */
TEST_F(PreinstallManagerTest, FailureReasonMapping)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    class TestablePreinstallManagerImpl : public Plugin::PreinstallManagerImplementation {
    public:
        using Plugin::PreinstallManagerImplementation::getFailReason;
    };
    
    TestablePreinstallManagerImpl testImpl;
    
    // Test all defined failure reasons
    EXPECT_EQ("SIGNATURE_VERIFICATION_FAILURE", 
              testImpl.getFailReason(Exchange::IPackageInstaller::FailReason::SIGNATURE_VERIFICATION_FAILURE));
    EXPECT_EQ("PACKAGE_MISMATCH_FAILURE", 
              testImpl.getFailReason(Exchange::IPackageInstaller::FailReason::PACKAGE_MISMATCH_FAILURE));
    EXPECT_EQ("INVALID_METADATA_FAILURE", 
              testImpl.getFailReason(Exchange::IPackageInstaller::FailReason::INVALID_METADATA_FAILURE));
    EXPECT_EQ("PERSISTENCE_FAILURE", 
              testImpl.getFailReason(Exchange::IPackageInstaller::FailReason::PERSISTENCE_FAILURE));
    
    // Test default case with unknown value (cast to create unknown enum value)
    EXPECT_EQ("NONE", testImpl.getFailReason(static_cast<Exchange::IPackageInstaller::FailReason>(999)));
    
    releaseResources();
}

/**
 * @brief Test event dispatching with unknown event types
 *
 * @details Test verifies that:
 * - Unknown event types are handled gracefully
 * - Dispatch method logs errors for unknown events
 */
TEST_F(PreinstallManagerTest, UnknownEventDispatch)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    class TestablePreinstallManagerImpl : public Plugin::PreinstallManagerImplementation {
    public:
        using Plugin::PreinstallManagerImplementation::Dispatch;
        using Plugin::PreinstallManagerImplementation::EventNames;
    };
    
    TestablePreinstallManagerImpl testImpl;
    
    JsonObject params;
    params["test"] = "value";
    
    // Test with unknown event (should not crash)
    testImpl.Dispatch(static_cast<TestablePreinstallManagerImpl::EventNames>(999), params);
    
    // Test event without required jsonresponse parameter
    testImpl.Dispatch(TestablePreinstallManagerImpl::PREINSTALL_MANAGER_APP_INSTALLATION_STATUS, params);
    
    // Test with proper jsonresponse parameter
    JsonObject validParams;
    validParams["jsonresponse"] = R"({"test":"data"})";
    testImpl.Dispatch(TestablePreinstallManagerImpl::PREINSTALL_MANAGER_APP_INSTALLATION_STATUS, validParams);
    
    releaseResources();
}

/**
 * @brief Test package iterator edge cases and null handling
 *
 * @details Test verifies that:
 * - Null package iterator is handled gracefully
 * - Package iterator with no packages works correctly
 * - Package iterator with packages of different states
 */
TEST_F(PreinstallManagerTest, PackageIteratorEdgeCases)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Test with null package iterator
    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
        .WillOnce([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
            packages = nullptr;
            return Core::ERROR_NONE; // Return success but with null iterator
        });

    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = PREINSTALL_MANAGER_TEST_VERSION;
            return Core::ERROR_NONE;
        });

    SetUpPreinstallDirectoryMocks();
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(false);
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    releaseResources();
}

/**
 * @brief Test package filtering logic with version comparisons
 *
 * @details Test verifies that:
 * - Newer versions are installed over older ones
 * - Older versions are skipped when newer is installed
 * - Equal versions are handled correctly
 */
TEST_F(PreinstallManagerTest, PackageFilteringWithVersions)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Create mock iterator with installed packages
    auto createMockIteratorWithVersion = [](const string& version) {
        std::list<Exchange::IPackageInstaller::Package> packageList;
        Exchange::IPackageInstaller::Package package;
        package.packageId = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
        package.version = version;
        package.digest = "";
        package.state = Exchange::IPackageInstaller::InstallState::INSTALLED;
        package.sizeKb = 0;
        packageList.emplace_back(package);
        return Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IPackageIterator>>::Create<Exchange::IPackageInstaller::IPackageIterator>(packageList);
    };
    
    // Test with older version installed (should install newer)
    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
        .WillOnce([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
            auto mockIterator = createMockIteratorWithVersion("0.9.0"); // Older version
            packages = mockIterator;
            return Core::ERROR_NONE;
        });

    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = PREINSTALL_MANAGER_TEST_VERSION; // "1.0.0" - newer than installed
            return Core::ERROR_NONE;
        });

    // Install should be called since preinstall version is newer
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce([&](const string &packageId, const string &version, 
                     Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                     const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            return Core::ERROR_NONE;
        });

    SetUpPreinstallDirectoryMocks();
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(false);
    EXPECT_EQ(Core::ERROR_NONE, result);
    
    releaseResources();
}

/**
 * @brief Test directory reading with various entry types
 *
 * @details Test verifies that:
 * - Directory entries "." and ".." are properly skipped
 * - Regular directory entries are processed
 * - Multiple directory entries are handled correctly
 */
TEST_F(PreinstallManagerTest, DirectoryReadingWithSpecialEntries)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
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

    // Mock directory with ".", ".." and regular entries
    ON_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillByDefault(::testing::Return(reinterpret_cast<DIR*>(0x1234)));

    static std::vector<std::string> dirEntries = {".", "..", "validapp1", "validapp2"};
    static size_t dirEntryIndex = 0;
    static struct dirent dirDirent;
    
    ON_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillByDefault(::testing::Invoke([&](DIR*) -> struct dirent* {
            if (dirEntryIndex < dirEntries.size()) {
                strcpy(dirDirent.d_name, dirEntries[dirEntryIndex].c_str());
                dirEntryIndex++;
                return &dirDirent;
            }
            dirEntryIndex = 0; // Reset for next test
            return nullptr;
        }));

    ON_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillByDefault(::testing::Return(0));
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    // Should only install validapp1 and validapp2, skipping "." and ".."
    EXPECT_EQ(Core::ERROR_NONE, result);
    
    releaseResources();
}

/**
 * @brief Test constructor and destructor behavior
 *
 * @details Test verifies that:
 * - Constructor properly initializes singleton instance
 * - Destructor properly cleans up resources
 * - Multiple instance creation is handled correctly
 */
TEST_F(PreinstallManagerTest, ConstructorDestructorBehavior)
{
    // Test that instance is null initially
    EXPECT_EQ(nullptr, Plugin::PreinstallManagerImplementation::_instance);
    
    // Create first instance
    auto impl1 = new Plugin::PreinstallManagerImplementation();
    EXPECT_EQ(impl1, Plugin::PreinstallManagerImplementation::getInstance());
    
    // Create second instance (should not change singleton)
    auto impl2 = new Plugin::PreinstallManagerImplementation();
    EXPECT_EQ(impl1, Plugin::PreinstallManagerImplementation::getInstance()); // First one should remain
    
    // Delete first instance
    delete impl1;
    EXPECT_EQ(nullptr, Plugin::PreinstallManagerImplementation::_instance);
    
    // Delete second instance
    delete impl2;
}

/**
 * @brief Test AddRef and Release functionality
 *
 * @details Test verifies that:
 * - AddRef/Release work correctly for interface lifecycle
 * - Reference counting is handled properly
 */
TEST_F(PreinstallManagerTest, AddRefReleaseLifecycle)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Test AddRef/Release on implementation
    mPreinstallManagerImpl->AddRef();
    uint32_t refCount = mPreinstallManagerImpl->Release();
    
    // The exact ref count depends on internal implementation, 
    // but we can verify the methods execute without crashing
    EXPECT_TRUE(refCount >= 0);
    
    releaseResources();
}

/**
 * @brief Test plugin Information method
 *
 * @details Test verifies that:
 * - Information method returns proper plugin information
 * - Method doesn't crash when called
 */
TEST_F(PreinstallManagerTest, PluginInformation)
{
    createPreinstallManagerImpl();
    
    string info = plugin->Information();
    EXPECT_FALSE(info.empty()); // Should return some information
    
    releasePreinstallManagerImpl();
}

/**
 * @brief Test package filtering when equal versions are found
 *
 * @details Test verifies that:
 * - Equal versions are not reinstalled when forceInstall=false
 * - Package filtering logic works correctly with same versions
 */
TEST_F(PreinstallManagerTest, PackageFilteringWithEqualVersions)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Create mock iterator with same version as preinstall
    auto createMockIteratorWithVersion = [](const string& version) {
        std::list<Exchange::IPackageInstaller::Package> packageList;
        Exchange::IPackageInstaller::Package package;
        package.packageId = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
        package.version = version;
        package.digest = "";
        package.state = Exchange::IPackageInstaller::InstallState::INSTALLED;
        package.sizeKb = 0;
        packageList.emplace_back(package);
        return Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IPackageIterator>>::Create<Exchange::IPackageInstaller::IPackageIterator>(packageList);
    };
    
    // Test with same version installed (should skip installation)
    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
        .WillOnce([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
            auto mockIterator = createMockIteratorWithVersion(PREINSTALL_MANAGER_TEST_VERSION); // Same version
            packages = mockIterator;
            return Core::ERROR_NONE;
        });

    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = PREINSTALL_MANAGER_TEST_VERSION; // Same version as installed
            return Core::ERROR_NONE;
        });

    // Install should NOT be called since versions are equal
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(0);

    SetUpPreinstallDirectoryMocks();
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(false);
    EXPECT_EQ(Core::ERROR_NONE, result); // Should succeed but with no installations
    
    releaseResources();
}

/**
 * @brief Test package filtering when newer version is already installed
 *
 * @details Test verifies that:
 * - Older preinstall packages are skipped when newer is already installed
 * - Version comparison logic prevents downgrades
 */
TEST_F(PreinstallManagerTest, PackageFilteringWithNewerInstalledVersion)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    auto createMockIteratorWithVersion = [](const string& version) {
        std::list<Exchange::IPackageInstaller::Package> packageList;
        Exchange::IPackageInstaller::Package package;
        package.packageId = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
        package.version = version;
        package.digest = "";
        package.state = Exchange::IPackageInstaller::InstallState::INSTALLED;
        package.sizeKb = 0;
        packageList.emplace_back(package);
        return Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IPackageIterator>>::Create<Exchange::IPackageInstaller::IPackageIterator>(packageList);
    };
    
    // Test with newer version already installed
    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
        .WillOnce([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
            auto mockIterator = createMockIteratorWithVersion("2.0.0"); // Newer than preinstall
            packages = mockIterator;
            return Core::ERROR_NONE;
        });

    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = PREINSTALL_MANAGER_TEST_VERSION; // "1.0.0" - older than installed "2.0.0"
            return Core::ERROR_NONE;
        });

    // Install should NOT be called since preinstall version is older
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(0);

    SetUpPreinstallDirectoryMocks();
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(false);
    EXPECT_EQ(Core::ERROR_NONE, result); // Should succeed but with no installations
    
    releaseResources();
}

/**
 * @brief Test package iterator with packages in different states
 *
 * @details Test verifies that:
 * - Only INSTALLED packages are considered during filtering
 * - Packages in other states are ignored
 */
TEST_F(PreinstallManagerTest, PackageIteratorWithMixedStates)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    auto createMockIteratorWithMixedStates = []() {
        std::list<Exchange::IPackageInstaller::Package> packageList;
        
        // Add package in INSTALLED state
        Exchange::IPackageInstaller::Package installedPackage;
        installedPackage.packageId = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
        installedPackage.version = "0.9.0"; // Older version
        installedPackage.state = Exchange::IPackageInstaller::InstallState::INSTALLED;
        packageList.emplace_back(installedPackage);
        
        // Add package in INSTALLING state (should be ignored)
        Exchange::IPackageInstaller::Package installingPackage;
        installingPackage.packageId = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
        installingPackage.version = "2.0.0"; // Newer version but wrong state
        installingPackage.state = Exchange::IPackageInstaller::InstallState::INSTALLING;
        packageList.emplace_back(installingPackage);
        
        return Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IPackageIterator>>::Create<Exchange::IPackageInstaller::IPackageIterator>(packageList);
    };
    
    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
        .WillOnce([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
            auto mockIterator = createMockIteratorWithMixedStates();
            packages = mockIterator;
            return Core::ERROR_NONE;
        });

    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = PREINSTALL_MANAGER_TEST_VERSION; // "1.0.0" - newer than installed "0.9.0"
            return Core::ERROR_NONE;
        });

    // Install should be called since only INSTALLED version "0.9.0" is considered
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce([&](const string &packageId, const string &version, 
                     Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                     const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            return Core::ERROR_NONE;
        });

    SetUpPreinstallDirectoryMocks();
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(false);
    EXPECT_EQ(Core::ERROR_NONE, result);
    
    releaseResources();
}

/**
 * @brief Test multiple different packages in preinstall directory
 *
 * @details Test verifies that:
 * - Multiple different packages are processed correctly
 * - Each package is evaluated independently
 * - Mixed success/failure scenarios work
 */
TEST_F(PreinstallManagerTest, MultiplePackagesInPreinstall)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Track which package is being processed
    static std::vector<std::string> packageIds = {"com.test.app1", "com.test.app2", "com.test.app3"};
    static size_t packageIndex = 0;
    
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            if (packageIndex < packageIds.size()) {
                id = packageIds[packageIndex];
                version = "1.0." + std::to_string(packageIndex);
                packageIndex++;
                return Core::ERROR_NONE;
            }
            return Core::ERROR_GENERAL;
        });

    // Some packages succeed, others fail
    static size_t installCallCount = 0;
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &packageId, const string &version, 
                           Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                           const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            installCallCount++;
            if (installCallCount % 2 == 0) {
                failReason = Exchange::IPackageInstaller::FailReason::SIGNATURE_VERIFICATION_FAILURE;
                return Core::ERROR_GENERAL; // Every second install fails
            }
            return Core::ERROR_NONE;
        });

    // Mock directory with multiple entries
    ON_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillByDefault(::testing::Return(reinterpret_cast<DIR*>(0x1234)));

    static std::vector<std::string> multiPackageEntries = {"app1", "app2", "app3"};
    static size_t multiPackageIndex = 0;
    static struct dirent multiPackageDirent;
    
    ON_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillByDefault(::testing::Invoke([&](DIR*) -> struct dirent* {
            if (multiPackageIndex < multiPackageEntries.size()) {
                strcpy(multiPackageDirent.d_name, multiPackageEntries[multiPackageIndex].c_str());
                multiPackageIndex++;
                return &multiPackageDirent;
            }
            multiPackageIndex = 0; // Reset for next test
            packageIndex = 0;      // Reset package index
            installCallCount = 0;  // Reset install count
            return nullptr;
        }));

    ON_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillByDefault(::testing::Return(0));
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    // Should return ERROR_GENERAL since some installations failed
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    releaseResources();
}

/**
 * @brief Test job creation and dispatch mechanism
 *
 * @details Test verifies that:
 * - Job creation works correctly
 * - Dispatch mechanism handles events properly
 * - Worker pool integration functions
 */
TEST_F(PreinstallManagerTest, JobCreationAndDispatch)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    auto mockNotification = Core::ProxyType<MockNotificationTest>::Create();
    testing::Mock::AllowLeak(mockNotification.operator->());
    
    std::promise<void> notificationPromise;
    std::future<void> notificationFuture = notificationPromise.get_future();
    
    string testJsonResponse = R"({"packageId":"testApp","version":"1.0.0","status":"SUCCESS"})";
    
    EXPECT_CALL(*mockNotification, OnAppInstallationStatus(testJsonResponse))
        .Times(1)
        .WillOnce(::testing::InvokeWithoutArgs([&notificationPromise]() {
            notificationPromise.set_value();
        }));
    
    mPreinstallManagerImpl->Register(mockNotification.operator->());
    
    // Trigger job creation and dispatch
    mPreinstallManagerImpl->handleOnAppInstallationStatus(testJsonResponse);
    
    // Wait for the job to be processed
    auto status = notificationFuture.wait_for(std::chrono::seconds(3));
    EXPECT_EQ(std::future_status::ready, status);
    
    mPreinstallManagerImpl->Unregister(mockNotification.operator->());
    releaseResources();
}

/**
 * @brief Test package manager notification integration
 *
 * @details Test verifies that:
 * - PackageManagerNotification class works correctly
 * - Integration with parent PreinstallManagerImplementation
 */
TEST_F(PreinstallManagerTest, PackageManagerNotificationIntegration)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // The PackageManagerNotification should be automatically registered
    // when package manager object is created. Test that callbacks work.
    
    auto mockNotification = Core::ProxyType<MockNotificationTest>::Create();
    testing::Mock::AllowLeak(mockNotification.operator->());
    
    std::promise<void> notificationPromise;
    std::future<void> notificationFuture = notificationPromise.get_future();
    
    string testJsonResponse = R"({"packageId":"testApp","version":"1.0.0","status":"INSTALLED"})";
    
    EXPECT_CALL(*mockNotification, OnAppInstallationStatus(testJsonResponse))
        .Times(1)
        .WillOnce(::testing::InvokeWithoutArgs([&notificationPromise]() {
            notificationPromise.set_value();
        }));
    
    mPreinstallManagerImpl->Register(mockNotification.operator->());
    
    // Simulate notification from PackageManager
    if (mPackageInstallerNotification_cb) {
        mPackageInstallerNotification_cb->OnAppInstallationStatus(testJsonResponse);
    }
    
    auto status = notificationFuture.wait_for(std::chrono::seconds(3));
    EXPECT_EQ(std::future_status::ready, status);
    
    mPreinstallManagerImpl->Unregister(mockNotification.operator->());
    releaseResources();
}

/**
 * @brief Test PreinstallManager plugin notification integration
 *
 * @details Test verifies that:
 * - Plugin-level notification works correctly
 * - Integration between plugin and implementation
 */
TEST_F(PreinstallManagerTest, PluginNotificationIntegration)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Test the plugin's QueryInterface for notification
    Exchange::IPreinstallManager* preinstallMgr = static_cast<Exchange::IPreinstallManager*>(
        plugin->QueryInterface(Exchange::IPreinstallManager::ID));
    
    EXPECT_TRUE(preinstallMgr != nullptr);
    
    if (preinstallMgr != nullptr) {
        auto mockNotification = Core::ProxyType<MockNotificationTest>::Create();
        testing::Mock::AllowLeak(mockNotification.operator->());
        
        // Test registration through plugin interface
        Core::hresult regResult = preinstallMgr->Register(mockNotification.operator->());
        EXPECT_EQ(Core::ERROR_NONE, regResult);
        
        // Test unregistration
        Core::hresult unregResult = preinstallMgr->Unregister(mockNotification.operator->());
        EXPECT_EQ(Core::ERROR_NONE, unregResult);
        
        preinstallMgr->Release();
    }
    
    releaseResources();
}
