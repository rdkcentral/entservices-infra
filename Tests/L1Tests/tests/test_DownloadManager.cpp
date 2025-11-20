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
    DownloadManager_AppDownloadStatus,
    DownloadManager_AppInstallStatus
} DownloadManagerTest_status_t;

struct StatusParams {
    string packageId;
    string version;
    string downloadId;
    string fileLocator;
};

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
    FactoriesImplementation factoriesImplementation;

    Core::ProxyType<Plugin::DownloadManagerImplementation> mDownloadManagerImpl;
    Core::ProxyType<WorkerPoolImplementation> workerPool;

    Exchange::IDownloadManager* interface = nullptr;
    Exchange::IDownloadManager::Options options;
	string downloadId;
    uint8_t percent;
    uint32_t quotaKB, usedKB;

    // Constructor
    DownloadManagerTest()
	: workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(
            2, Core::Thread::DefaultStackSize(), 16)),
      plugin(Core::ProxyType<Plugin::DownloadManager>::Create()),
      mJsonRpcHandler(*plugin),
      INIT_CONX(1,0)
    {
        mDownloadManagerImpl = Core::ProxyType<Plugin::DownloadManagerImplementation>::Create();

        interface = static_cast<Exchange::IDownloadManager*>(mDownloadManagerImpl->QueryInterface(Exchange::IDownloadManager::ID));

		Core::IWorkerPool::Assign(&(*workerPool));
		workerPool->Run();
    }

    // Destructor
    virtual ~DownloadManagerTest() override
    {
        interface->Release();
        
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
          .WillRepeatedly(::testing::Return("{\"downloadDir\": \"/opt/CDL/\"}"));
		  
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
        interface->Initialize(mServiceMock);
    }

    void getDownloadParams()
    {
        // Initialize the parameters required for COM-RPC with default values
        uri = "https://www.examplefile.com/file-download/328";

        options = { 
            true,2,1024
        };

        downloadId = "";
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
        interface->Deinitialize(mServiceMock);
    }

    void waitforSignal(uint32_t timeout_ms) 
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
    }
};

#if 0
class NotificationTest : public Exchange::IDownloadManager::INotification, 
                         public Exchange::IPackageInstaller::INotification
{
    private:
        BEGIN_INTERFACE_MAP(NotificationTest)
        INTERFACE_ENTRY(Exchange::IDownloadManager::INotification)
        INTERFACE_ENTRY(Exchange::IPackageInstaller::INotification)
        END_INTERFACE_MAP

    public:
        /** @brief Mutex */
        std::mutex m_mutex;

        /** @brief Condition variable */
        std::condition_variable m_condition_variable;

        /** @brief Status signal flag */
        uint32_t m_status_signal = DownloadManager_invalidStatus;

        StatusParams m_status_param;

        NotificationTest(){}
        ~NotificationTest(){}

        void SetStatusParams(const StatusParams& statusParam)
        {
            m_status_param = statusParam;
        }

        void OnAppDownloadStatus(Exchange::IDownloadManager::IPackageInfoIterator* const packageInfos) override
        {
            m_status_signal = DownloadManager_AppDownloadStatus;
            JsonValue downloadId;
            JsonValue fileLocator;
            JsonValue failReason;

            std::unique_lock<std::mutex> lock(m_mutex);
            if(packageInfos != nullptr) 
            {
                Exchange::IDownloadManager::PackageInfo resultItem{};

                while (packageInfos->Next(resultItem) == true)
                {
                    downloadId = resultItem.downloadId;
                    fileLocator = resultItem.fileLocator;
                    failReason = (resultItem.reason == Exchange::IDownloadManager::Reason::NONE) ? "NONE" :
                                (resultItem.reason == Exchange::IDownloadManager::Reason::DOWNLOAD_FAILURE) ? "DOWNLOAD_FAILURE" :
                                (resultItem.reason == Exchange::IDownloadManager::Reason::DISK_PERSISTENCE_FAILURE) ? "DISK_PERSISTENCE_FAILURE" : "UNKNOWN";
                }
            }

            EXPECT_EQ(m_status_param.downloadId, downloadId.String());

            m_condition_variable.notify_one();
        }

        void OnAppInstallationStatus(const string& jsonresponse) override
        {
            m_status_signal = DownloadManager_AppInstallStatus;
            JsonValue packageId;
            JsonValue version;
            
            JsonArray arr;
            if(arr.IElement::FromString(jsonresponse) && arr.Length() > 0) {
                JsonObject obj = arr[0].Object();
                packageId = obj["packageId"];
                version = obj["version"]; 
            }

            std::unique_lock<std::mutex> lock(m_mutex);
            EXPECT_EQ(m_status_param.packageId, packageId.String());
            EXPECT_EQ(m_status_param.version, version.String());

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
#endif

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

	waitforSignal(200);

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

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));
    
    // TC-2: Add download request to regular queue using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"url\": \"https://httpbin.org/bytes/1024\"}"), mJsonRpcResponse));

    EXPECT_NE(mJsonRpcResponse.find("2001"), std::string::npos);
	
	waitforSignal(200);

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

	waitforSignal(200);
	
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
    EXPECT_EQ(Core::ERROR_NONE, interface->Download(uri, options, downloadId));
    
    waitforSignal(TIMEOUT);

    EXPECT_EQ(downloadId, "2001");

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
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, interface->Download(uri, options, downloadId));

	waitforSignal(200);
	
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

    waitforSignal(100);

    EXPECT_NE(mJsonRpcResponse.find("2001"), std::string::npos);

    // TC-6: Pause download via downloadId using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("pause"), _T("{\"downloadId\": \"2001\"}"), mJsonRpcResponse));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("cancel"), _T("{\"downloadId\": \"2001\"}"), mJsonRpcResponse));

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
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("pause"), _T("{\"downloadId\": \"2001\"}"), mJsonRpcResponse));

	waitforSignal(200);
	
    deinitforJsonRpc();
}

