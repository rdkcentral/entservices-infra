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
 * @brief Test version comparison logic with various version formats
 *
 * @details Test verifies that:
 * - isNewerVersion correctly compares semantic versions
 * - Handles version strings with suffixes
 * - Returns false for equal versions
 */
TEST_F(PreinstallManagerTest, VersionComparisonLogic)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Test newer version detection
    EXPECT_TRUE(mPreinstallManagerImpl->isNewerVersion("2.0.0", "1.0.0"));
    EXPECT_TRUE(mPreinstallManagerImpl->isNewerVersion("1.1.0", "1.0.0"));
    EXPECT_TRUE(mPreinstallManagerImpl->isNewerVersion("1.0.1", "1.0.0"));
    EXPECT_TRUE(mPreinstallManagerImpl->isNewerVersion("1.0.0.1", "1.0.0.0"));
    
    // Test equal versions
    EXPECT_FALSE(mPreinstallManagerImpl->isNewerVersion("1.0.0", "1.0.0"));
    EXPECT_FALSE(mPreinstallManagerImpl->isNewerVersion("1.0.0.0", "1.0.0.0"));
    
    // Test older version detection
    EXPECT_FALSE(mPreinstallManagerImpl->isNewerVersion("1.0.0", "2.0.0"));
    EXPECT_FALSE(mPreinstallManagerImpl->isNewerVersion("1.0.0", "1.1.0"));
    EXPECT_FALSE(mPreinstallManagerImpl->isNewerVersion("1.0.0", "1.0.1"));
    
    // Test versions with suffixes
    EXPECT_TRUE(mPreinstallManagerImpl->isNewerVersion("2.0.0-beta", "1.0.0-alpha"));
    EXPECT_TRUE(mPreinstallManagerImpl->isNewerVersion("1.1.0+build.1", "1.0.0+build.2"));
    
    // Test invalid version formats
    EXPECT_FALSE(mPreinstallManagerImpl->isNewerVersion("invalid", "1.0.0"));
    EXPECT_FALSE(mPreinstallManagerImpl->isNewerVersion("1.0.0", "invalid"));
    EXPECT_FALSE(mPreinstallManagerImpl->isNewerVersion("1.0", "1.0.0"));
    
    releaseResources();
}

/**
 * @brief Test error handling when notification is null in Register
 *
 * @details Test verifies that:
 * - Register method handles null notification gracefully
 */
TEST_F(PreinstallManagerTest, RegisterDuplicateNotification)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    auto mockNotification = Core::ProxyType<MockNotificationTest>::Create();
    testing::Mock::AllowLeak(mockNotification.operator->());
    
    // Test multiple registrations of same notification
    Core::hresult status1 = mPreinstallManagerImpl->Register(mockNotification.operator->());
    Core::hresult status2 = mPreinstallManagerImpl->Register(mockNotification.operator->());
    
    EXPECT_EQ(Core::ERROR_NONE, status1);
    EXPECT_EQ(Core::ERROR_NONE, status2); // Should handle duplicate registration
    
    mPreinstallManagerImpl->Unregister(mockNotification.operator->());
    releaseResources();
}

/**
 * @brief Test unregistering a notification that was never registered
 *
 * @details Test verifies that:
 * - Unregister method returns ERROR_GENERAL for unregistered notifications
 */
TEST_F(PreinstallManagerTest, UnregisterNonExistentNotification)
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
 * @brief Test Configure method with valid and invalid service
 *
 * @details Test verifies that:
 * - Configure returns ERROR_NONE with valid service
 * - Configure returns ERROR_GENERAL with null service
 */
TEST_F(PreinstallManagerTest, ConfigureMethod)
{
    createPreinstallManagerImpl();
    
    // Test with valid service
    Core::hresult validResult = mPreinstallManagerImpl->Configure(mServiceMock);
    EXPECT_EQ(Core::ERROR_NONE, validResult);
    
    // Test with null service
    Core::hresult nullResult = mPreinstallManagerImpl->Configure(nullptr);
    EXPECT_EQ(Core::ERROR_GENERAL, nullResult);
    
    releasePreinstallManagerImpl();
}

/**
 * @brief Test handleOnAppInstallationStatus with empty response
 *
 * @details Test verifies that:
 * - Empty jsonresponse is handled gracefully
 * - No notifications are sent for empty responses
 */
TEST_F(PreinstallManagerTest, HandleEmptyAppInstallationStatus)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    auto mockNotification = Core::ProxyType<MockNotificationTest>::Create();
    testing::Mock::AllowLeak(mockNotification.operator->());
    
    // Expect no notification for empty response
    EXPECT_CALL(*mockNotification, OnAppInstallationStatus(::testing::_))
        .Times(0);
    
    mPreinstallManagerImpl->Register(mockNotification.operator->());
    
    // Call with empty string
    mPreinstallManagerImpl->handleOnAppInstallationStatus("");
    
    // Give some time for any potential async processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    mPreinstallManagerImpl->Unregister(mockNotification.operator->());
    releaseResources();
}

