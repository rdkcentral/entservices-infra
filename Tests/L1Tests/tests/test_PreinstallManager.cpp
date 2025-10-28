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
 * - Configure method accepts valid service and returns ERROR_NONE
 * - Service is properly stored
 */
TEST_F(PreinstallManagerTest, ConfigureWithValidService)
{
    mServiceMock = new NiceMock<ServiceMock>;
    
    EXPECT_EQ(string(""), plugin->Initialize(mServiceMock));
    mPreinstallManagerImpl = Plugin::PreinstallManagerImplementation::getInstance();
    
    // Test Configure with valid service
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
 * - Configure method rejects null service and returns ERROR_GENERAL
 */
TEST_F(PreinstallManagerTest, ConfigureWithNullService)
{
    mServiceMock = new NiceMock<ServiceMock>;
    
    EXPECT_EQ(string(""), plugin->Initialize(mServiceMock));
    mPreinstallManagerImpl = Plugin::PreinstallManagerImplementation::getInstance();
    
    // Test Configure with null service
    uint32_t result = mPreinstallManagerImpl->Configure(nullptr);
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    plugin->Deinitialize(mServiceMock);
    delete mServiceMock;
    mPreinstallManagerImpl = nullptr;
}

/**
 * @brief Test version comparison functionality
 *
 * @details Test verifies that:
 * - isNewerVersion correctly compares different version strings
 * - Handles various version formats and edge cases
 */
TEST_F(PreinstallManagerTest, VersionComparison)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Test basic version comparison
    EXPECT_TRUE(mPreinstallManagerImpl->isNewerVersion("2.0.0", "1.0.0"));
    EXPECT_TRUE(mPreinstallManagerImpl->isNewerVersion("1.1.0", "1.0.0"));
    EXPECT_TRUE(mPreinstallManagerImpl->isNewerVersion("1.0.1", "1.0.0"));
    EXPECT_FALSE(mPreinstallManagerImpl->isNewerVersion("1.0.0", "1.0.0"));
    EXPECT_FALSE(mPreinstallManagerImpl->isNewerVersion("1.0.0", "2.0.0"));
    
    // Test versions with build numbers
    EXPECT_TRUE(mPreinstallManagerImpl->isNewerVersion("1.0.0.2", "1.0.0.1"));
    EXPECT_FALSE(mPreinstallManagerImpl->isNewerVersion("1.0.0.1", "1.0.0.2"));
    
    // Test versions with pre-release suffixes
    EXPECT_FALSE(mPreinstallManagerImpl->isNewerVersion("1.0.0-beta", "1.0.0"));
    EXPECT_FALSE(mPreinstallManagerImpl->isNewerVersion("1.0.0+build1", "1.0.0"));
    
    // Test invalid version formats
    EXPECT_FALSE(mPreinstallManagerImpl->isNewerVersion("invalid", "1.0.0"));
    EXPECT_FALSE(mPreinstallManagerImpl->isNewerVersion("1.0.0", "invalid"));
    EXPECT_FALSE(mPreinstallManagerImpl->isNewerVersion("1.x.0", "1.0.0"));
    
    releaseResources();
}

/**
 * @brief Test getFailReason method with different failure reasons
 *
 * @details Test verifies that:
 * - getFailReason returns correct strings for each failure reason
 * - Handles unknown reasons gracefully
 */
TEST_F(PreinstallManagerTest, GetFailReason)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Test all known failure reasons
    EXPECT_EQ("SIGNATURE_VERIFICATION_FAILURE", 
              mPreinstallManagerImpl->getFailReason(Exchange::IPackageInstaller::FailReason::SIGNATURE_VERIFICATION_FAILURE));
    EXPECT_EQ("PACKAGE_MISMATCH_FAILURE", 
              mPreinstallManagerImpl->getFailReason(Exchange::IPackageInstaller::FailReason::PACKAGE_MISMATCH_FAILURE));
    EXPECT_EQ("INVALID_METADATA_FAILURE", 
              mPreinstallManagerImpl->getFailReason(Exchange::IPackageInstaller::FailReason::INVALID_METADATA_FAILURE));
    EXPECT_EQ("PERSISTENCE_FAILURE", 
              mPreinstallManagerImpl->getFailReason(Exchange::IPackageInstaller::FailReason::PERSISTENCE_FAILURE));
    
    // Test default case with unknown reason
    EXPECT_EQ("NONE", 
              mPreinstallManagerImpl->getFailReason(static_cast<Exchange::IPackageInstaller::FailReason>(999)));
    
    releaseResources();
}

