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
#include "FactoriesImplementation.h"

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);

#define TIMEOUT   (50000)
#define PREINSTALL_FORCE_INSTALL_TRUE   true
#define PREINSTALL_FORCE_INSTALL_FALSE  false
#define PREINSTALL_JSON_RESPONSE        "{\"appId\":\"com.test.app\",\"status\":\"installed\"}"

typedef enum : uint32_t {
    PreinstallManager_StateInvalid = 0x00000000,
    PreinstallManager_onAppInstallationStatus = 0x00000001
} PreinstallManagerL1test_async_events_t;

using ::testing::NiceMock;
using namespace WPEFramework;

namespace {
const string callSign = _T("PreinstallManager");
}

namespace WPEFramework {
namespace Plugin {
class PreinstallManagerTest : public ::testing::Test {
    friend class PreinstallManagerImplementation;
};
} // namespace Plugin
} // namespace WPEFramework

class NotificationHandler : public Exchange::IPreinstallManager::INotification 
{
private:
    BEGIN_INTERFACE_MAP(NotificationHandler)
    INTERFACE_ENTRY(Exchange::IPreinstallManager::INotification)
    END_INTERFACE_MAP

public:
    NotificationHandler() = default;
    virtual ~NotificationHandler() = default;

    void OnAppInstallationStatus(const string& jsonresponse) override 
    {
        TEST_LOG("OnAppInstallationStatus received: %s", jsonresponse.c_str());
        mJsonResponse = jsonresponse;
        mEventSignal = PreinstallManager_onAppInstallationStatus;
        mConditionVariable.notify_one();
    }

    uint32_t WaitForEvent(uint32_t timeout_ms, PreinstallManagerL1test_async_events_t expectedEvent)
    {
        std::unique_lock<std::mutex> lock(mMutex);
        auto now = std::chrono::steady_clock::now();
        auto timeout = std::chrono::milliseconds(timeout_ms);
        if (mConditionVariable.wait_until(lock, now + timeout) == std::cv_status::timeout)
        {
            TEST_LOG("Timeout waiting for event");
            return PreinstallManager_StateInvalid;
        }
        uint32_t event = mEventSignal;
        mEventSignal = PreinstallManager_StateInvalid;
        return event;
    }

    string getJsonResponse() const { return mJsonResponse; }

private:
    std::mutex mMutex;
    std::condition_variable mConditionVariable;
    uint32_t mEventSignal = PreinstallManager_StateInvalid;
    string mJsonResponse;
};

class PreinstallManagerTest : public ::testing::Test {
protected:
    ServiceMock* mServiceMock = nullptr;
    PackageManagerMock* mPackageManagerMock = nullptr;
    PackageInstallerMock* mPackageInstallerMock = nullptr;
    Core::JSONRPC::Message message;
    FactoriesImplementation factoriesImplementation;
    PLUGINHOST_DISPATCHER *dispatcher;

    Core::ProxyType<Plugin::PreinstallManager> plugin;
    Plugin::PreinstallManagerImplementation *mPreinstallManagerImpl;
    Exchange::IPreinstallManager* mPreinstallManagerInterface = nullptr;
    Exchange::IConfiguration* mPreinstallManagerConfigure = nullptr;

    Core::ProxyType<WorkerPoolImplementation> workerPool;
    Core::JSONRPC::Handler& mJsonRpcHandler;
    DECL_CORE_JSONRPC_CONX connection;
    string mJsonRpcResponse;
    
    Core::Sink<NotificationHandler> mNotificationHandler;

