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
#include <mntent.h>
#include <fstream>
#include <string>
#include <vector>
#include <cstdio>
#include <mutex>
#include <chrono>
#include <condition_variable>

#include "DownloadManager.h"
#include "DownloadManagerImplementation.h"
#include "ISubSystemMock.h"
#include "ServiceMock.h"
#include "COMLinkMock.h"
#include "ThunderPortability.h"
#include "WorkerPoolImplementation.h"
#include "FactoriesImplementation.h"

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);
#define TIMEOUT   (500)

using ::testing::NiceMock;
using namespace WPEFramework;
using namespace std;

typedef enum : uint32_t {
    DownloadManager_invalidStatus = 0,
    DownloadManager_AppDownloadStatus
} DownloadManagerTest_status_t;

struct StatusParams {
    string downloadId;
    string fileLocator;
    Exchange::IDownloadManager::FailReason reason;
};

class ServiceMock : public WPEFramework::PluginHost::IShell {
public:
    ~ServiceMock() override = default;
    MOCK_METHOD(string, Versions, (), (const, override));
    MOCK_METHOD(string, Locator, (), (const, override));
    MOCK_METHOD(string, ClassName, (), (const, override));
    MOCK_METHOD(string, Callsign, (), (const, override));
    MOCK_METHOD(string, WebPrefix, (), (const, override));
    MOCK_METHOD(string, ConfigLine, (), (const, override));
    MOCK_METHOD(string, PersistentPath, (), (const, override));
    MOCK_METHOD(string, VolatilePath, (), (const, override));
    MOCK_METHOD(string, DataPath, (), (const, override));
    MOCK_METHOD(WPEFramework::PluginHost::IShell::state, State, (), (const, override));
    MOCK_METHOD(bool, Resumed, (), (const, override));
    MOCK_METHOD(bool, IsSupported, (const uint8_t), (const, override));
    MOCK_METHOD(void, EnableWebServer, (const string&, const string&), (override));
    MOCK_METHOD(void, DisableWebServer, (), (override));
    MOCK_METHOD(WPEFramework::PluginHost::ISubSystem*, SubSystems, (), (override));
    MOCK_METHOD(uint32_t, Submit, (const uint32_t, const WPEFramework::Core::ProxyType<WPEFramework::Core::JSON::IElement>&), (override));
    MOCK_METHOD(void, Notify, (const string&, const string&), (override));
    MOCK_METHOD(void*, QueryInterfaceByCallsign, (const uint32_t, const string&), (override));
    MOCK_METHOD(void, Register, (WPEFramework::PluginHost::IPlugin::INotification*), (override));
    MOCK_METHOD(void, Unregister, (WPEFramework::PluginHost::IPlugin::INotification*), (override));
    MOCK_METHOD(string, Model, (), (const, override));
    MOCK_METHOD(bool, Background, (), (const, override));
    MOCK_METHOD(string, Accessor, (), (const, override));

    BEGIN_INTERFACE_MAP(ServiceMock)
    INTERFACE_ENTRY(WPEFramework::PluginHost::IShell)
    END_INTERFACE_MAP
};

class SubSystemMock : public WPEFramework::PluginHost::ISubSystem {
public:
    ~SubSystemMock() override = default;
    MOCK_METHOD(void, Register, (WPEFramework::PluginHost::ISubSystem::INotification*), (override));
    MOCK_METHOD(void, Unregister, (WPEFramework::PluginHost::ISubSystem::INotification*), (override));
    MOCK_METHOD(string, BuildTreeHash, (), (const, override));
    MOCK_METHOD(bool, IsActive, (const WPEFramework::PluginHost::ISubSystem::subsystem), (const, override));
    MOCK_METHOD(void, Set, (const WPEFramework::PluginHost::ISubSystem::subsystem, WPEFramework::PluginHost::ISubSystem::INotification*), (override));

    BEGIN_INTERFACE_MAP(SubSystemMock)
    INTERFACE_ENTRY(WPEFramework::PluginHost::ISubSystem)
    END_INTERFACE_MAP
};

namespace WPEFramework {
    namespace PluginHost {
        class FactoriesImplementation {
        public:
            FactoriesImplementation() = default;
            ~FactoriesImplementation() = default;
        };
    }