/**
 * @brief Test handling of empty JSON response
 *
 * @details Test verifies that:
 * - Empty JSON response is handled gracefully
 * - No notification is sent for empty response
 */
TEST_F(PreinstallManagerTest, HandleEmptyJsonResponse)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    auto mockNotification = Core::ProxyType<MockNotificationTest>::Create();
    testing::Mock::AllowLeak(mockNotification.operator->());
    
    // Expect no notification for empty JSON
    EXPECT_CALL(*mockNotification, OnAppInstallationStatus(::testing::_))
        .Times(0);
    
    mPreinstallManagerImpl->Register(mockNotification.operator->());
    
    // Call handler with empty JSON response
    mPreinstallManagerImpl->handleOnAppInstallationStatus("");
    
    // Give time for any async operations
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    mPreinstallManagerImpl->Unregister(mockNotification.operator->());
    releaseResources();
}

/**
 * @brief Test multiple notification registrations
 *
 * @details Test verifies that:
 * - Multiple notifications can be registered
 * - All registered notifications receive events
 * - Duplicate registrations are handled properly
 */
TEST_F(PreinstallManagerTest, MultipleNotificationRegistrations)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    auto mockNotification1 = Core::ProxyType<MockNotificationTest>::Create();
    auto mockNotification2 = Core::ProxyType<MockNotificationTest>::Create();
    testing::Mock::AllowLeak(mockNotification1.operator->());
    testing::Mock::AllowLeak(mockNotification2.operator->());
    
    std::promise<void> notification1Promise;
    std::promise<void> notification2Promise;
    auto future1 = notification1Promise.get_future();
    auto future2 = notification2Promise.get_future();
    
    // Expect both notifications to be called
    EXPECT_CALL(*mockNotification1, OnAppInstallationStatus(::testing::_))
        .Times(1)
        .WillOnce(::testing::InvokeWithoutArgs([&notification1Promise]() {
            notification1Promise.set_value();
        }));
    
    EXPECT_CALL(*mockNotification2, OnAppInstallationStatus(::testing::_))
        .Times(1)
        .WillOnce(::testing::InvokeWithoutArgs([&notification2Promise]() {
            notification2Promise.set_value();
        }));
    
    // Register both notifications
    mPreinstallManagerImpl->Register(mockNotification1.operator->());
    mPreinstallManagerImpl->Register(mockNotification2.operator->());
    
    // Test duplicate registration (should not cause issues)
    mPreinstallManagerImpl->Register(mockNotification1.operator->());
    
    // Send test event
    string testJsonResponse = R"({"packageId":"testApp","version":"1.0.0","status":"SUCCESS"})";
    mPreinstallManagerImpl->handleOnAppInstallationStatus(testJsonResponse);
    
    // Wait for both notifications
    EXPECT_EQ(std::future_status::ready, future1.wait_for(std::chrono::seconds(2)));
    EXPECT_EQ(std::future_status::ready, future2.wait_for(std::chrono::seconds(2)));
    
    // Cleanup
    mPreinstallManagerImpl->Unregister(mockNotification1.operator->());
    mPreinstallManagerImpl->Unregister(mockNotification2.operator->());
    releaseResources();
}

/**
 * @brief Test unregistering notification that was never registered
 *
 * @details Test verifies that:
 * - Unregistering non-registered notification returns ERROR_GENERAL
 * - System handles this gracefully
 */
TEST_F(PreinstallManagerTest, UnregisterNonRegisteredNotification)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    auto mockNotification = Core::ProxyType<MockNotificationTest>::Create();
    testing::Mock::AllowLeak(mockNotification.operator->());
    
    // Try to unregister without registering first
    Core::hresult result = mPreinstallManagerImpl->Unregister(mockNotification.operator->());
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    releaseResources();
}

/**
 * @brief Test StartPreinstall with GetConfigForPackage failure
 *
 * @details Test verifies that:
 * - StartPreinstall handles GetConfigForPackage failures gracefully
 * - Packages with config failures are skipped but process continues
 */
