/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2024 RDK Management
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
#include <dirent.h>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

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

#define TIMEOUT   (2000)
#define PREINSTALL_MANAGER_PACKAGE_ID        "com.test.preinstall"
#define PREINSTALL_MANAGER_VERSION           "1.0.0"
#define PREINSTALL_MANAGER_FILE_LOCATOR      "/opt/preinstall/com.test.preinstall.tar.gz"
#define PREINSTALL_MANAGER_INSTALL_STATUS    "INSTALLED"
#define PREINSTALL_MANAGER_UNPACK_PATH       "/opt/apps/com.test.preinstall"
#define TEST_JSON_INSTALLATION_STATUS        R"({"packageId":"com.test.preinstall","version":"1.0.0","status":"INSTALLED"})"

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

    Core::hresult createResources()
    {
        Core::hresult status = Core::ERROR_GENERAL;
        mServiceMock = new NiceMock<ServiceMock>;
        mPackageInstallerMock = new NiceMock<PackageInstallerMock>;
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
                    if (name == "org.rdk.PackageManagerRDKEMS" || name == "PackageManager") {
                        if (id == Exchange::IPackageInstaller::ID) {
                            mPackageInstallerMock->AddRef();
                            return reinterpret_cast<void*>(mPackageInstallerMock);
                        }
                    }
                    return nullptr;
                }));

        EXPECT_CALL(*mPackageInstallerMock, Register(::testing::_))
            .WillRepeatedly(::testing::Invoke(
                [&](Exchange::IPackageInstaller::INotification* notification) {
                    mPackageInstallerNotification_cb = notification;
                    return Core::ERROR_NONE;
                }));
        
        EXPECT_CALL(*mPackageInstallerMock, AddRef())
            .WillRepeatedly(::testing::Return(1));
        
        EXPECT_CALL(*mPackageInstallerMock, Release())
            .WillRepeatedly(::testing::Return(0));

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
            EXPECT_CALL(*mPackageInstallerMock, Unregister(::testing::_))
                .WillRepeatedly(::testing::Invoke([&](Exchange::IPackageInstaller::INotification*) {
                    return Core::ERROR_NONE;
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
};

class NotificationHandler : public Exchange::IPreinstallManager::INotification {
    private:
        BEGIN_INTERFACE_MAP(Notification)
        INTERFACE_ENTRY(Exchange::IPreinstallManager::INotification)
        END_INTERFACE_MAP

    public:
        NotificationHandler() = default;
        virtual ~NotificationHandler() = default;

        std::mutex m_mutex;
        std::condition_variable m_condition_variable;
        uint32_t m_event_signalled = PreinstallManager_StateInvalid;
        string m_lastInstallationStatus;

        void OnAppInstallationStatus(const string& jsonresponse) override
        {
            TEST_LOG("OnAppInstallationStatus called with: %s", jsonresponse.c_str());
            
            std::unique_lock<std::mutex> lock(m_mutex);
            m_lastInstallationStatus = jsonresponse;
            m_event_signalled = PreinstallManager_onAppInstallationStatus;
            m_condition_variable.notify_one();
        }

        uint32_t WaitForEvent(uint32_t timeout_ms)
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            auto timeout = std::chrono::milliseconds(timeout_ms);
            
            m_condition_variable.wait_for(lock, timeout, [this]() {
                return m_event_signalled != PreinstallManager_StateInvalid;
            });
            
            return m_event_signalled;
        }

        void ResetEvent()
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_event_signalled = PreinstallManager_StateInvalid;
            m_lastInstallationStatus.clear();
        }
};

/*******************************************************************************************************************
 * Test Case for RegisteredMethodsUsingJsonRpcSuccess
 * Setting up PreinstallManager Plugin and creating required JSON-RPC resources
 * Verifying whether all methods exist or not
 * Releasing the PreinstallManager interface and all related test resources
 ********************************************************************************************************************/
TEST_F(PreinstallManagerTest, RegisteredMethodsUsingJsonRpcSuccess)
{
    createResources();

    EXPECT_TRUE(mJsonRpcHandler.Exists(_T("startPreinstall")));

    releaseResources();
}