    namespace Core {
        class WorkerPoolImplementation {
        public:
            WorkerPoolImplementation(uint8_t threads, uint32_t stackSize, uint32_t queueSize) {
            }
            ~WorkerPoolImplementation() = default;
            void Run() {}
        };
    }
}

class DownloadManagerTest : public ::testing::Test {
protected:
    // Declare the protected members
    ServiceMock* mServiceMock = nullptr;
    SubSystemMock* mSubSystemMock = nullptr;

    Core::ProxyType<Plugin::DownloadManager> plugin;
    Core::JSONRPC::Handler& mJsonRpcHandler;
    Core::JSONRPC::Message message;
    DECL_CORE_JSONRPC_CONX connection;
    string mJsonRpcResponse;
    string uri;

    PLUGINHOST_DISPATCHER *dispatcher;
    PluginHost::FactoriesImplementation factoriesImplementation;

    Core::ProxyType<Plugin::DownloadManagerImplementation> mDownloadManagerImpl;
    Core::ProxyType<Core::WorkerPoolImplementation> workerPool;

    Exchange::IDownloadManager* downloadManagerInterface = nullptr;
    Exchange::IDownloadManager::Options options;
    string downloadId;
    uint8_t progress;
    uint32_t quotaKB, usedKB;

    // Constructor
    DownloadManagerTest()
        : workerPool(Core::ProxyType<Core::WorkerPoolImplementation>::Create(
            2, Core::Thread::DefaultStackSize(), 16)),
          plugin(Core::ProxyType<Plugin::DownloadManager>::Create()),
          mJsonRpcHandler(*plugin),
          INIT_CONX(1,0)
    {
        mDownloadManagerImpl = Core::ProxyType<Plugin::DownloadManagerImplementation>::Create();

        downloadManagerInterface = static_cast<Exchange::IDownloadManager*>(mDownloadManagerImpl->QueryInterface(Exchange::IDownloadManager::ID));

        Core::IWorkerPool::Assign(&(*workerPool));
        workerPool->Run();
    }

    // Destructor
    virtual ~DownloadManagerTest() override
    {
        downloadManagerInterface->Release();

        Core::IWorkerPool::Assign(nullptr);
        workerPool.Release();
    }

    void SetUp() override 
    {        
        // Set up mocks and expect calls
        mServiceMock = new NiceMock<ServiceMock>;
        mSubSystemMock = new NiceMock<SubSystemMock>;

        EXPECT_CALL(*mServiceMock, ConfigLine())
          .Times(::testing::AnyNumber())
          .WillRepeatedly(::testing::Return("{\"downloadDir\": \"/opt/downloads/\"}"));

        EXPECT_CALL(*mServiceMock, PersistentPath())
          .Times(::testing::AnyNumber())
          .WillRepeatedly(::testing::Return("/tmp/"));

        EXPECT_CALL(*mServiceMock, VolatilePath())
          .Times(::testing::AnyNumber())
          .WillRepeatedly(::testing::Return("/tmp/"));

        EXPECT_CALL(*mServiceMock, DataPath())
          .Times(::testing::AnyNumber())
          .WillRepeatedly(::testing::Return("/tmp/"));

        EXPECT_CALL(*mServiceMock, SubSystems())
          .Times(::testing::AnyNumber())
          .WillRepeatedly(::testing::Return(mSubSystemMock));
    }

    void initforJsonRpc() 
    {    
        EXPECT_CALL(*mServiceMock, Register(::testing::_))
          .Times(::testing::AnyNumber());

        EXPECT_CALL(*mServiceMock, AddRef())
          .Times(::testing::AnyNumber());

        // Activate the dispatcher and initialize the plugin for JSON-RPC
        PluginHost::IFactories::Assign(&factoriesImplementation);
        dispatcher = static_cast<PLUGINHOST_DISPATCHER*>(plugin->QueryInterface(PLUGINHOST_DISPATCHER_ID));
        dispatcher->Activate(mServiceMock);
        plugin->Initialize(mServiceMock);  
    }