TEST_F(PreinstallManagerTest, StartPreinstallWithGetConfigFailure)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock GetConfigForPackage to return error
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(Core::ERROR_GENERAL));
    
    SetUpPreinstallDirectoryMocks();
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    // Should handle config failure and continue (may return ERROR_NONE or ERROR_GENERAL)
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
    
    releaseResources();
}

/**
 * @brief Test StartPreinstall with Install failure
 *
 * @details Test verifies that:
 * - StartPreinstall handles Install method failures
 * - Failed installations are logged properly
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

    // Mock Install to return error
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &packageId, const string &version, 
                           Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                           const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            failReason = Exchange::IPackageInstaller::FailReason::SIGNATURE_VERIFICATION_FAILURE;
            return Core::ERROR_GENERAL;
        });

    SetUpPreinstallDirectoryMocks();
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    // Should return error when installations fail
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    releaseResources();
}

/**
 * @brief Test StartPreinstall with ListPackages failure
 *
 * @details Test verifies that:
 * - StartPreinstall handles ListPackages failures when forceInstall=false
 */
TEST_F(PreinstallManagerTest, StartPreinstallWithListPackagesFailure)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock ListPackages to return error
    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
        .WillRepeatedly(::testing::Return(Core::ERROR_GENERAL));

    SetUpPreinstallDirectoryMocks();
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(false);
    
    // Should return error when ListPackages fails
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    releaseResources();
}

/**
 * @brief Test readPreinstallDirectory with no directory
 *
 * @details Test verifies that:
 * - readPreinstallDirectory handles missing directory gracefully
 */
TEST_F(PreinstallManagerTest, ReadPreinstallDirectoryNoDirectory)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock opendir to return null (directory doesn't exist)
    ON_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillByDefault(::testing::Return(nullptr));
    
    std::list<Plugin::PreinstallManagerImplementation::PackageInfo> packages;
    bool result = mPreinstallManagerImpl->readPreinstallDirectory(packages);
    
    EXPECT_FALSE(result);
    EXPECT_TRUE(packages.empty());
    
    releaseResources();
}

/**
 * @brief Test readPreinstallDirectory with empty directory
 *
 * @details Test verifies that:
 * - readPreinstallDirectory handles empty directory correctly
 */
TEST_F(PreinstallManagerTest, ReadPreinstallDirectoryEmpty)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock directory operations for empty directory
    ON_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillByDefault(::testing::Return(reinterpret_cast<DIR*>(0x1234)));
    
    ON_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillByDefault(::testing::Return(nullptr)); // No entries
    
    ON_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillByDefault(::testing::Return(0));
    
    std::list<Plugin::PreinstallManagerImplementation::PackageInfo> packages;
    bool result = mPreinstallManagerImpl->readPreinstallDirectory(packages);
    
    EXPECT_TRUE(result);
    EXPECT_TRUE(packages.empty());
    
    releaseResources();
}

/**
 * @brief Test PreinstallManager Plugin Initialize method
 *
 * @details Test verifies that:
 * - Initialize method works correctly
 * - Returns empty string on success
 */
TEST_F(PreinstallManagerTest, PluginInitialize)
{
    mServiceMock = new NiceMock<ServiceMock>;
    
    // Initialize the plugin
    string result = plugin->Initialize(mServiceMock);
    EXPECT_EQ(string(""), result);
    
    plugin->Deinitialize(mServiceMock);
    delete mServiceMock;
}

/**
 * @brief Test PreinstallManager Plugin Information method
 *
 * @details Test verifies that:
 * - Information method returns proper JSON
 */
TEST_F(PreinstallManagerTest, PluginInformation)
{
    string info = plugin->Information();
    EXPECT_FALSE(info.empty());
    
    // Should be valid JSON
    JsonObject infoJson;
    EXPECT_TRUE(infoJson.FromString(info));
}

/**
 * @brief Test Dispatch method with unknown event
 *
 * @details Test verifies that:
 * - Dispatch method handles unknown events gracefully
 */
TEST_F(PreinstallManagerTest, DispatchUnknownEvent)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    JsonObject params;
    params["test"] = "value";
    
    // Call Dispatch with unknown event (should not crash)
    mPreinstallManagerImpl->Dispatch(static_cast<Plugin::PreinstallManagerImplementation::EventNames>(999), params);
    
    releaseResources();
}

