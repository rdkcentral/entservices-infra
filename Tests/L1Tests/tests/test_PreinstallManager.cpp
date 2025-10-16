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

#define PREINSTALL_PACKAGE_ID           "com.test.preinstall.app"
#define PREINSTALL_PACKAGE_VERSION      "1.0.0"
#define PREINSTALL_PACKAGE_FILELOCATION "/opt/preinstall/testapp/package.wgt"
#define PREINSTALL_NEWER_VERSION        "2.0.0"
#define PREINSTALL_OLDER_VERSION        "0.9.0"
#define PREINSTALL_MANAGER_PACKAGE_ID        "com.test.preinstall"
#define PREINSTALL_MANAGER_VERSION           "1.0.0"
#define PREINSTALL_MANAGER_FILE_LOCATOR      "/opt/preinstall/com.test.preinstall.tar.gz"
#define PREINSTALL_MANAGER_INSTALL_STATUS    "INSTALLED"
#define PREINSTALL_MANAGER_UNPACK_PATH       "/opt/apps/com.test.preinstall"
#define TEST_JSON_INSTALLATION_STATUS        R"({"packageId":"com.test.preinstall","version":"1.0.0","status":"INSTALLED"})"

typedef enum : uint32_t {
    PreinstallManager_invalidEvent = 0,
    PreinstallManager_onAppInstallationStatusEvent
} PreinstallManagerTest_events_t;

using ::testing::NiceMock;
using namespace WPEFramework;
using namespace std;

namespace WPEFramework {
namespace Plugin {
// Make PreinstallManagerL1Test a friend of PreinstallManager
class PreinstallManagerL1Test;
} // namespace Plugin
} // namespace WPEFramework

class NotificationTest : public Exchange::IPreinstallManager::INotification 
{
private:
    BEGIN_INTERFACE_MAP(NotificationTest)
    INTERFACE_ENTRY(Exchange::IPreinstallManager::INotification)
    END_INTERFACE_MAP

public:
    mutable mutex m_mutex;
    mutable condition_variable m_condition_variable;
    mutable uint32_t m_event_signal = PreinstallManager_invalidEvent;
    mutable string receivedJsonResponse;

    void OnAppInstallationStatus(const std::string& jsonresponse) override 
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        receivedJsonResponse = jsonresponse;
        m_event_signal = PreinstallManager_onAppInstallationStatusEvent;
        m_condition_variable.notify_one();
    }

    uint32_t WaitForEventStatus(uint32_t timeout_ms, PreinstallManagerTest_events_t status) const
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        auto now = std::chrono::steady_clock::now();
        auto timeout = std::chrono::milliseconds(timeout_ms);
        if (m_condition_variable.wait_until(lock, now + timeout) == std::cv_status::timeout)
        {
             TEST_LOG("Timeout waiting for request status event");
             return PreinstallManager_invalidEvent;
        }
        return m_event_signal;
    }
};

namespace {
const string callSign = _T("PreinstallManager");
}

class PreinstallManagerL1Test : public ::testing::Test {
protected:
    ServiceMock* mServiceMock = nullptr;
    PackageInstallerMock* mPackageInstallerMock = nullptr;
    WrapsImplMock* p_wrapsImplMock = nullptr;
    Core::JSONRPC::Message message;
    FactoriesImplementation factoriesImplementation;
    PLUGINHOST_DISPATCHER* dispatcher;

    Core::ProxyType<Plugin::PreinstallManager> plugin;
    Plugin::PreinstallManagerImplementation* mPreinstallManagerImpl;
    Exchange::IPackageInstaller::INotification* mPackageManagerNotification_cb = nullptr;
    Exchange::IPreinstallManager::INotification* mPreinstallManagerNotification = nullptr;

    Core::ProxyType<WorkerPoolImplementation> workerPool;
    Core::JSONRPC::Handler& mJsonRpcHandler;
    DECL_CORE_JSONRPC_CONX connection;
    string mJsonRpcResponse;