/* Test Case for pausing download via ID using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, notificatios/events, mocks and expectations
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

	uint32_t timeout_ms = 100;

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, interface->Download(uri, options, downloadId));

    waitforSignal(timeout_ms);

    EXPECT_EQ(downloadId, "2001");

    // TC-8: Pause download via downloadId using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, interface->Pause(downloadId));

    EXPECT_EQ(Core::ERROR_NONE, interface->Cancel(downloadId));

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

    string downloadId = "2001";

    // TC-9: Failure in pausing download using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, interface->Pause(downloadId));

	waitforSignal(200);
	
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

    waitforSignal(100);

    EXPECT_NE(mJsonRpcResponse.find("2001"), std::string::npos);

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("pause"), _T("{\"downloadId\": \"2001\"}"), mJsonRpcResponse));

    // TC-10: Resume download via downloadId using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("resume"), _T("{\"downloadId\": \"2001\"}"), mJsonRpcResponse));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("cancel"), _T("{\"downloadId\": \"2001\"}"), mJsonRpcResponse));

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
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("resume"), _T("{\"downloadId\": \"2001\"}"), mJsonRpcResponse));

	waitforSignal(200);
	 
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

    uint32_t timeout_ms = 100;

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

   	EXPECT_EQ(Core::ERROR_NONE, interface->Download(uri, options, downloadId));

    waitforSignal(timeout_ms);

    EXPECT_EQ(downloadId, "2001");
    
    EXPECT_EQ(Core::ERROR_NONE, interface->Pause(downloadId));

    // TC-12: Resume download via downloadId using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, interface->Resume(downloadId));

    EXPECT_EQ(Core::ERROR_NONE, interface->Cancel(downloadId));

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

    string downloadId = "2001";

    // TC-13: Failure in resuming download using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, interface->Resume(downloadId));

	waitforSignal(200);
	  
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

    waitforSignal(100);

    EXPECT_NE(mJsonRpcResponse.find("2001"), std::string::npos);

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("pause"), _T("{\"downloadId\": \"2001\"}"), mJsonRpcResponse));

    // TC-14: Cancel download via downloadId using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("cancel"), _T("{\"downloadId\": \"2001\"}"), mJsonRpcResponse));
	
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
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("cancel"), _T("{\"downloadId\": \"2001\"}"), mJsonRpcResponse));

	waitforSignal(200);
	 
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
    
    uint32_t timeout_ms = 100;

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, interface->Download(uri, options, downloadId));

    waitforSignal(timeout_ms);

    EXPECT_EQ(downloadId, "2001");
    
    EXPECT_EQ(Core::ERROR_NONE, interface->Pause(downloadId));

    // TC-16: Cancel download via downloadId using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, interface->Cancel(downloadId));

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

    string downloadId = "2001";

    // TC-17: Failure in cancelling download using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, interface->Cancel(downloadId));

	waitforSignal(200);
	
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

	uint32_t timeout_ms = 6000;

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"url\": \"https://httpbin.org/bytes/1024\"}"), mJsonRpcResponse));
	
    EXPECT_NE(mJsonRpcResponse.find("2001"), std::string::npos);

	waitforSignal(timeout_ms);
	
    // TC-18: Delete download using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("delete"), _T("{\"fileLocator\": \"/opt/CDL/package2001\"}"), mJsonRpcResponse));

	waitforSignal(200);
	
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

    // TC-19: Failure in delete using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("delete"), _T("{\"fileLocator\": \"\"}"), mJsonRpcResponse));

	waitforSignal(200);
	
	deinitforJsonRpc();
}

/* Test Case for delete download using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters and wait
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the delete method using the COM RPC interface, passing fileLocator
 * Verify successful delete by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, deleteMethodusingComRpcSuccess) {

    initforComRpc();

    getDownloadParams();

    uint32_t timeout_ms = 4000;

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, interface->Download(uri, options, downloadId));

    waitforSignal(timeout_ms);

    EXPECT_EQ(downloadId, "2001");

    string fileLocator = "/opt/CDL/package2001";

    // TC-20: Delete download failure when download in progress using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, interface->Delete(fileLocator));

	deinitforComRpc();
}

/* Test Case for delete download failure using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the delete method using the COM RPC interface, passing fileLocator as empty string
 * Verify delete method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, deleteMethodusingComRpcFailure) {

    initforComRpc();

    string fileLocator = "";

    // TC-21: Failure in delete using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, interface->Delete(fileLocator));

	waitforSignal(200);
	
	deinitforComRpc();
}

/* Test Case for download progress via ID using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters and wait
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the pause method using the JSON RPC handler, passing the downloadId
 * Verify that the pause method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Invoke the progress method using the JSON RPC handler, passing the downloadId and progress info
 * Verify that the progress method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking that response is not empty string.
 * Invoke the cancel method using the JSON RPC handler, passing the downloadId for cancelling download
 * Verify that the cancel method is invoked successfully by asserting that it returns Core::ERROR_NONE
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

    waitforSignal(100);

    EXPECT_NE(mJsonRpcResponse.find("2001"), std::string::npos);

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("pause"), _T("{\"downloadId\": \"2001\"}"), mJsonRpcResponse));

    // TC-22: Download progress via downloadId using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("progress"), _T("{\"downloadId\": \"2001\"}"), mJsonRpcResponse));

    EXPECT_NE(mJsonRpcResponse, "");

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("cancel"), _T("{\"downloadId\": \"2001\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
}

/* Test Case for download progress failure using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the progress method using the JSON RPC handler, passing downloadId and progress info
 * Verify progress method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, progressMethodusingJsonRpcFailure) {

    initforJsonRpc();

    // TC-23: Download progress failure using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("progress"), _T("{\"downloadId\": \"2001\"}"), mJsonRpcResponse));

	waitforSignal(200);
	
	deinitforJsonRpc();
}

/* Test Case for download progress via ID using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, notifications/events, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters and wait
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the pause method using the COM RPC interface along with downloadId
 * Verify successful pause by asserting that it returns Core::ERROR_NONE
 * Call the progress method using the COM RPC interface, passing the downloadId and progress info
 * Verify successful progress by asserting that it returns Core::ERROR_NONE and checking that progress is non-zero
 * Call the cancel method using the COM RPC interface, passing the downloadId for cancelling download
 * Verify successful cancel by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

 TEST_F(DownloadManagerTest, progressMethodusingComRpcSuccess) {

    initforComRpc();

    getDownloadParams();
    
    uint32_t timeout_ms = 100;

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    percent = 0;

    EXPECT_EQ(Core::ERROR_NONE, interface->Download(uri, options, downloadId));

    waitforSignal(timeout_ms);

    EXPECT_EQ(downloadId, "2001");

    EXPECT_EQ(Core::ERROR_NONE, interface->Pause(downloadId));

    // TC-24: Download progress via downloadId using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, interface->Progress(downloadId, percent));

    EXPECT_EQ(Core::ERROR_NONE, interface->Cancel(downloadId));
    
	deinitforComRpc();
}

/* Test Case for download progress failure using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the progress method using the COM RPC interface, passing downloadId and progress info
 * Verify progress method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, progressMethodusingComRpcFailure) {

    initforComRpc();

    percent = 0;

    string downloadId = "2001";

    // TC-25: Progress failure via downloadId using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, interface->Progress(downloadId, percent));

	waitforSignal(200);
	
	deinitforComRpc();
}

/* Test Case for getting storage details using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the getStorageDetails method using the JSON RPC handler, passing required parameters
 * Verify getStorageDetails method success by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, getStorageDetailsusingJsonRpc) {

    initforJsonRpc();

    // TC-26: Get Storage Details using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("getStorageDetails"), _T("{\"quotaKB\": \"1024\", \"usedKB\": \"568\"}"), mJsonRpcResponse));

	waitforSignal(200);
	
	deinitforJsonRpc();
}

/* Test Case for getting storage details using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the getStorageDetails method using the COM RPC interface, passing required parameters
 * Verify getStorageDetails method success by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, getStorageDetailsusingComRpc) {

    initforComRpc();

    quotaKB = 1024;
    usedKB = 568;

    // TC-27: Get Storage Details using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, interface->GetStorageDetails(quotaKB, usedKB));

	waitforSignal(200);
	
	deinitforComRpc();
}

/* Test Case for setting rate limit via ID using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters and wait
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the pause method using the JSON RPC handler, passing the downloadId
 * Verify that the pause method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Invoke the rateLimit method using the JSON RPC handler, passing the downloadId and the limit
 * Verify that the rateLimit method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Invoke the cancel method using the JSON RPC handler, passing the downloadId for cancelling download
 * Verify that the cancel method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, rateLimitusingJsonRpcSuccess) {

    initforJsonRpc();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"url\": \"https://www.examplefile.com/file-download/328\"}"), mJsonRpcResponse));

    waitforSignal(100);

    EXPECT_NE(mJsonRpcResponse.find("2001"), std::string::npos);

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("pause"), _T("{\"downloadId\": \"2001\"}"), mJsonRpcResponse));

    // TC-28: Set rate limit via downloadID using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("rateLimit"), _T("{\"downloadId\": \"2001\", \"limit\": 1024}"), mJsonRpcResponse));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("cancel"), _T("{\"downloadId\": \"2001\"}"), mJsonRpcResponse));

	deinitforJsonRpc();    
}

 /* Test Case for setting rate limit failure using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the rateLimit method using the JSON RPC handler, passing downloadId and limit
 * Verify rateLimit method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, rateLimitusingJsonRpcFailure) {

    initforJsonRpc();

    // TC-29: Rate limit failure using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("rateLimit"), _T("{\"downloadId\": \"2001\", \"limit\": 1024}"), mJsonRpcResponse));

	waitforSignal(200);
	
	deinitforJsonRpc();
}

/* Test Case for setting rate limit via ID using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters and wait
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the Pause method using the COM RPC interface along with downloadId
 * Verify successful pause by asserting that it returns Core::ERROR_NONE
 * Call the rateLimit method using the COM RPC interface, passing the downloadId and limit
 * Verify rateLimit is set successfully by asserting that it returns Core::ERROR_NONE
 * Call the cancel method using the COM RPC interface, passing the downloadId for cancelling download
 * Verify successful cancel by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, rateLimitusingComRpcSuccess) {

    initforComRpc();

    getDownloadParams();
    
    uint32_t timeout_ms = 100;

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    uint64_t limit = 1024;

    EXPECT_EQ(Core::ERROR_NONE, interface->Download(uri, options, downloadId));

    waitforSignal(timeout_ms);

    EXPECT_EQ(downloadId, "2001");

    EXPECT_EQ(Core::ERROR_NONE, interface->Pause(downloadId));

    // TC-30: Set rate limit via downloadID using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, interface->RateLimit(downloadId, limit));

    EXPECT_EQ(Core::ERROR_NONE, interface->Cancel(downloadId));
    
	deinitforComRpc();
}

/* Test Case for failure in setting rateLimit using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the rateLimit method using the COM RPC interface, passing downloadId and limit
 * Verify rateLimit method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

 TEST_F(DownloadManagerTest, rateLimitusingComRpcFailure) {

    initforComRpc();

    uint64_t limit = 1024;
    string downloadId = "2001";

    // TC-31: Rate limit failure using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, interface->RateLimit(downloadId, limit));

	waitforSignal(200);
	 
	deinitforComRpc();
}