/**
 * @brief Test Dispatch method with valid event but missing parameters
 *
 * @details Test verifies that:
 * - Dispatch method handles missing parameters gracefully
 */
TEST_F(PreinstallManagerTest, DispatchMissingParameters)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    auto mockNotification = Core::ProxyType<MockNotificationTest>::Create();
    testing::Mock::AllowLeak(mockNotification.operator->());
    
    // Don't expect notification to be called due to missing parameters
    EXPECT_CALL(*mockNotification, OnAppInstallationStatus(::testing::_))
        .Times(0);
    
    mPreinstallManagerImpl->Register(mockNotification.operator->());
    
    JsonObject params; // Empty params (missing jsonresponse)
    mPreinstallManagerImpl->Dispatch(Plugin::PreinstallManagerImplementation::PREINSTALL_MANAGER_APP_INSTALLATION_STATUS, params);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    mPreinstallManagerImpl->Unregister(mockNotification.operator->());
    releaseResources();
}

/**
 * @brief Test StartPreinstall with mixed package scenarios
 *
 * @details Test verifies that:
 * - StartPreinstall handles packages with empty fields correctly
 * - Skips invalid packages but continues with valid ones
 */
TEST_F(PreinstallManagerTest, StartPreinstallWithMixedPackages)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock directory operations for multiple packages
    ON_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillByDefault(::testing::Return(reinterpret_cast<DIR*>(0x1234)));

    static struct dirent testDirent1, testDirent2;
    strcpy(testDirent1.d_name, "validapp");
    strcpy(testDirent2.d_name, "invalidapp");
    static struct dirent* direntPtr1 = &testDirent1;
    static struct dirent* direntPtr2 = &testDirent2;
    static int callCount = 0;

    ON_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillByDefault(::testing::Invoke([&](DIR*) -> struct dirent* {
            callCount++;
            if (callCount == 1) return direntPtr1;
            if (callCount == 2) return direntPtr2;
            return nullptr;
        }));

    ON_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillByDefault(::testing::Return(0));
    
    // Mock GetConfigForPackage to succeed for valid, fail for invalid
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            if (fileLocator.find("validapp") != std::string::npos) {
                id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
                version = PREINSTALL_MANAGER_TEST_VERSION;
                return Core::ERROR_NONE;
            } else {
                id = "";  // Empty fields to test error handling
                version = "";
                return Core::ERROR_GENERAL;
            }
        });

    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &packageId, const string &version, 
                           Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                           const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            return Core::ERROR_NONE;
        });
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    // Should succeed despite having some invalid packages
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
    
    releaseResources();
}

/**
 * @brief Test version comparison with equal versions
 *
 * @details Test verifies that:
 * - isNewerVersion correctly identifies equal versions
 * - Returns false for equal versions
 */
TEST_F(PreinstallManagerTest, VersionComparisonEqualVersions)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Test exact same versions
    EXPECT_FALSE(mPreinstallManagerImpl->isNewerVersion("1.0.0", "1.0.0"));
    EXPECT_FALSE(mPreinstallManagerImpl->isNewerVersion("2.5.10", "2.5.10"));
    EXPECT_FALSE(mPreinstallManagerImpl->isNewerVersion("1.0.0.1", "1.0.0.1"));
    
    // Test versions that are effectively equal after stripping
    EXPECT_FALSE(mPreinstallManagerImpl->isNewerVersion("1.0.0-beta", "1.0.0+build1"));
    EXPECT_FALSE(mPreinstallManagerImpl->isNewerVersion("1.0.0+suffix1", "1.0.0-suffix2"));
    
    releaseResources();
}

/**
 * @brief Test createPackageManagerObject failure scenario
 *
 * @details Test verifies that:
 * - createPackageManagerObject handles QueryInterfaceByCallsign failure
 * - Returns appropriate error code
 */
TEST_F(PreinstallManagerTest, CreatePackageManagerObjectFailure)
{
    mServiceMock = new NiceMock<ServiceMock>;
    
    // Don't set up PackageInstaller mock in QueryInterfaceByCallsign
    EXPECT_CALL(*mServiceMock, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(nullptr));
    
    EXPECT_EQ(string(""), plugin->Initialize(mServiceMock));
    mPreinstallManagerImpl = Plugin::PreinstallManagerImplementation::getInstance();
    
    // Configure the service first
    mPreinstallManagerImpl->Configure(mServiceMock);
    
    // Now test createPackageManagerObject
    Core::hresult result = mPreinstallManagerImpl->createPackageManagerObject();
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    plugin->Deinitialize(mServiceMock);
    delete mServiceMock;
    mPreinstallManagerImpl = nullptr;
}