    PreinstallManagerL1Test()
        : workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(
            2, Core::Thread::DefaultStackSize(), 16))
        , mJsonRpcHandler(Core::JSONRPC::Handler::CreateHandler())
        , connection(1, _T("PreinstallManager.1"), mJsonRpcHandler)
    {
        Core::IWorkerPool::Assign(&(*workerPool));
        workerPool->Run();
        plugin = Core::ProxyType<Plugin::PreinstallManager>::Create();
    }

    virtual ~PreinstallManagerL1Test() override
    {
        Core::IWorkerPool::Assign(nullptr);
        workerPool.Release();
    }

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
                    mPackageManagerNotification_cb = notification;
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

        if (mPackageInstallerMock != nullptr && mPackageManagerNotification_cb != nullptr)
        {
            ON_CALL(*mPackageInstallerMock, Unregister(::testing::_))
                .WillByDefault(::testing::Invoke([&]() {
                    return 0;
                }));
            mPackageManagerNotification_cb = nullptr;
        }

        if (mPackageInstallerMock != nullptr)
        {
            EXPECT_CALL(*mPackageInstallerMock, Unregister(::testing::_))
                .WillRepeatedly(::testing::Return(Core::ERROR_NONE));

            EXPECT_CALL(*mPackageInstallerMock, Release())
                .WillOnce(::testing::Invoke(
                    [&]() {
                        delete mPackageInstallerMock;
                        mPackageInstallerMock = nullptr;
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
        
        if (mServiceMock != nullptr)
        {
            delete mServiceMock;
            mServiceMock = nullptr;
        }

        PluginHost::IFactories::Assign(nullptr);
        TEST_LOG("releaseResources - All done!");
    }
};

class PreinstallManagerL1TestWithParam : public PreinstallManagerL1Test, public ::testing::WithParamInterface<string>
{
protected:
    Core::JSONRPC::Connection connection;
    string method;
    string parameters;
    string result;

    PreinstallManagerL1TestWithParam()
        : PreinstallManagerL1Test()
        , connection(1, _T("PreinstallManager.1"))
    {
    }
};

/**
 * @brief : Test PreinstallManager interface register/unregister callbacks
 *          
 *          Register a notification callback and check to make sure it's registered
 *          Unregister the callback and make sure it's unregistered
 */
TEST_F(PreinstallManagerL1Test, RegisterUnregisterCallback)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());

    NotificationTest notification;
    
    // Test Register
    EXPECT_EQ(Core::ERROR_NONE, mPreinstallManagerImpl->Register(&notification));
    
    // Test Unregister  
    EXPECT_EQ(Core::ERROR_NONE, mPreinstallManagerImpl->Unregister(&notification));
    
    releaseResources();
}

/**
 * @brief : Test PreinstallManager onAppInstallationStatus notification
 *          
 *          Register for notifications and trigger an app installation status event
 *          Verify that the notification is received with correct data
 */
TEST_F(PreinstallManagerL1Test, OnAppInstallationStatusNotification)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());

    NotificationTest notification;
    
    // Register for notifications
    EXPECT_EQ(Core::ERROR_NONE, mPreinstallManagerImpl->Register(&notification));
    
    // Simulate app installation status from PackageManager
    string testJsonResponse = TEST_JSON_INSTALLATION_STATUS;
    
    if (mPackageManagerNotification_cb != nullptr) {
        mPackageManagerNotification_cb->OnAppInstallationStatus(testJsonResponse);
        
        // Wait for the notification
        EXPECT_EQ(PreinstallManager_onAppInstallationStatusEvent, 
                  notification.WaitForEventStatus(TIMEOUT, PreinstallManager_onAppInstallationStatusEvent));
        
        // Verify the received data
        EXPECT_EQ(testJsonResponse, notification.receivedJsonResponse);
    }
    
    // Unregister
    EXPECT_EQ(Core::ERROR_NONE, mPreinstallManagerImpl->Unregister(&notification));
    
    releaseResources();
}

/**
 * @brief : Test PreinstallManager StartPreinstall method
 *          
 *          Call StartPreinstall and verify the response
 */
TEST_F(PreinstallManagerL1Test, StartPreinstall)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());

    // Mock the package manager installer calls
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Invoke(
            [&](const string& fileLocator, string& packageId, string& version, Exchange::RuntimeConfig& configMetadata) {
                packageId = PREINSTALL_PACKAGE_ID;
                version = PREINSTALL_PACKAGE_VERSION;
                return Core::ERROR_NONE;
            }));
    
    EXPECT_CALL(*mPackageInstallerMock, GetInstalledPackages(::testing::_))
        .WillRepeatedly(::testing::Return(Core::ERROR_NONE));
    
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    // Mock directory operations to simulate preinstall directory exists
    ON_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillByDefault(::testing::Return(reinterpret_cast<DIR*>(1))); // Non-null DIR*
        
    ON_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillByDefault(::testing::Return(nullptr)); // End of directory
        
    ON_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillByDefault(::testing::Return(0));

    // Call StartPreinstall
    EXPECT_EQ(Core::ERROR_NONE, mPreinstallManagerImpl->StartPreinstall());
    
    releaseResources();
}

/**
 * @brief : Test PreinstallManager GetInstalledVersions method
 *          
 *          Mock installed packages and verify GetInstalledVersions returns correct data
 */