    void initforComRpc() 
    {
        EXPECT_CALL(*mServiceMock, AddRef())
          .Times(::testing::AnyNumber());

        // Initialize the plugin for COM-RPC
        downloadManagerInterface->Initialize(mServiceMock);
    }

    void getDownloadParams()
    {
        // Initialize the parameters required for COM-RPC with default values
        uri = "https://httpbin.org/bytes/1024";

        options = { 
            true, 2, 1024
        };

        downloadId = {};
    }

    void TearDown() override
    {
        // Clean up mocks
        if (mServiceMock != nullptr)
        {
            delete mServiceMock;
            mServiceMock = nullptr;
        }

        if(mSubSystemMock != nullptr)
        {
            delete mSubSystemMock;
            mSubSystemMock = nullptr;
        }
    }

    void deinitforJsonRpc() 
    {
        EXPECT_CALL(*mServiceMock, Unregister(::testing::_))
          .Times(::testing::AnyNumber());

        EXPECT_CALL(*mServiceMock, Release())
          .Times(::testing::AnyNumber());

        // Deactivate the dispatcher and deinitialize the plugin for JSON-RPC
        dispatcher->Deactivate();
        dispatcher->Release();

        plugin->Deinitialize(mServiceMock);
    }

    void deinitforComRpc()
    {
        EXPECT_CALL(*mServiceMock, Release())
          .Times(::testing::AnyNumber());

        // Deinitialize the plugin for COM-RPC
        downloadManagerInterface->Deinitialize(mServiceMock);
    }

    void waitforSignal(uint32_t timeout_ms) 
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
    }
};

class NotificationTest : public Exchange::IDownloadManager::INotification
{
    public:
        /** @brief Mutex */
        std::mutex m_mutex;

        /** @brief Condition variable */
        std::condition_variable m_condition_variable;

        /** @brief Status signal flag */
        uint32_t m_status_signal = DownloadManager_invalidStatus;

        StatusParams m_status_param;

        NotificationTest()
        {
        }
        
        virtual ~NotificationTest() override = default;

        // Required for reference counting
        virtual uint32_t AddRef() const override { return 1; }
        virtual uint32_t Release() const override { return 1; }

    private:
        BEGIN_INTERFACE_MAP(NotificationTest)
        INTERFACE_ENTRY(Exchange::IDownloadManager::INotification)
        END_INTERFACE_MAP

        void SetStatusParams(const StatusParams& statusParam)
        {
            m_status_param = statusParam;
        }

        void OnAppDownloadStatus(const string& downloadStatus) override {
            m_status_signal = DownloadManager_AppDownloadStatus;
            
            std::unique_lock<std::mutex> lock(m_mutex);
            
            JsonArray list;
            list.FromString(downloadStatus);
            
            if (list.Length() > 0) {
                JsonObject obj = list[0].Object();
                m_status_param.downloadId = obj["downloadId"].String();
                m_status_param.fileLocator = obj["fileLocator"].String();
                
                if (obj.HasLabel("failReason")) {
                    string reason = obj["failReason"].String();
                    if (reason == "DOWNLOAD_FAILURE") {
                        m_status_param.reason = Exchange::IDownloadManager::FailReason::DOWNLOAD_FAILURE;
                    } else if (reason == "DISK_PERSISTENCE_FAILURE") {
                        m_status_param.reason = Exchange::IDownloadManager::FailReason::DISK_PERSISTENCE_FAILURE;
                    }
                }
            }
            
            EXPECT_EQ(m_status_param.downloadId, m_status_param.downloadId);

            m_condition_variable.notify_one();
        }

        uint32_t WaitForStatusSignal(uint32_t timeout_ms, DownloadManagerTest_status_t status)
        {
            uint32_t status_signal = DownloadManager_invalidStatus;
            std::unique_lock<std::mutex> lock(m_mutex);
            auto now = std::chrono::steady_clock::now();
            auto timeout = std::chrono::milliseconds(timeout_ms);
            if (m_condition_variable.wait_until(lock, now + timeout) == std::cv_status::timeout)
            {
                 TEST_LOG("Timeout waiting for request status event");
                 return m_status_signal;
            }
            status_signal = m_status_signal;
            m_status_signal = DownloadManager_invalidStatus;
            return status_signal;
        }
    };