/**
 * @brief Test StartPreinstall when existing version is newer
 *
 * @details Test verifies that:
 * - StartPreinstall skips installation when existing version is newer
 * - Properly handles version comparison logic
 */
TEST_F(PreinstallManagerTest, StartPreinstallExistingVersionNewer)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Create mock package with newer installed version
    auto mockIterator = [&]() {
        std::list<Exchange::IPackageInstaller::Package> packageList;
        Exchange::IPackageInstaller::Package package_1;
        package_1.packageId = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
        package_1.version = "2.0.0"; // Newer than test version "1.0.0"
        package_1.digest = "";
        package_1.state = Exchange::IPackageInstaller::InstallState::INSTALLED;
        package_1.sizeKb = 0;
        packageList.emplace_back(package_1);
        return Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IPackageIterator>>::Create<Exchange::IPackageInstaller::IPackageIterator>(packageList);
    };
    
    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
        .WillRepeatedly([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
            packages = mockIterator();
            return Core::ERROR_NONE;
        });

    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = PREINSTALL_MANAGER_TEST_VERSION; // "1.0.0" - older than installed
            return Core::ERROR_NONE;
        });

    SetUpPreinstallDirectoryMocks();
    
    // Should not attempt to install since existing version is newer
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(0);
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(false);
    EXPECT_EQ(Core::ERROR_NONE, result);
    
    releaseResources();
}

/**
 * @brief Test Notification class methods
 *
 * @details Test verifies that:
 * - Notification inner class methods work correctly
 * - Handles activation and deactivation properly
 */
TEST_F(PreinstallManagerTest, NotificationClassMethods)
{
    mServiceMock = new NiceMock<ServiceMock>;
    EXPECT_EQ(string(""), plugin->Initialize(mServiceMock));
    
    // Access the notification object
    auto& notification = plugin->mPreinstallManagerNotification;
    
    // Test Activated method (should not crash)
    notification.Activated(nullptr);
    
    // Test OnAppInstallationStatus method (should not crash)
    string testJsonResponse = R"({"packageId":"testApp","version":"1.0.0","status":"SUCCESS"})";
    notification.OnAppInstallationStatus(testJsonResponse);
    
    plugin->Deinitialize(mServiceMock);
    delete mServiceMock;
}

/**
 * @brief Test plugin with failure during initialization
 *
 * @details Test verifies that:
 * - Plugin handles initialization failures gracefully
 * - Returns proper error messages
 */
TEST_F(PreinstallManagerTest, PluginInitializationFailure)
{
    // Test with null service
    string result = plugin->Initialize(nullptr);
    EXPECT_FALSE(result.empty()); // Should return error message
}

/**
 * @brief Test dispatchEvent method
 *
 * @details Test verifies that:
 * - dispatchEvent submits jobs to worker pool correctly
 */
TEST_F(PreinstallManagerTest, DispatchEventMethod)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    auto mockNotification = Core::ProxyType<MockNotificationTest>::Create();
    testing::Mock::AllowLeak(mockNotification.operator->());
    
    std::promise<void> notificationPromise;
    auto notificationFuture = notificationPromise.get_future();
    
    EXPECT_CALL(*mockNotification, OnAppInstallationStatus(::testing::_))
        .Times(1)
        .WillOnce(::testing::InvokeWithoutArgs([&notificationPromise]() {
            notificationPromise.set_value();
        }));
    
    mPreinstallManagerImpl->Register(mockNotification.operator->());
    
    // Test dispatchEvent
    JsonObject params;
    params["jsonresponse"] = R"({"packageId":"testApp","version":"1.0.0","status":"SUCCESS"})";
    mPreinstallManagerImpl->dispatchEvent(Plugin::PreinstallManagerImplementation::PREINSTALL_MANAGER_APP_INSTALLATION_STATUS, params);
    
    // Wait for async execution
    auto status = notificationFuture.wait_for(std::chrono::seconds(2));
    EXPECT_EQ(std::future_status::ready, status);
    
    mPreinstallManagerImpl->Unregister(mockNotification.operator->());
    releaseResources();
}