/*
 * Test Case for StartPreinstallUsingComRpcSuccess
 * Setting up PreinstallManager Plugin and creating required COM-RPC resources
 * Setting Mock for PackageInstaller Install() to simulate successful installation
 * Calling StartPreinstall() with forceInstall=false using QueryInterface
 * Verifying the return of the API
 * Releasing the PreinstallManager Interface object and all related test resources
 */
TEST_F(PreinstallManagerTest, StartPreinstallUsingComRpcSuccess)
{
    createResources();

    // Get the IPreinstallManager interface using QueryInterface
    Exchange::IPreinstallManager* preinstallInterface = static_cast<Exchange::IPreinstallManager*>(
        plugin->QueryInterface(Exchange::IPreinstallManager::ID));
    ASSERT_NE(preinstallInterface, nullptr);

    // Use RAII-style cleanup
    auto cleanup = [&]() {
        if (preinstallInterface) {
            preinstallInterface->Release();
        }
        releaseResources();
    };

    // Mock stat to return success for preinstall directory and files
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::StrContains("/opt/preinstall"), ::testing::_))
        .WillRepeatedly(::testing::DoAll(
            ::testing::Invoke([](const char* path, struct stat* buf) {
                buf->st_mode = S_IFDIR | 0755; // Directory with read/write/execute permissions
                return 0;
            })));
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::StrContains(".tar.gz"), ::testing::_))
        .WillRepeatedly(::testing::DoAll(
            ::testing::Invoke([](const char* path, struct stat* buf) {
                buf->st_mode = S_IFREG | 0644; // Regular file with read/write permissions
                buf->st_size = 1024; // File size
                return 0;
            })));
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(0));
    
    // Mock access to return success for file access checks
    EXPECT_CALL(*p_wrapsImplMock, access(::testing::StrContains("/opt/preinstall"), ::testing::_))
        .WillRepeatedly(::testing::Return(0));
    EXPECT_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(0));
    
    // Create a mock dirent structure for package discovery
    static struct dirent mockDirent1;
    snprintf(mockDirent1.d_name, sizeof(mockDirent1.d_name), "%s.tar.gz", PREINSTALL_MANAGER_PACKAGE_ID);
    
    // Mock directory operations for directory scanning
    EXPECT_CALL(*p_wrapsImplMock, opendir(::testing::StrContains("/opt/preinstall")))
        .WillRepeatedly(::testing::Return(reinterpret_cast<DIR*>(0x12345678))); // Non-null DIR*
    EXPECT_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillRepeatedly(::testing::Return(reinterpret_cast<DIR*>(0x12345678))); // Non-null DIR*
    EXPECT_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillOnce(::testing::Return(&mockDirent1)) // Return our mock package file
        .WillRepeatedly(::testing::Return(nullptr)); // Then end of directory
    EXPECT_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillRepeatedly(::testing::Return(0));

    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtLeast(1))
        .WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    Core::hresult result = preinstallInterface->StartPreinstall(false);
    EXPECT_EQ(result, Core::ERROR_NONE);

    cleanup();
    preinstallInterface = nullptr;
}

/*
 * Test Case for StartPreinstallWithForceInstallUsingComRpcSuccess
 * Setting up PreinstallManager Plugin and creating required COM-RPC resources  
 * Setting Mock for PackageInstaller Install() to simulate successful installation
 * Calling StartPreinstall() with forceInstall=true using QueryInterface
 * Verifying the return of the API
 * Releasing the PreinstallManager Interface object and all related test resources
 */
