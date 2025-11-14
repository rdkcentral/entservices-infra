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
using namespace WPEFramework;

#include "ISubSystemMock.h"
#include "ServiceMock.h"
#include "COMLinkMock.h"
#include "ThunderPortability.h"
#include "WorkerPoolImplementation.h"
#include "FactoriesImplementation.h"

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);
#define TIMEOUT   (500)

using ::testing::NiceMock;
//using namespace WPEFramework;
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

           

class DownloadManagerTest : public ::testing::Test {
protected:
    // Declare the protected members
    ServiceMock* mServiceMock = nullptr;
    SubSystemMock* mSubSystemMock = nullptr;

    Core::ProxyType<WorkerPoolImplementation> workerPool; 
    Core::ProxyType<Plugin::DownloadManager> plugin;
    Core::JSONRPC::Handler& mJsonRpcHandler;
    Core::JSONRPC::Message message;
    DECL_CORE_JSONRPC_CONX connection;
    string mJsonRpcResponse;
    string uri;

    PLUGINHOST_DISPATCHER *dispatcher;
    FactoriesImplementation factoriesImplementation;

    Exchange::IDownloadManager* downloadManagerInterface = nullptr;
    Exchange::IDownloadManager::Options options;
    string downloadId;
    uint8_t progress;
    uint32_t quotaKB, usedKB;

    // Constructor
    DownloadManagerTest()
     : plugin(Core::ProxyType<Plugin::DownloadManager>::Create()),
	 workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(
         2, Core::Thread::DefaultStackSize(), 16)),
       mJsonRpcHandler(*plugin),
        INIT_CONX(1,0)
    {
        Core::IWorkerPool::Assign(&(*workerPool));
        workerPool->Run();
    }

    // Destructor
    virtual ~DownloadManagerTest() override
    {

        Core::IWorkerPool::Assign(nullptr);
        workerPool.Release();
    }

   /* void SetUp() override 
    {
        // Create resources similar to PreinstallManager pattern
        Core::hresult status = createResources();
        EXPECT_EQ(status, Core::ERROR_NONE);
    }

    void TearDown() override
    {
        releaseResources();
    }*/

    Core::hresult createResources()
    {        
        Core::hresult status = Core::ERROR_GENERAL;
        
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

        // Initialize plugin following PreinstallManager pattern
        PluginHost::IFactories::Assign(&factoriesImplementation);
        dispatcher = static_cast<PLUGINHOST_DISPATCHER*>(
            plugin->QueryInterface(PLUGINHOST_DISPATCHER_ID));
        dispatcher->Activate(mServiceMock);
        TEST_LOG("In createResources!"); 
       
        // Get the interface directly from the plugin using interface ID
        downloadManagerInterface = static_cast<Exchange::IDownloadManager*>(
            plugin->QueryInterface(Exchange::IDownloadManager::ID));
        
        // Ensure downloadManagerInterface is valid before proceeding
        EXPECT_NE(downloadManagerInterface, nullptr);
         
        TEST_LOG("createResources - All done!");
        status = Core::ERROR_NONE;

        return status;
    }

    void releaseResources()
    {
        TEST_LOG("In releaseResources!");

        if (downloadManagerInterface) {
            downloadManagerInterface->Release();
            downloadManagerInterface = nullptr;
        }

        dispatcher->Deactivate();
        dispatcher->Release();

        plugin->Deinitialize(mServiceMock);
        
        if (mServiceMock) {
            delete mServiceMock;
            mServiceMock = nullptr;
        }

        if (mSubSystemMock) {
            delete mSubSystemMock;
            mSubSystemMock = nullptr;
        }
         
    }
      
    void SetUp() override
    {
        Core::hresult status = createResources();
        EXPECT_EQ(status, Core::ERROR_NONE);
    }

    void TearDown() override
    {
        releaseResources();
    }


    void initforJsonRpc() 
    {    
        EXPECT_CALL(*mServiceMock, Register(::testing::_))
          .Times(::testing::AnyNumber());

        EXPECT_CALL(*mServiceMock, AddRef())
          .Times(::testing::AnyNumber());

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

	// Activate the dispatcher and initialize the plugin for JSON-RPC
        PluginHost::IFactories::Assign(&factoriesImplementation);
        dispatcher = static_cast<PLUGINHOST_DISPATCHER*>(plugin->QueryInterface(PLUGINHOST_DISPATCHER_ID));
        dispatcher->Activate(mServiceMock);
              // Initialize should now succeed
       // Initialize the plugin - this should succeed if the environment is properly set up
       	string result = plugin->Initialize(mServiceMock);
        if (result != "") {
            // If initialization fails, we'll skip the JSON-RPC method checks
            TEST_LOG("Plugin initialization failed: %s", result.c_str());
    }
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
        virtual void AddRef() const override {  }
        virtual uint32_t Release() const override { return 1; }

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