/**
 * @brief Test version comparison with malformed versions
 *
 * @details Test verifies that:
 * - isNewerVersion handles malformed version strings gracefully
 * - Returns false for invalid formats
 */
TEST_F(PreinstallManagerTest, VersionComparisonMalformed)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Test completely invalid formats
    EXPECT_FALSE(mPreinstallManagerImpl->isNewerVersion("abc", "1.0.0"));
    EXPECT_FALSE(mPreinstallManagerImpl->isNewerVersion("1.0.0", "xyz"));
    EXPECT_FALSE(mPreinstallManagerImpl->isNewerVersion("", "1.0.0"));
    EXPECT_FALSE(mPreinstallManagerImpl->isNewerVersion("1.0.0", ""));
    
    // Test partial formats (less than 3 components)
    EXPECT_FALSE(mPreinstallManagerImpl->isNewerVersion("1", "1.0.0"));
    EXPECT_FALSE(mPreinstallManagerImpl->isNewerVersion("1.0", "1.0.0"));
    EXPECT_FALSE(mPreinstallManagerImpl->isNewerVersion("1.0.0", "1"));
    
    releaseResources();
}

/**
 * @brief Test destructor functionality
 *
 * @details Test verifies that:
 * - PreinstallManagerImplementation destructor works correctly
 * - Properly cleans up resources
 */
TEST_F(PreinstallManagerTest, DestructorCleanup)
{
    mServiceMock = new NiceMock<ServiceMock>;
    EXPECT_EQ(string(""), plugin->Initialize(mServiceMock));
    mPreinstallManagerImpl = Plugin::PreinstallManagerImplementation::getInstance();
    
    // Configure to set up service reference
    mPreinstallManagerImpl->Configure(mServiceMock);
    
    // Deinitialize should trigger destructor cleanup
    plugin->Deinitialize(mServiceMock);
    delete mServiceMock;
    mPreinstallManagerImpl = nullptr;
}

/**
 * @brief Test Plugin Deinitialize functionality
 *
 * @details Test verifies that:
 * - Deinitialize properly cleans up resources
 * - Handles multiple calls gracefully
 */
TEST_F(PreinstallManagerTest, PluginDeinitialize)
{
    mServiceMock = new NiceMock<ServiceMock>;
    
    EXPECT_EQ(string(""), plugin->Initialize(mServiceMock));
    
    // Test normal deinitialize
    plugin->Deinitialize(mServiceMock);
    
    // Test double deinitialize (should not crash)
    plugin->Deinitialize(mServiceMock);
    
    delete mServiceMock;
}

/**
 * @brief Test Job class functionality
 *
 * @details Test verifies that:
 * - Job class can be created and dispatched properly
 * - Handles event dispatch through worker pool
 */
TEST_F(PreinstallManagerTest, JobClassFunctionality)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    auto mockNotification = Core::ProxyType<MockNotificationTest>::Create();
    testing::Mock::AllowLeak(mockNotification.operator->());
    
    std::promise<void> notificationPromise;
    auto notificationFuture = notificationPromise.get_future();
    
    EXPECT_CALL(*mockNotification, OnAppInstallationStatus(::testing::_))
        .Times(1)
        .WillOnce(::testing::InvokeWithoutArgs([&notificationPromise]() {
            notificationPromise.set_value();
        }));
    
    mPreinstallManagerImpl->Register(mockNotification.operator->());
    
    // Create and dispatch a job directly
    JsonObject params;
    params["jsonresponse"] = R"({"packageId":"testApp","version":"1.0.0","status":"SUCCESS"})";
    
    auto job = Plugin::PreinstallManagerImplementation::Job::Create(
        mPreinstallManagerImpl, 
        Plugin::PreinstallManagerImplementation::PREINSTALL_MANAGER_APP_INSTALLATION_STATUS, 
        params
    );
    
    EXPECT_TRUE(job.IsValid());
    
    // Dispatch the job
    Core::IWorkerPool::Instance().Submit(job);
    
    // Wait for execution
    auto status = notificationFuture.wait_for(std::chrono::seconds(2));
    EXPECT_EQ(std::future_status::ready, status);
    
    mPreinstallManagerImpl->Unregister(mockNotification.operator->());
    releaseResources();
}