/* Test Case for verifying registered methods using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Check if the methods listed exist by using the Exists() from the JSON RPC handler
 * Verify the methods exist by asserting that Exists() returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, registeredMethodsusingJsonRpc) {

    initforJsonRpc();

    // TC-1: Check if the listed methods exist using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("download")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("pause")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("resume")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("cancel")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("delete")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("progress")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("getStorageDetails")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("rateLimit")));

    deinitforJsonRpc();
}

/* Test Case for adding download request to a regular queue using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, notifications/events, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters 
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, downloadMethodusingJsonRpcSuccess) {

    initforJsonRpc();

    Core::Event onAppDownloadStatus(false, true);

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_CALL(*mServiceMock, Submit(::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
                onAppDownloadStatus.SetEvent();
                return Core::ERROR_NONE;
            }));

    EVENT_SUBSCRIBE(0, _T("onAppDownloadStatus"), _T("org.rdk.DownloadManager"), message);

    // TC-2: Add download request to regular queue using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"url\": \"https://httpbin.org/bytes/1024\"}"), mJsonRpcResponse));

    EXPECT_EQ(Core::ERROR_NONE, onAppDownloadStatus.Lock());
    EVENT_UNSUBSCRIBE(0, _T("onAppDownloadStatus"), _T("org.rdk.DownloadManager"), message);

    EXPECT_TRUE(mJsonRpcResponse.find("downloadId") != std::string::npos);

    deinitforJsonRpc();
}

/* Test Case for checking download request error when internet is unavailable using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the method using the JSON RPC handler, passing the required parameters
 * Verify download method error due to unavailability of internet by asserting that it returns Core::ERROR_UNAVAILABLE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, downloadMethodusingJsonRpcError) {

    initforJsonRpc();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return false;
            }));

    // TC-3: Download request error when internet is unavailable using JsonRpc
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"url\": \"https://httpbin.org/bytes/1024\"}"), mJsonRpcResponse));

    deinitforJsonRpc();
}

/* Test Case for adding download request to a priority queue using ComRpc
 * 
 * Set up and initialize required COM-RPC resources, configurations, notifications/events, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters, setting priority as true and wait
 * Verify successful download request by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, downloadMethodsusingComRpcSuccess) {

    initforComRpc();

    getDownloadParams();

    uri = "https://httpbin.org/bytes/1024";

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type){
                return true;
            }));

    // TC-4: Add download request to priority queue using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Download(uri, options, downloadId));

    waitforSignal(TIMEOUT);

    EXPECT_FALSE(downloadId.empty());

    deinitforComRpc();
}

/* Test Case for checking download request error when internet is unavailable using ComRpc
 * 
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters
 * Verify download method error due to unavailability of internet by asserting that it returns Core::ERROR_UNAVAILABLE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, downloadMethodsusingComRpcError) {

    initforComRpc();

    getDownloadParams();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type){
                return false;
            }));

    // TC-5: Download request error when internet is unavailable using ComRpc
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, downloadManagerInterface->Download(uri, options, downloadId));

    deinitforComRpc();   
}

/* Test Case for pausing download via ID using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters and wait
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the pause method using the JSON RPC handler, passing the downloadId
 * Verify that the pause method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Invoke the cancel method using the JSON RPC handler, passing the downloadId for cancelling download
 * Verify that the cancel method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, pauseMethodusingJsonRpcSuccess) {

    initforJsonRpc();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"url\": \"https://www.examplefile.com/file-download/328\"}"), mJsonRpcResponse));

    waitforSignal(200);

    JsonObject jsonResponse;
    EXPECT_TRUE(jsonResponse.FromString(mJsonRpcResponse));
    string currentDownloadId = jsonResponse["downloadId"].String();

    // TC-6: Pause download via downloadId using JsonRpc
    string pauseParams = "{\"downloadId\": \"" + currentDownloadId + "\"}";
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("pause"), pauseParams, mJsonRpcResponse));

    string cancelParams = "{\"downloadId\": \"" + currentDownloadId + "\"}";
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("cancel"), cancelParams, mJsonRpcResponse));

    deinitforJsonRpc();
}

/* Test Case for pausing failed using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the pause method using the JSON RPC handler, passing downloadId
 * Verify pause method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, pauseMethodusingJsonRpcFailure) {

    initforJsonRpc();

    // TC-7: Failure in pausing download using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("pause"), _T("{\"downloadId\": \"invalid_id\"}"), mJsonRpcResponse));

    deinitforJsonRpc();
}

/* Test Case for pausing download via ID using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, notifications/events, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters and wait
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the pause method using the COM RPC interface, passing the downloadId
 * Verify successful pause by asserting that it returns Core::ERROR_NONE
 * Call the cancel method using the COM RPC interface, passing the downloadId for cancelling download
 * Verify successful cancel by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, pauseMethodusingComRpcSuccess) {

    initforComRpc();

    getDownloadParams();

    uint32_t timeout_ms = 300;

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Download(uri, options, downloadId));

    waitforSignal(timeout_ms);

    EXPECT_FALSE(downloadId.empty());

    // TC-8: Pause download via downloadId using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Pause(downloadId));

    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Cancel(downloadId));

    deinitforComRpc();
}

/* Test Case for pausing failed using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the pause method using the COM RPC interface, passing downloadId
 * Verify pause method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, pauseMethodusingComRpcFailure) {

    initforComRpc();

    // TC-9: Failure in pausing download using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, downloadManagerInterface->Pause("invalid_id"));

    deinitforComRpc();
}

/* Test Case for resuming download via ID using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters and wait
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the pause method using the JSON RPC handler, passing the downloadId
 * Verify that the pause method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Invoke the resume method using the JSON RPC handler, passing the downloadId
 * Verify that the resume method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Invoke the cancel method using the JSON RPC handler, passing the downloadId for cancelling download
 * Verify that the cancel method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, resumeMethodusingJsonRpcSuccess) {

    initforJsonRpc();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"url\": \"https://www.examplefile.com/file-download/328\"}"), mJsonRpcResponse));

    waitforSignal(200);

    JsonObject jsonResponse;
    EXPECT_TRUE(jsonResponse.FromString(mJsonRpcResponse));
    string currentDownloadId = jsonResponse["downloadId"].String();

    // Pause download
    string pauseParams = "{\"downloadId\": \"" + currentDownloadId + "\"}";
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("pause"), pauseParams, mJsonRpcResponse));

    // TC-10: Resume download via downloadId using JsonRpc
    string resumeParams = "{\"downloadId\": \"" + currentDownloadId + "\"}";
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("resume"), resumeParams, mJsonRpcResponse));

    string cancelParams = "{\"downloadId\": \"" + currentDownloadId + "\"}";
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("cancel"), cancelParams, mJsonRpcResponse));

    deinitforJsonRpc();
}

/* Test Case for resuming failed using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the resume method using the JSON RPC handler, passing downloadId
 * Verify resume method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, resumeMethodusingJsonRpcFailure) {

    initforJsonRpc();

    // TC-11: Failure in resuming download using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("resume"), _T("{\"downloadId\": \"invalid_id\"}"), mJsonRpcResponse));

    deinitforJsonRpc();
}

/* Test Case for resuming download via ID using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, notifications/events, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters and wait
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the pause method using the COM RPC interface, passing the downloadId
 * Verify successful pause by asserting that it returns Core::ERROR_NONE
 * Call the resume method using the COM RPC interface, passing the downloadId
 * Verify successful resume by asserting that it returns Core::ERROR_NONE
 * Call the cancel method using the COM RPC interface, passing the downloadId for cancelling download
 * Verify successful cancel by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, resumeMethodusingComRpcSuccess) {

    initforComRpc();

    getDownloadParams();

    uint32_t timeout_ms = 300;

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Download(uri, options, downloadId));

    waitforSignal(timeout_ms);

    EXPECT_FALSE(downloadId.empty());

    // Pause download
    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Pause(downloadId));

    // TC-12: Resume download via downloadId using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Resume(downloadId));

    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Cancel(downloadId));

    deinitforComRpc();
}

/* Test Case for resuming failed using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the resume method using the COM RPC interface, passing downloadId
 * Verify resume method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, resumeMethodusingComRpcFailure) {

    initforComRpc();

    // TC-13: Failure in resuming download using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, downloadManagerInterface->Resume("invalid_id"));

    deinitforComRpc();
}

/* Test Case for cancelling download via ID using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters and wait
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the pause method using the JSON RPC handler, passing the downloadId
 * Verify that the pause method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Invoke the cancel method using the JSON RPC handler, passing the downloadId
 * Verify that the cancel method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, cancelMethodusingJsonRpcSuccess) {

    initforJsonRpc();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"url\": \"https://www.examplefile.com/file-download/328\"}"), mJsonRpcResponse));

    waitforSignal(200);

    JsonObject jsonResponse;
    EXPECT_TRUE(jsonResponse.FromString(mJsonRpcResponse));
    string currentDownloadId = jsonResponse["downloadId"].String();

    // Pause download
    string pauseParams = "{\"downloadId\": \"" + currentDownloadId + "\"}";
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("pause"), pauseParams, mJsonRpcResponse));

    // TC-14: Cancel download via downloadId using JsonRpc
    string cancelParams = "{\"downloadId\": \"" + currentDownloadId + "\"}";
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("cancel"), cancelParams, mJsonRpcResponse));

    deinitforJsonRpc();
}

/* Test Case for cancelling failed using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the cancel method using the JSON RPC handler, passing downloadId
 * Verify cancel method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, cancelMethodusingJsonRpcFailure) {

    initforJsonRpc();

    // TC-15: Failure in cancelling download using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("cancel"), _T("{\"downloadId\": \"invalid_id\"}"), mJsonRpcResponse));

    deinitforJsonRpc();
}

/* Test Case for cancelling download via ID using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters and wait
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the pause method using the COM RPC interface, passing the downloadId
 * Verify successful pause by asserting that it returns Core::ERROR_NONE
 * Call the cancel method using the COM RPC interface, passing the downloadId
 * Verify successful cancel by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, cancelMethodusingComRpcSuccess) {

    initforComRpc();

    getDownloadParams();

    uint32_t timeout_ms = 300;

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Download(uri, options, downloadId));

    waitforSignal(timeout_ms);

    EXPECT_FALSE(downloadId.empty());

    // Pause download
    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Pause(downloadId));

    // TC-16: Cancel download via downloadId using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Cancel(downloadId));

    deinitforComRpc();
}

/* Test Case for cancelling failed using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the cancel method using the COM RPC interface, passing downloadId
 * Verify cancel method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, cancelMethodusingComRpcFailure) {

    initforComRpc();

    // TC-17: Failure in cancelling download using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, downloadManagerInterface->Cancel("invalid_id"));

    deinitforComRpc();
}

/* Test Case for delete download using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, notifications/events, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the delete method using the JSON RPC handler, passing the fileLocator
 * Verify successful delete by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, deleteMethodusingJsonRpcSuccess) {

    initforJsonRpc();

    NotificationTest notification;

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    // Register notification to get download status
    downloadManagerInterface->Register(&notification);

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"url\": \"https://httpbin.org/bytes/1024\"}"), mJsonRpcResponse));

    waitforSignal(TIMEOUT);

    // Wait for download completion
    notification.WaitForStatusSignal(TIMEOUT, DownloadManager_AppDownloadStatus);
    string fileLocator = notification.m_status_param.fileLocator;

    // TC-18: Delete file using JsonRpc
    string deleteParams = "{\"fileLocator\": \"" + fileLocator + "\"}";
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("delete"), deleteParams, mJsonRpcResponse));

    downloadManagerInterface->Unregister(&notification);

    deinitforJsonRpc();
}

/* Test Case for delete failed using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the delete method using the JSON RPC handler, passing fileLocator
 * Verify delete method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, deleteMethodusingJsonRpcFailure) {

    initforJsonRpc();

    // TC-19: Failure in deleting download using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("delete"), _T("{\"fileLocator\": \"/invalid/path/file.zip\"}"), mJsonRpcResponse));

    deinitforJsonRpc();
}

/* Test Case for delete download using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, notifications/events, mocks and expectations
 * Call the Download method using the COM RPC interface along with the required parameters
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the delete method using the COM RPC interface, passing the fileLocator
 * Verify successful delete by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, deleteMethodusingComRpcSuccess) {

    initforComRpc();

    getDownloadParams();

    NotificationTest notification;

    uri = "https://httpbin.org/bytes/1024";

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type){
                return true;
            }));

    // Register notification to get download status
    downloadManagerInterface->Register(&notification);

    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Download(uri, options, downloadId));

    waitforSignal(TIMEOUT);

    // Wait for download completion
    notification.WaitForStatusSignal(TIMEOUT, DownloadManager_AppDownloadStatus);
    string fileLocator = notification.m_status_param.fileLocator;
    EXPECT_FALSE(fileLocator.empty());

    // TC-20: Delete file using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Delete(fileLocator));

    downloadManagerInterface->Unregister(&notification);

    deinitforComRpc();
}

/* Test Case for delete failed using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the delete method using the COM RPC interface, passing fileLocator
 * Verify delete method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, deleteMethodusingComRpcFailure) {

    initforComRpc();

    // TC-21: Failure in deleting download using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, downloadManagerInterface->Delete("/invalid/path/file.zip"));

    deinitforComRpc();
}

/* Test Case for progress retrieval using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the progress method using the JSON RPC handler, passing the downloadId
 * Verify successful progress retrieval by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, progressMethodusingJsonRpcSuccess) {

    initforJsonRpc();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"url\": \"https://www.examplefile.com/file-download/328\"}"), mJsonRpcResponse));

    waitforSignal(200);

    JsonObject jsonResponse;
    EXPECT_TRUE(jsonResponse.FromString(mJsonRpcResponse));
    string currentDownloadId = jsonResponse["downloadId"].String();

    // TC-22: Get progress using JsonRpc
    string progressParams = "{\"downloadId\": \"" + currentDownloadId + "\"}";
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("progress"), progressParams, mJsonRpcResponse));
    
    JsonObject progressResponse;
    EXPECT_TRUE(progressResponse.FromString(mJsonRpcResponse));
    EXPECT_TRUE(progressResponse.HasLabel("percent"));

    // Cancel download for cleanup
    string cancelParams = "{\"downloadId\": \"" + currentDownloadId + "\"}";
    mJsonRpcHandler.Invoke(connection, _T("cancel"), cancelParams, mJsonRpcResponse);

    deinitforJsonRpc();
}

/* Test Case for progress retrieval failed using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the progress method using the JSON RPC handler, passing invalid downloadId
 * Verify progress method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, progressMethodusingJsonRpcFailure) {

    initforJsonRpc();

    // TC-23: Failure in getting progress using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("progress"), _T("{\"downloadId\": \"invalid_id\"}"), mJsonRpcResponse));

    deinitforJsonRpc();
}

/* Test Case for progress retrieval using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the Download method using the COM RPC interface along with the required parameters
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the progress method using the COM RPC interface, passing the downloadId
 * Verify successful progress retrieval by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, progressMethodusingComRpcSuccess) {

    initforComRpc();

    getDownloadParams();

    uri = "https://www.examplefile.com/file-download/328";

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Download(uri, options, downloadId));

    waitforSignal(TIMEOUT);

    EXPECT_FALSE(downloadId.empty());

    // TC-24: Get progress using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Progress(downloadId, progress));
    EXPECT_GE(progress, 0);
    EXPECT_LE(progress, 100);

    // Cancel download for cleanup
    downloadManagerInterface->Cancel(downloadId);

    deinitforComRpc();
}

/* Test Case for progress retrieval failed using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the progress method using the COM RPC interface, passing invalid downloadId
 * Verify progress method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, progressMethodusingComRpcFailure) {

    initforComRpc();

    // TC-25: Failure in getting progress using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, downloadManagerInterface->Progress("invalid_id", progress));

    deinitforComRpc();
}

/* Test Case for storage details retrieval using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the getStorageDetails method using the JSON RPC handler
 * Verify successful storage details retrieval by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, getStorageDetailsMethodusingJsonRpcSuccess) {

    initforJsonRpc();

    // TC-26: Get storage details using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("getStorageDetails"), _T("{}"), mJsonRpcResponse));
    
    JsonObject storageResponse;
    EXPECT_TRUE(storageResponse.FromString(mJsonRpcResponse));
    EXPECT_TRUE(storageResponse.HasLabel("quotaKb"));
    EXPECT_TRUE(storageResponse.HasLabel("usedKb"));

    deinitforJsonRpc();
}

/* Test Case for storage details retrieval using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the GetStorageDetails method using the COM RPC interface
 * Verify successful storage details retrieval by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, getStorageDetailsMethodusingComRpcSuccess) {

    initforComRpc();

    // TC-27: Get storage details using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->GetStorageDetails(quotaKB, usedKB));

    deinitforComRpc();
}

/* Test Case for rate limit setting using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the rateLimit method using the JSON RPC handler, passing the downloadId and limit
 * Verify successful rate limit setting by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, rateLimitMethodusingJsonRpcSuccess) {

    initforJsonRpc();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"url\": \"https://www.examplefile.com/file-download/328\"}"), mJsonRpcResponse));

    waitforSignal(200);

    JsonObject jsonResponse;
    EXPECT_TRUE(jsonResponse.FromString(mJsonRpcResponse));
    string currentDownloadId = jsonResponse["downloadId"].String();

    // TC-28: Set rate limit using JsonRpc
    string rateLimitParams = "{\"downloadId\": \"" + currentDownloadId + "\", \"limit\": 50}";
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("rateLimit"), rateLimitParams, mJsonRpcResponse));

    // Cancel download for cleanup
    string cancelParams = "{\"downloadId\": \"" + currentDownloadId + "\"}";
    mJsonRpcHandler.Invoke(connection, _T("cancel"), cancelParams, mJsonRpcResponse);

    deinitforJsonRpc();
}

/* Test Case for rate limit setting failed using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the rateLimit method using the JSON RPC handler, passing invalid downloadId
 * Verify rate limit method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, rateLimitMethodusingJsonRpcFailure) {

    initforJsonRpc();

    // TC-29: Failure in setting rate limit using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("rateLimit"), _T("{\"downloadId\": \"invalid_id\", \"limit\": 50}"), mJsonRpcResponse));

    deinitforJsonRpc();
}

/* Test Case for rate limit setting using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the Download method using the COM RPC interface along with the required parameters
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the RateLimit method using the COM RPC interface, passing the downloadId and limit
 * Verify successful rate limit setting by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, rateLimitMethodusingComRpcSuccess) {

    initforComRpc();

    getDownloadParams();

    uri = "https://www.examplefile.com/file-download/328";

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Download(uri, options, downloadId));

    waitforSignal(TIMEOUT);

    EXPECT_FALSE(downloadId.empty());

    // TC-30: Set rate limit using ComRpc
    uint32_t limit = 50;
    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->RateLimit(downloadId, limit));

    // Cancel download for cleanup
    downloadManagerInterface->Cancel(downloadId);

    deinitforComRpc();
}

/* Test Case for rate limit setting failed using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the RateLimit method using the COM RPC interface, passing invalid downloadId
 * Verify rate limit method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, rateLimitMethodusingComRpcFailure) {

    initforComRpc();

    // TC-31: Failure in setting rate limit using ComRpc
    uint32_t limit = 50;
    EXPECT_EQ(Core::ERROR_GENERAL, downloadManagerInterface->RateLimit("invalid_id", limit));

    deinitforComRpc();
}

/* Test Case for notification registration and unregistration
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Register notification handler and verify successful registration
 * Unregister notification handler and verify successful unregistration
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, notificationRegistrationSuccess) {

    initforComRpc();

    NotificationTest notification;
    
    // TC-32: Register notification
    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Register(&notification));
    
    // TC-33: Unregister notification
    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Unregister(&notification));

    deinitforComRpc();
}

/* Test Case for notification unregistration failure
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Try to unregister a notification that was never registered
 * Verify unregistration failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, notificationUnregistrationFailure) {

    initforComRpc();

    NotificationTest notification;
    
    // TC-34: Try to unregister notification that was never registered
    EXPECT_EQ(Core::ERROR_GENERAL, downloadManagerInterface->Unregister(&notification));

    deinitforComRpc();
}