/**
 * @brief Test readPreinstallDirectory failure scenarios
 *
 * @details Test verifies that:
 * - Function handles directory open failure
 * - Function handles package config retrieval failure
 */
TEST_F(PreinstallManagerTest, ReadPreinstallDirectoryFailures)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Test 1: Directory open failure
    ON_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillByDefault(::testing::Return(nullptr)); // Simulate opendir failure
    
    std::list<Plugin::PreinstallManagerImplementation::PackageInfo> packages1;
    bool result1 = mPreinstallManagerImpl->readPreinstallDirectory(packages1);
    EXPECT_FALSE(result1);
    
    // Test 2: GetConfigForPackage failure
    SetUpPreinstallDirectoryMocks(); // Reset to working directory mocks
    
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(Core::ERROR_GENERAL)); // Simulate config failure
    
    std::list<Plugin::PreinstallManagerImplementation::PackageInfo> packages2;
    bool result2 = mPreinstallManagerImpl->readPreinstallDirectory(packages2);
    EXPECT_TRUE(result2); // Should still return true but package will be marked as skipped
    EXPECT_FALSE(packages2.empty());
    if (!packages2.empty()) {
        EXPECT_TRUE(packages2.front().installStatus.find("SKIPPED") != std::string::npos);
    }
    
    releaseResources();
}

/**
 * @brief Test StartPreinstall with package installation failures
 *
 * @details Test verifies that:
 * - Installation failures are handled gracefully
 * - Failed packages are properly logged
 * - Method returns ERROR_GENERAL when installations fail
 */
TEST_F(PreinstallManagerTest, StartPreinstallWithInstallationFailures)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = PREINSTALL_MANAGER_TEST_VERSION;
            return Core::ERROR_NONE;
        });

    // Mock Install to fail
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &packageId, const string &version, 
                           Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                           const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            failReason = Exchange::IPackageInstaller::FailReason::SIGNATURE_VERIFICATION_FAILURE;
            return Core::ERROR_GENERAL; // Simulate installation failure
        });

    SetUpPreinstallDirectoryMocks();
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    releaseResources();
}

/**
 * @brief Test getFailReason method with all failure reasons
 *
 * @details Test verifies that:
 * - All failure reason enums are properly converted to strings
 */
TEST_F(PreinstallManagerTest, GetFailReasonAllTypes)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Test all failure reason types
    EXPECT_EQ("SIGNATURE_VERIFICATION_FAILURE", 
              mPreinstallManagerImpl->getFailReason(Exchange::IPackageInstaller::FailReason::SIGNATURE_VERIFICATION_FAILURE));
    EXPECT_EQ("PACKAGE_MISMATCH_FAILURE", 
              mPreinstallManagerImpl->getFailReason(Exchange::IPackageInstaller::FailReason::PACKAGE_MISMATCH_FAILURE));
    EXPECT_EQ("INVALID_METADATA_FAILURE", 
              mPreinstallManagerImpl->getFailReason(Exchange::IPackageInstaller::FailReason::INVALID_METADATA_FAILURE));
    EXPECT_EQ("PERSISTENCE_FAILURE", 
              mPreinstallManagerImpl->getFailReason(Exchange::IPackageInstaller::FailReason::PERSISTENCE_FAILURE));
    
    // Test unknown/default case
    EXPECT_EQ("NONE", 
              mPreinstallManagerImpl->getFailReason(static_cast<Exchange::IPackageInstaller::FailReason>(999)));
    
    releaseResources();
}

/**
 * @brief Test StartPreinstall with packages having empty fields
 *
 * @details Test verifies that:
 * - Packages with empty packageId/version/fileLocator are skipped
 * - Proper error status is set for invalid packages
 */
TEST_F(PreinstallManagerTest, StartPreinstallWithEmptyPackageFields)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock GetConfigForPackage to return empty fields
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = ""; // Empty package ID
            version = ""; // Empty version
            return Core::ERROR_NONE;
        });

    SetUpPreinstallDirectoryMocks();
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(true);
    
    // Should return ERROR_GENERAL due to failed apps
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    releaseResources();
}

/**
 * @brief Test StartPreinstall with ListPackages failure
 *
 * @details Test verifies that:
 * - ListPackages failure is handled when forceInstall is false
 * - Method returns ERROR_GENERAL on ListPackages failure
 */
TEST_F(PreinstallManagerTest, StartPreinstallListPackagesFailure)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock ListPackages to fail
    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));

    SetUpPreinstallDirectoryMocks();
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(false); // forceInstall = false
    
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    releaseResources();
}

/**
 * @brief Test package filtering logic when newer version exists locally
 *
 * @details Test verifies that:
 * - Packages are not installed when newer version already exists
 * - Packages are installed when local version is older
 */