TEST_F(PreinstallManagerTest, StartPreinstallWithForceInstallUsingComRpcSuccess)
{
    createResources();

    // Get the IPreinstallManager interface using QueryInterface
    Exchange::IPreinstallManager* preinstallInterface = static_cast<Exchange::IPreinstallManager*>(
        plugin->QueryInterface(Exchange::IPreinstallManager::ID));
    ASSERT_NE(preinstallInterface, nullptr);

    // Use RAII-style cleanup
    auto cleanup = [&]() {
        if (preinstallInterface) {
            preinstallInterface->Release();
        }
        releaseResources();
    };

    // Mock stat to return success for preinstall directory and files
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::StrContains("/opt/preinstall"), ::testing::_))
        .WillRepeatedly(::testing::DoAll(
            ::testing::Invoke([](const char* path, struct stat* buf) {
                buf->st_mode = S_IFDIR | 0755; // Directory with read/write/execute permissions
                return 0;
            })));
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::StrContains(".tar.gz"), ::testing::_))
        .WillRepeatedly(::testing::DoAll(
            ::testing::Invoke([](const char* path, struct stat* buf) {
                buf->st_mode = S_IFREG | 0644; // Regular file with read/write permissions
                buf->st_size = 1024; // File size
                return 0;
            })));
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(0));
    
    // Mock access to return success for file access checks
    EXPECT_CALL(*p_wrapsImplMock, access(::testing::StrContains("/opt/preinstall"), ::testing::_))
        .WillRepeatedly(::testing::Return(0));
    EXPECT_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(0));
    
    // Create a mock dirent structure for package discovery
    static struct dirent mockDirent2;
    snprintf(mockDirent2.d_name, sizeof(mockDirent2.d_name), "%s.tar.gz", PREINSTALL_MANAGER_PACKAGE_ID);
    
    // Mock directory operations for directory scanning
    EXPECT_CALL(*p_wrapsImplMock, opendir(::testing::StrContains("/opt/preinstall")))
        .WillRepeatedly(::testing::Return(reinterpret_cast<DIR*>(0x12345678))); // Non-null DIR*
    EXPECT_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillRepeatedly(::testing::Return(reinterpret_cast<DIR*>(0x12345678))); // Non-null DIR*
    EXPECT_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillOnce(::testing::Return(&mockDirent)) // Return our mock package file
        .WillRepeatedly(::testing::Return(nullptr)); // Then end of directory
    EXPECT_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillRepeatedly(::testing::Return(0));

    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtLeast(1))
        .WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    Core::hresult result = preinstallInterface->StartPreinstall(true);
    EXPECT_EQ(result, Core::ERROR_NONE);

    cleanup();
    preinstallInterface = nullptr;
}

/*
 * Test Case for StartPreinstallUsingComRpcFailurePackageInstallerObjectIsNull
 * Setting up only PreinstallManager Plugin and creating required COM-RPC resources
 * PackageInstaller Interface object is not created and hence the API should return error
 * Releasing the PreinstallManager Interface object only
 */
TEST_F(PreinstallManagerTest, StartPreinstallUsingComRpcFailurePackageInstallerObjectIsNull)
{
    // Setup with null PackageInstaller to simulate failure
    mServiceMock = new NiceMock<ServiceMock>;
    p_wrapsImplMock = new NiceMock<WrapsImplMock>;
    Wraps::setImpl(p_wrapsImplMock);

    PluginHost::IFactories::Assign(&factoriesImplementation);
    dispatcher = static_cast<PLUGINHOST_DISPATCHER*>(
        plugin->QueryInterface(PLUGINHOST_DISPATCHER_ID));
    dispatcher->Activate(mServiceMock);
    
    // Mock to return nullptr for PackageInstaller
    EXPECT_CALL(*mServiceMock, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(nullptr));

    ON_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(-1));

    EXPECT_EQ(string(""), plugin->Initialize(mServiceMock));
    mPreinstallManagerImpl = Plugin::PreinstallManagerImplementation::getInstance();

    // Get the IPreinstallManager interface using QueryInterface
    Exchange::IPreinstallManager* preinstallInterface = static_cast<Exchange::IPreinstallManager*>(
        plugin->QueryInterface(Exchange::IPreinstallManager::ID));
    ASSERT_NE(preinstallInterface, nullptr);

    // Use RAII-style cleanup
    auto cleanup = [&]() {
        if (preinstallInterface) {
            preinstallInterface->Release();
        }
        Wraps::setImpl(nullptr);
        if (p_wrapsImplMock != nullptr) {
            delete p_wrapsImplMock;
            p_wrapsImplMock = nullptr;
        }
        dispatcher->Deactivate();
        dispatcher->Release();
        plugin->Deinitialize(mServiceMock);
        delete mServiceMock;
        mPreinstallManagerImpl = nullptr;
    };

    Core::hresult result = preinstallInterface->StartPreinstall(false);
    EXPECT_EQ(result, Core::ERROR_GENERAL);

    cleanup();
    preinstallInterface = nullptr;
}

/*
 * Test Case for RegisterNotificationUsingComRpcSuccess
 * Setting up PreinstallManager Plugin and creating required COM-RPC resources
 * Creating a notification handler and registering it using QueryInterface
 * Verifying successful registration
 * Unregistering the notification and verifying successful unregistration
 * Releasing the PreinstallManager Interface object and all related test resources
 */