    Core::hresult createResources()
    {
        Core::hresult status = Core::ERROR_GENERAL;
        mServiceMock = new NiceMock<ServiceMock>;
        mPackageManagerMock = new NiceMock<PackageManagerMock>;
        mPackageInstallerMock = new NiceMock<PackageInstallerMock>;

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
                    if (id == Exchange::IPackageHandler::ID) {
                        return reinterpret_cast<void*>(mPackageManagerMock);
                    }
                    else if (id == Exchange::IPackageInstaller::ID){
                        return reinterpret_cast<void*>(mPackageInstallerMock);
                    }
                }
            return nullptr;
        }));

        EXPECT_EQ(string(""), plugin->Initialize(mServiceMock));
        mPreinstallManagerImpl = Plugin::PreinstallManagerImplementation::getInstance();
        
        // Get COM-RPC interface
        mPreinstallManagerInterface = static_cast<Exchange::IPreinstallManager*>(
            mPreinstallManagerImpl->QueryInterface(Exchange::IPreinstallManager::ID));
        ASSERT_NE(mPreinstallManagerInterface, nullptr);
        
        // Get configuration interface
        mPreinstallManagerConfigure = static_cast<Exchange::IConfiguration*>(
            mPreinstallManagerImpl->QueryInterface(Exchange::IConfiguration::ID));
        ASSERT_NE(mPreinstallManagerConfigure, nullptr);
        
        TEST_LOG("createResources - All done!");
        status = Core::ERROR_NONE;

        return status;
    }

    void releaseResources()
    {
        TEST_LOG("In releaseResources!");

        if (mPackageManagerMock != nullptr)
        {
            EXPECT_CALL(*mPackageManagerMock, Release())
                .WillOnce(::testing::Invoke(
                [&]() {
                     delete mPackageManagerMock;
                     return 0;
                    }));
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

        if (mPreinstallManagerInterface != nullptr)
        {
            mPreinstallManagerInterface->Release();
            mPreinstallManagerInterface = nullptr;
        }
        
        if (mPreinstallManagerConfigure != nullptr)
        {
            mPreinstallManagerConfigure->Release();
            mPreinstallManagerConfigure = nullptr;
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
        INIT_CONX(1, 0),
        mNotificationHandler(*this)
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

// Test cases for COM-RPC interface
TEST_F(PreinstallManagerTest, RegisterNotification)
{
    ASSERT_EQ(createResources(), Core::ERROR_NONE);

    // Test Register notification
    Core::hresult result = mPreinstallManagerInterface->Register(&mNotificationHandler);
    EXPECT_EQ(result, Core::ERROR_NONE);

    // Test Unregister notification
    result = mPreinstallManagerInterface->Unregister(&mNotificationHandler);
    EXPECT_EQ(result, Core::ERROR_NONE);

    releaseResources();
}

TEST_F(PreinstallManagerTest, StartPreinstallForceTrue)
{
    ASSERT_EQ(createResources(), Core::ERROR_NONE);

    // Setup PackageInstaller mock expectations
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));

    // Test StartPreinstall with forceInstall = true
    Core::hresult result = mPreinstallManagerInterface->StartPreinstall(PREINSTALL_FORCE_INSTALL_TRUE);
    EXPECT_EQ(result, Core::ERROR_NONE);

    releaseResources();
}

TEST_F(PreinstallManagerTest, StartPreinstallForceFalse)
{
    ASSERT_EQ(createResources(), Core::ERROR_NONE);

    // Setup PackageInstaller mock expectations
    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));

    // Test StartPreinstall with forceInstall = false
    Core::hresult result = mPreinstallManagerInterface->StartPreinstall(PREINSTALL_FORCE_INSTALL_FALSE);
    EXPECT_EQ(result, Core::ERROR_NONE);

    releaseResources();
}

TEST_F(PreinstallManagerTest, OnAppInstallationStatusNotification)
{
    ASSERT_EQ(createResources(), Core::ERROR_NONE);

    // Register notification handler
    Core::hresult result = mPreinstallManagerInterface->Register(&mNotificationHandler);
    EXPECT_EQ(result, Core::ERROR_NONE);

    // Simulate notification from PackageManager
    mPreinstallManagerImpl->handleOnAppInstallationStatus(PREINSTALL_JSON_RESPONSE);

    // Wait for notification event
    uint32_t eventReceived = mNotificationHandler.WaitForEvent(TIMEOUT, PreinstallManager_onAppInstallationStatus);
    EXPECT_EQ(eventReceived, PreinstallManager_onAppInstallationStatus);
    EXPECT_EQ(mNotificationHandler.getJsonResponse(), PREINSTALL_JSON_RESPONSE);

    // Unregister notification
    result = mPreinstallManagerInterface->Unregister(&mNotificationHandler);
    EXPECT_EQ(result, Core::ERROR_NONE);

    releaseResources();
}

// Test cases for JSON-RPC interface
TEST_F(PreinstallManagerTest, JsonRpc_StartPreinstall)
{
    ASSERT_EQ(createResources(), Core::ERROR_NONE);

    // Setup JSON-RPC request
    JsonObject params;
    params["forceInstall"] = PREINSTALL_FORCE_INSTALL_TRUE;

    // Make JSON-RPC call
    string response;
    Core::hresult result = mJsonRpcHandler.Invoke(connection, _T("startPreinstall"), params.ToString(), response);
    
    // Note: The actual result depends on the JSON-RPC implementation
    // This test verifies the method exists and can be called
    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_UNAVAILABLE);

    releaseResources();
}

TEST_F(PreinstallManagerTest, ConfigurationInterface)
{
    ASSERT_EQ(createResources(), Core::ERROR_NONE);

    // Test Configure method
    uint32_t result = mPreinstallManagerConfigure->Configure(mServiceMock);
    EXPECT_EQ(result, Core::ERROR_NONE);

    releaseResources();
}

TEST_F(PreinstallManagerTest, MultipleNotificationRegistration)
{
    ASSERT_EQ(createResources(), Core::ERROR_NONE);

    NotificationHandler handler1, handler2;

    // Register multiple notification handlers
    Core::hresult result1 = mPreinstallManagerInterface->Register(&handler1);
    EXPECT_EQ(result1, Core::ERROR_NONE);

    Core::hresult result2 = mPreinstallManagerInterface->Register(&handler2);
    EXPECT_EQ(result2, Core::ERROR_NONE);

    // Unregister handlers
    result1 = mPreinstallManagerInterface->Unregister(&handler1);
    EXPECT_EQ(result1, Core::ERROR_NONE);

    result2 = mPreinstallManagerInterface->Unregister(&handler2);
    EXPECT_EQ(result2, Core::ERROR_NONE);

    releaseResources();
}