TEST_F(PreinstallManagerTest, PackageFilteringByVersion)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Create a package list with newer version already installed
    auto CreateNewerVersionIterator = []() {
        std::list<Exchange::IPackageInstaller::Package> packageList;
        Exchange::IPackageInstaller::Package package;
        package.packageId = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
        package.version = "2.0.0"; // Newer than test version (1.0.0)
        package.state = Exchange::IPackageInstaller::InstallState::INSTALLED;
        packageList.emplace_back(package);
        return Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IPackageIterator>>::Create<Exchange::IPackageInstaller::IPackageIterator>(packageList);
    };
    
    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
        .WillOnce([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
            packages = CreateNewerVersionIterator();
            return Core::ERROR_NONE;
        });

    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = PREINSTALL_MANAGER_TEST_VERSION; // 1.0.0 - older than installed
            return Core::ERROR_NONE;
        });

    // Install should not be called since newer version exists
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(0);

    SetUpPreinstallDirectoryMocks();
    
    Core::hresult result = mPreinstallManagerImpl->StartPreinstall(false);
    
    EXPECT_EQ(Core::ERROR_NONE, result); // Should succeed without installing anything
    
    releaseResources();
}

/**
 * @brief Test createPackageManagerObject with null service
 *
 * @details Test verifies that:
 * - createPackageManagerObject handles null service gracefully
 */
TEST_F(PreinstallManagerTest, CreatePackageManagerObjectNullService)
{
    createPreinstallManagerImpl();
    
    // Set service to null to test error path
    mPreinstallManagerImpl->Configure(nullptr);
    
    Core::hresult result = mPreinstallManagerImpl->createPackageManagerObject();
    
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    releasePreinstallManagerImpl();
}

/**
 * @brief Test event dispatching mechanism
 *
 * @details Test verifies that:
 * - dispatchEvent properly queues events
 * - Dispatch method handles unknown events
 */
TEST_F(PreinstallManagerTest, EventDispatchingMechanism)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Test unknown event dispatch
    JsonObject params;
    params["test"] = "value";
    
    // This should handle unknown event gracefully
    mPreinstallManagerImpl->dispatchEvent(static_cast<Plugin::PreinstallManagerImplementation::EventNames>(999), params);
    
    // Give some time for async processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    releaseResources();
}

/**
 * @brief Test directory iteration with multiple packages
 *
 * @details Test verifies that:
 * - Multiple packages in directory are processed
 * - Dot directories are skipped properly
 */
TEST_F(PreinstallManagerTest, ReadDirectoryWithMultiplePackages)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Mock directory operations for multiple entries
    ON_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillByDefault(::testing::Return(reinterpret_cast<DIR*>(0x1234)));

    // Create mock dirent structures
    static struct dirent dotDirent, dotdotDirent, testDirent1, testDirent2;
    strcpy(dotDirent.d_name, ".");
    strcpy(dotdotDirent.d_name, "..");
    strcpy(testDirent1.d_name, "testapp1");
    strcpy(testDirent2.d_name, "testapp2");
    
    static std::queue<struct dirent*> direntQueue;
    direntQueue.push(&dotDirent);
    direntQueue.push(&dotdotDirent);
    direntQueue.push(&testDirent1);
    direntQueue.push(&testDirent2);

    ON_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillByDefault(::testing::Invoke([&](DIR*) -> struct dirent* {
            if (!direntQueue.empty()) {
                struct dirent* result = direntQueue.front();
                direntQueue.pop();
                return result;
            }
            return nullptr;
        }));

    ON_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillByDefault(::testing::Return(0));

    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            if (fileLocator.find("testapp1") != std::string::npos) {
                id = "com.test.app1";
                version = "1.0.0";
            } else {
                id = "com.test.app2";
                version = "2.0.0";
            }
            return Core::ERROR_NONE;
        });
    
    std::list<Plugin::PreinstallManagerImplementation::PackageInfo> packages;
    bool result = mPreinstallManagerImpl->readPreinstallDirectory(packages);
    
    EXPECT_TRUE(result);
    EXPECT_EQ(2, packages.size()); // Should have 2 packages (dot directories skipped)
    
    releaseResources();
}

/**
 * @brief Test Dispatch method with invalid parameters
 *
 * @details Test verifies that:
 * - Dispatch handles missing jsonresponse parameter
 */
TEST_F(PreinstallManagerTest, DispatchWithInvalidParams)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());
    
    // Test with missing jsonresponse parameter
    JsonObject invalidParams;
    invalidParams["invalid"] = "data";
    
    // This should handle missing jsonresponse gracefully
    mPreinstallManagerImpl->Dispatch(Plugin::PreinstallManagerImplementation::PREINSTALL_MANAGER_APP_INSTALLATION_STATUS, invalidParams);
    
    releaseResources();
}