TEST_F(PreinstallManagerTest, RegisterNotificationUsingComRpcSuccess)
{
    createResources();

    // Get the IPreinstallManager interface using QueryInterface
    Exchange::IPreinstallManager* preinstallInterface = static_cast<Exchange::IPreinstallManager*>(
        plugin->QueryInterface(Exchange::IPreinstallManager::ID));
    ASSERT_NE(preinstallInterface, nullptr);

    // Use RAII-style cleanup
    auto cleanup = [&]() {
        if (preinstallInterface) {
            preinstallInterface->Release();
        }
        releaseResources();
    };

    Core::Sink<NotificationHandler> notification;
    
    Core::hresult result = preinstallInterface->Register(&notification);
    EXPECT_EQ(result, Core::ERROR_NONE);

    result = preinstallInterface->Unregister(&notification);
    EXPECT_EQ(result, Core::ERROR_NONE);

    cleanup();
    preinstallInterface = nullptr;
}

/*
 * Test Case for UnregisterNotificationWithoutRegisterUsingComRpcFailure
 * Setting up PreinstallManager Plugin and creating required COM-RPC resources
 * Creating a notification handler but not registering it
 * Attempting to unregister the notification should fail using QueryInterface
 * Releasing the PreinstallManager Interface object and all related test resources
 */
TEST_F(PreinstallManagerTest, UnregisterNotificationWithoutRegisterUsingComRpcFailure)
{
    createResources();

    // Get the IPreinstallManager interface using QueryInterface
    Exchange::IPreinstallManager* preinstallInterface = static_cast<Exchange::IPreinstallManager*>(
        plugin->QueryInterface(Exchange::IPreinstallManager::ID));
    ASSERT_NE(preinstallInterface, nullptr);

    // Use RAII-style cleanup
    auto cleanup = [&]() {
        if (preinstallInterface) {
            preinstallInterface->Release();
        }
        releaseResources();
    };

    Core::Sink<NotificationHandler> notification;
    
    Core::hresult result = preinstallInterface->Unregister(&notification);
    EXPECT_EQ(result, Core::ERROR_GENERAL);

    cleanup();
    preinstallInterface = nullptr;
}

/*
 * Test Case for OnAppInstallationStatusNotificationSuccess
 * Setting up PreinstallManager Plugin and creating required COM-RPC resources
 * Registering a notification handler using Sink<NotificationHandler>
 * Simulating an installation status callback from PackageInstaller
 * Verifying that the notification is received
 * Releasing the PreinstallManager Interface object and all related test resources
 */
TEST_F(PreinstallManagerTest, OnAppInstallationStatusNotificationSuccess)
{
    createResources();

    // Get the IPreinstallManager interface using QueryInterface
    Exchange::IPreinstallManager* preinstallInterface = static_cast<Exchange::IPreinstallManager*>(
        plugin->QueryInterface(Exchange::IPreinstallManager::ID));
    ASSERT_NE(preinstallInterface, nullptr);

    // Use RAII-style cleanup
    auto cleanup = [&]() {
        if (preinstallInterface) {
            preinstallInterface->Release();
        }
        releaseResources();
    };

    Core::Sink<NotificationHandler> notification;
    Core::hresult result = preinstallInterface->Register(&notification);
    ASSERT_EQ(result, Core::ERROR_NONE);

    // Simulate installation status callback
    const string testInstallationStatus = TEST_JSON_INSTALLATION_STATUS;
    
    ASSERT_NE(mPackageInstallerNotification_cb, nullptr) << "PackageInstaller notification callback is null";
    
    // Reset event before triggering
    notification.ResetEvent();
    
    mPackageInstallerNotification_cb->OnAppInstallationStatus(testInstallationStatus);

    // Wait for notification with shorter timeout
    uint32_t eventReceived = notification.WaitForEvent(TIMEOUT);
    EXPECT_EQ(eventReceived, PreinstallManager_onAppInstallationStatus);
    EXPECT_EQ(notification.m_lastInstallationStatus, testInstallationStatus);

    result = preinstallInterface->Unregister(&notification);
    EXPECT_EQ(result, Core::ERROR_NONE);

    cleanup();
    preinstallInterface = nullptr;
}