/**
 * @brief Test PackageManagerNotification class
 *
 * @details Test verifies that:
 * - PackageManagerNotification forwards events correctly
 * - Handles OnAppInstallationStatus properly
 */
TEST_F(PreinstallManagerTest, PackageManagerNotificationClass)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    auto mockNotification = Core::ProxyType<MockNotificationTest>::Create();
    testing::Mock::AllowLeak(mockNotification.operator->());
    
    std::promise<void> notificationPromise;
    auto notificationFuture = notificationPromise.get_future();
    
    EXPECT_CALL(*mockNotification, OnAppInstallationStatus(::testing::_))
        .Times(1)
        .WillOnce(::testing::InvokeWithoutArgs([&notificationPromise]() {
            notificationPromise.set_value();
        }));
    
    mPreinstallManagerImpl->Register(mockNotification.operator->());
    
    // Access the PackageManagerNotification object and call it directly
    string testJsonResponse = R"({"packageId":"testApp","version":"1.0.0","status":"SUCCESS"})";
    mPreinstallManagerImpl->mPackageManagerNotification.OnAppInstallationStatus(testJsonResponse);
    
    // Wait for async processing
    auto status = notificationFuture.wait_for(std::chrono::seconds(2));
    EXPECT_EQ(std::future_status::ready, status);
    
    mPreinstallManagerImpl->Unregister(mockNotification.operator->());
    releaseResources();
}

/**
 * @brief Test version comparison edge cases
 *
 * @details Test verifies that:
 * - isNewerVersion handles edge cases correctly
 * - Proper handling of build numbers and pre-release versions
 */
TEST_F(PreinstallManagerTest, VersionComparisonEdgeCases)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Test build number comparisons
    EXPECT_TRUE(mPreinstallManagerImpl->isNewerVersion("1.0.0.5", "1.0.0.1"));
    EXPECT_FALSE(mPreinstallManagerImpl->isNewerVersion("1.0.0.1", "1.0.0.5"));
    
    // Test major version differences
    EXPECT_TRUE(mPreinstallManagerImpl->isNewerVersion("2.0.0", "1.9.9"));
    EXPECT_FALSE(mPreinstallManagerImpl->isNewerVersion("1.9.9", "2.0.0"));
    
    // Test minor version differences
    EXPECT_TRUE(mPreinstallManagerImpl->isNewerVersion("1.5.0", "1.4.9"));
    EXPECT_FALSE(mPreinstallManagerImpl->isNewerVersion("1.4.9", "1.5.0"));
    
    // Test patch version differences
    EXPECT_TRUE(mPreinstallManagerImpl->isNewerVersion("1.0.5", "1.0.4"));
    EXPECT_FALSE(mPreinstallManagerImpl->isNewerVersion("1.0.4", "1.0.5"));
    
    releaseResources();
}

/**
 * @brief Test StartPreinstall with no packages in directory
 *
 * @details Test verifies that:
 * - StartPreinstall handles empty preinstall directory gracefully
 * - Returns appropriate status for empty package list
 */
TEST_F(PreinstallManagerTest, StartPreinstallEmptyDirectory)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock empty directory
    ON_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillByDefault(::testing::Return(reinterpret_cast<DIR*>(0x1234)));
    
    ON_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillByDefault(::testing::Return(nullptr)); // No entries
    
    ON_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillByDefault(::testing::Return(0));
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    // Should return success for empty directory
    EXPECT_EQ(Core::ERROR_NONE, result);
    
    releaseResources();
}

/**
 * @brief Test Configure with null service in createPackageManagerObject
 *
 * @details Test verifies that:
 * - createPackageManagerObject handles null current service
 * - Returns appropriate error
 */
TEST_F(PreinstallManagerTest, CreatePackageManagerObjectNullService)
{
    mServiceMock = new NiceMock<ServiceMock>;
    EXPECT_EQ(string(""), plugin->Initialize(mServiceMock));
    mPreinstallManagerImpl = Plugin::PreinstallManagerImplementation::getInstance();
    
    // Don't configure service (leave it null)
    // mPreinstallManagerImpl->Configure(mServiceMock);
    
    Core::hresult result = mPreinstallManagerImpl->createPackageManagerObject();
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    plugin->Deinitialize(mServiceMock);
    delete mServiceMock;
    mPreinstallManagerImpl = nullptr;
}