TEST_F(PreinstallManagerL1Test, GetInstalledVersions)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());

    // Create a package iterator mock
    class PackageIteratorMock : public Exchange::IPackageInstaller::IPackageIterator {
    public:
        virtual ~PackageIteratorMock() = default;
        
        MOCK_METHOD(bool, Next, (Exchange::IPackageInstaller::Package&), (override));
        MOCK_METHOD(void, Reset, (), (override));
        MOCK_METHOD(bool, IsValid, (), (const, override));

        BEGIN_INTERFACE_MAP(PackageIteratorMock)
        INTERFACE_ENTRY(Exchange::IPackageInstaller::IPackageIterator)
        END_INTERFACE_MAP
    };
    
    PackageIteratorMock* packageIteratorMock = new PackageIteratorMock();
    
    // Setup mock expectations
    EXPECT_CALL(*mPackageInstallerMock, GetInstalledPackages(::testing::_))
        .WillOnce(::testing::Invoke(
            [&](Exchange::IPackageInstaller::IPackageIterator*& iterator) {
                iterator = packageIteratorMock;
                return Core::ERROR_NONE;
            }));
    
    EXPECT_CALL(*packageIteratorMock, Next(::testing::_))
        .WillOnce(::testing::Invoke([&](Exchange::IPackageInstaller::Package& package) {
            package.packageId = PREINSTALL_PACKAGE_ID;
            package.version = PREINSTALL_PACKAGE_VERSION;
            package.state = Exchange::IPackageInstaller::InstallState::INSTALLED;
            return true;
        }))
        .WillOnce(::testing::Return(false));
    
    EXPECT_CALL(*packageIteratorMock, Release())
        .WillOnce(::testing::Invoke([&]() {
            delete packageIteratorMock;
            return 0;
        }));

    string jsonResult;
    EXPECT_EQ(Core::ERROR_NONE, mPreinstallManagerImpl->GetInstalledVersions(jsonResult));
    
    // Verify the result contains expected package info
    EXPECT_NE(string::npos, jsonResult.find(PREINSTALL_PACKAGE_ID));
    EXPECT_NE(string::npos, jsonResult.find(PREINSTALL_PACKAGE_VERSION));
    
    releaseResources();
}

/**
 * @brief Test JSON-RPC StartPreinstall method
 *        
 *        This test verifies the JSON-RPC interface for StartPreinstall
 */
TEST_P(PreinstallManagerL1TestWithParam, StartPreinstallJsonRpc)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());

    method = GetParam();
    parameters = "{}";
    
    // Mock the required calls for successful preinstall
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(Core::ERROR_NONE));
    
    ON_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillByDefault(::testing::Return(reinterpret_cast<DIR*>(1)));
        
    ON_CALL(*p_wrapsImplMock, readdir(::testing::_))
        .WillByDefault(::testing::Return(nullptr));
        
    ON_CALL(*p_wrapsImplMock, closedir(::testing::_))
        .WillByDefault(::testing::Return(0));

    Core::JSONRPC::Message message;
    message.Id = 1;
    message.Designator = method;
    message.Parameters = parameters;

    Core::JSONRPC::Message response;
    Core::hresult status = plugin->Invoke(connection, message, response);
    
    EXPECT_EQ(Core::ERROR_NONE, status);
    
    releaseResources();
}

/**
 * @brief Test JSON-RPC GetInstalledVersions method
 *        
 *        This test verifies the JSON-RPC interface for GetInstalledVersions  
 */
TEST_P(PreinstallManagerL1TestWithParam, GetInstalledVersionsJsonRpc)
{
    ASSERT_EQ(Core::ERROR_NONE, createResources());

    method = GetParam();
    parameters = "{}";
    
    // Mock package iterator
    class PackageIteratorMock : public Exchange::IPackageInstaller::IPackageIterator {
    public:
        MOCK_METHOD(bool, Next, (Exchange::IPackageInstaller::Package&), (override));
        MOCK_METHOD(void, Reset, (), (override));
        MOCK_METHOD(bool, IsValid, (), (const, override));
        BEGIN_INTERFACE_MAP(PackageIteratorMock) INTERFACE_ENTRY(Exchange::IPackageInstaller::IPackageIterator) END_INTERFACE_MAP
    };
    
    PackageIteratorMock* packageIteratorMock = new PackageIteratorMock();
    
    EXPECT_CALL(*mPackageInstallerMock, GetInstalledPackages(::testing::_))
        .WillOnce(::testing::Invoke([&](Exchange::IPackageInstaller::IPackageIterator*& iterator) {
            iterator = packageIteratorMock;
            return Core::ERROR_NONE;
        }));
    
    EXPECT_CALL(*packageIteratorMock, Next(::testing::_))
        .WillOnce(::testing::Return(false));
    
    EXPECT_CALL(*packageIteratorMock, Release())
        .WillOnce(::testing::Invoke([&]() {
            delete packageIteratorMock;
            return 0;
        }));

    Core::JSONRPC::Message message;
    message.Id = 1;
    message.Designator = method;
    message.Parameters = parameters;

    Core::JSONRPC::Message response;
    Core::hresult status = plugin->Invoke(connection, message, response);
    
    EXPECT_EQ(Core::ERROR_NONE, status);
    
    releaseResources();
}

// Parameterized test for JSON-RPC methods
INSTANTIATE_TEST_SUITE_P(
    JsonRpcMethods,
    PreinstallManagerL1TestWithParam,
    ::testing::Values(
        "PreinstallManager.1.startPreinstall",
        "PreinstallManager.1.getInstalledVersions"
    ));
