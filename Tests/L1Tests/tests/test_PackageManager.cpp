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

#include "PackageManager.h"
#include "PackageManagerImplementation.h"
#include "StorageManagerMock.h"
#include "ISubSystemMock.h"
#include "ServiceMock.h"
#include "COMLinkMock.h"
#include "ThunderPortability.h"
#include "WorkerPoolImplementation.h"
#include "FactoriesImplementation.h"

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);
#define TIMEOUT   (500)
#define DEBUG_PRINTF(fmt, ...) \
    std::printf("[DEBUG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

using ::testing::NiceMock;
using namespace WPEFramework;
using namespace std;

typedef enum : uint32_t {
    PackageManager_invalidStatus = 0,
    PackageManager_AppDownloadStatus,
    PackageManager_AppInstallStatus
} PackageManagerTest_status_t;

struct StatusParams {
    string packageId;
    string version;
    string downloadId;
    string fileLocator;
    Exchange::IPackageDownloader::Reason reason;
};

class PackageManagerTest : public ::testing::Test {
protected:
    // Declare the protected members
    ServiceMock* mServiceMock = nullptr;
    StorageManagerMock* mStorageManagerMock = nullptr;
    SubSystemMock* mSubSystemMock = nullptr;

    Core::ProxyType<Plugin::PackageManager> plugin;
    Core::JSONRPC::Handler& mJsonRpcHandler;
    Core::JSONRPC::Message message;
    DECL_CORE_JSONRPC_CONX connection;
    string mJsonRpcResponse;
    string uri;

    PLUGINHOST_DISPATCHER *dispatcher;
    FactoriesImplementation factoriesImplementation;

    Core::ProxyType<Plugin::PackageManagerImplementation> mPackageManagerImpl;
    Core::ProxyType<WorkerPoolImplementation> workerPool;

    Exchange::IPackageDownloader* pkgdownloaderInterface = nullptr;
    Exchange::IPackageInstaller* pkginstallerInterface = nullptr;
    Exchange::IPackageHandler* pkghandlerInterface = nullptr;
    Exchange::IPackageDownloader::Options options;
    Exchange::IPackageDownloader::DownloadId downloadId;
    Exchange::IPackageDownloader::ProgressInfo progress;
    Exchange::IPackageDownloader::PackageInfo packageInfo;
    std::list<Exchange::IPackageDownloader::PackageInfo> packageInfoList;
    Exchange::IPackageDownloader::IPackageInfoIterator* packageInfoIterator = nullptr;

    // Constructor
    PackageManagerTest()
	: workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(
            2, Core::Thread::DefaultStackSize(), 16)),
      plugin(Core::ProxyType<Plugin::PackageManager>::Create()),
      mJsonRpcHandler(*plugin),
      INIT_CONX(1,0)
    {
        mPackageManagerImpl = Core::ProxyType<Plugin::PackageManagerImplementation>::Create();

        pkgdownloaderInterface = static_cast<Exchange::IPackageDownloader*>(mPackageManagerImpl->QueryInterface(Exchange::IPackageDownloader::ID));

        pkginstallerInterface = static_cast<Exchange::IPackageInstaller*>(mPackageManagerImpl->QueryInterface(Exchange::IPackageInstaller::ID));

        pkghandlerInterface = static_cast<Exchange::IPackageHandler*>(mPackageManagerImpl->QueryInterface(Exchange::IPackageHandler::ID));

		Core::IWorkerPool::Assign(&(*workerPool));
		workerPool->Run();
    }

    // Destructor
    virtual ~PackageManagerTest() override
    {
        pkgdownloaderInterface->Release();
        pkginstallerInterface->Release();
        pkghandlerInterface->Release();
        
        Core::IWorkerPool::Assign(nullptr);
		workerPool.Release();
    }
	
	void SetUp() override 
	{		
		// Set up mocks and expect calls
        mServiceMock = new NiceMock<ServiceMock>;
        mStorageManagerMock = new NiceMock<StorageManagerMock>;
        mSubSystemMock = new NiceMock<SubSystemMock>;

        EXPECT_CALL(*mServiceMock, QueryInterfaceByCallsign(::testing::_, ::testing::_))
          .Times(::testing::AnyNumber())
          .WillRepeatedly(::testing::Invoke(
              [&](const uint32_t, const std::string& name) -> void* {
                if (name == "org.rdk.StorageManager") {
                    return reinterpret_cast<void*>(mStorageManagerMock);
                } 
            return nullptr;
        }));

        EXPECT_CALL(*mServiceMock, ConfigLine())
          .Times(::testing::AnyNumber())
          .WillRepeatedly(::testing::Return("{\"downloadDir\": \"/opt/CDL/\"}"));

        EXPECT_CALL(*mServiceMock, SubSystems())
          .Times(::testing::AnyNumber())
          .WillRepeatedly(::testing::Return(mSubSystemMock));

		 ASSERT_TRUE(pkgdownloaderInterface != nullptr);
		 ASSERT_TRUE(pkginstallerInterface != nullptr);
		 ASSERT_TRUE(pkghandlerInterface != nullptr);
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
        pkgdownloaderInterface->Initialize(mServiceMock);
    }

    void getDownloadParams()
    {
        // Initialize the parameters required for COM-RPC with default values
        uri = "https://www.examplefile.com/file-download/328";

        options = { 
            true,2,1024
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

		 ASSERT_TRUE(pkgdownloaderInterface != nullptr);
		 ASSERT_TRUE(pkginstallerInterface != nullptr);
		 ASSERT_TRUE(pkghandlerInterface != nullptr);
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
        
        if(mStorageManagerMock != nullptr)
        {
            delete mStorageManagerMock;
            mStorageManagerMock = nullptr;
        }
    }

    void deinitforComRpc()
    {
        EXPECT_CALL(*mServiceMock, Release())
          .Times(::testing::AnyNumber());

        EXPECT_CALL(*mStorageManagerMock, Release())
          .WillOnce(::testing::Invoke(
                [&]() {
                     delete mStorageManagerMock;
                     mStorageManagerMock = nullptr;
                     return 0;
            }));

        // Deinitialize the plugin for COM-RPC
        pkgdownloaderInterface->Deinitialize(mServiceMock);
    }

    void waitforJsonRpc(uint32_t timeout_ms) 
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
    }
};

class NotificationTest : public Exchange::IPackageDownloader::INotification, 
                         public Exchange::IPackageInstaller::INotification
{
    private:
        BEGIN_INTERFACE_MAP(Notification)
        INTERFACE_ENTRY(Exchange::IPackageDownloader::INotification)
        INTERFACE_ENTRY(Exchange::IPackageInstaller::INotification)
        END_INTERFACE_MAP

    public:
        /** @brief Mutex */
        std::mutex m_mutex;

        /** @brief Condition variable */
        std::condition_variable m_condition_variable;

        /** @brief Status signal flag */
        uint32_t m_status_signal = PackageManager_invalidStatus;

        StatusParams m_status_param;

        NotificationTest(){}
        ~NotificationTest(){}

        void SetStatusParams(const StatusParams& statusParam)
        {
            m_status_param = statusParam;
        }

        void OnAppDownloadStatus(Exchange::IPackageDownloader::IPackageInfoIterator* const packageInfos) override
        {
            m_status_signal = PackageManager_AppDownloadStatus;
            JsonValue downloadId;
            JsonValue fileLocator;
            JsonValue failReason;

            std::unique_lock<std::mutex> lock(m_mutex);
            if(packageInfos != nullptr) 
            {
                Exchange::IPackageDownloader::PackageInfo resultItem{};

                while (packageInfos->Next(resultItem) == true)
                {
                    downloadId = resultItem.downloadId;
                    fileLocator = resultItem.fileLocator;
                    failReason = (resultItem.reason == Exchange::IPackageDownloader::Reason::NONE) ? "NONE" :
                                (resultItem.reason == Exchange::IPackageDownloader::Reason::DOWNLOAD_FAILURE) ? "DOWNLOAD_FAILURE" :
                                (resultItem.reason == Exchange::IPackageDownloader::Reason::DISK_PERSISTENCE_FAILURE) ? "DISK_PERSISTENCE_FAILURE" : "UNKNOWN";
                }
            }

            EXPECT_EQ(m_status_param.downloadId, downloadId.String());

            m_condition_variable.notify_one();
        }

        void OnAppInstallationStatus(const string& jsonresponse) override
        {
            m_status_signal = PackageManager_AppInstallStatus;
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

        uint32_t WaitForStatusSignal(uint32_t timeout_ms, PackageManagerTest_status_t status)
        {
            DEBUG_PRINTF("--------------------ERROR: RDKEMW-2803-----------------------------");
            uint32_t status_signal = PackageManager_invalidStatus;
            std::unique_lock<std::mutex> lock(m_mutex);
            auto now = std::chrono::steady_clock::now();
            auto timeout = std::chrono::milliseconds(timeout_ms);
            while (!(status & m_status_signal))
            {
                DEBUG_PRINTF("--------------------ERROR: RDKEMW-2803-----------------------------");
                if (m_condition_variable.wait_until(lock, now + timeout) == std::cv_status::timeout)
                {
                    DEBUG_PRINTF("--------------------ERROR: RDKEMW-2803-----------------------------");
                    TEST_LOG("Timeout waiting for request status event");
                    return m_status_signal;
                }
            }
            DEBUG_PRINTF("--------------------ERROR: RDKEMW-2803-----------------------------");
            status_signal = m_status_signal;
            m_status_signal = PackageManager_invalidStatus;
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

TEST_F(PackageManagerTest, registeredMethodsusingJsonRpc) {

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
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("install")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("uninstall")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("listPackages")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("config")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("download")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("packageState")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("lock")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("unlock")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("getLockedInfo")));

	deinitforJsonRpc();
}

/* Test Case for adding download request to a regular queue using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, notifications/events, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters 
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, downloadMethodusingJsonRpcSuccess) {

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

    EVENT_SUBSCRIBE(0, _T("onAppDownloadStatus"), _T("org.rdk.PackageManagerRDKEMS"), message);
    
    // TC-2: Add download request to regular queue using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"url\": \"https://httpbin.org/bytes/1024\"}"), mJsonRpcResponse));

    EXPECT_EQ(Core::ERROR_NONE, onAppDownloadStatus.Lock());
    EVENT_UNSUBSCRIBE(0, _T("onAppDownloadStatus"), _T("org.rdk.PackageManagerRDKEMS"), message);

    EXPECT_NE(mJsonRpcResponse.find("1001"), std::string::npos);

    deinitforJsonRpc();
}

/* Test Case for checking download request error when internet is unavailable using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the method using the JSON RPC handler, passing the required parameters
 * Verify download method error due to unavailability of internet by asserting that it returns Core::ERROR_UNAVAILABLE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, downloadMethodusingJsonRpcError) {

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
 * Register the notification using the COM RPC interface and set the status params via the SetStatusParams()
 * Call the Download method using the COM RPC interface along with the required parameters, setting priority as true and wait
 * Verify successful download request by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Unregister the notification using the COM RPC interface
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, downloadMethodusingComRpcSuccess) {
    
    initforComRpc();

    getDownloadParams();

    uri = "https://httpbin.org/bytes/1024";

    Core::Sink<NotificationTest> notification;
    uint32_t signal = PackageManager_invalidStatus;

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type){
                return true;
            }));

    // Initialize the status params
    StatusParams statusParams;

    // Register the notification
    pkgdownloaderInterface->Register(&notification);
    
    // TC-4: Add download request to priority queue using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));
    
    statusParams.downloadId = "1001";
    notification.SetStatusParams(statusParams);
    signal = notification.WaitForStatusSignal(TIMEOUT, PackageManager_AppDownloadStatus);

    EXPECT_EQ(downloadId.downloadId, "1001");

    // Unregister the notification
    pkgdownloaderInterface->Unregister(&notification);

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

TEST_F(PackageManagerTest, downloadMethodusingComRpcError) {

    initforComRpc();

    getDownloadParams();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type){
                return false;
            }));
    
    // TC-5: Download request error when internet is unavailable using ComRpc
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, pkgdownloaderInterface->Download(uri, options, downloadId));

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

TEST_F(PackageManagerTest, pauseMethodusingJsonRpcSuccess) {

    initforJsonRpc();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"url\": \"https://www.examplefile.com/file-download/328\"}"), mJsonRpcResponse));

    waitforJsonRpc(200);

    EXPECT_NE(mJsonRpcResponse.find("1001"), std::string::npos);

    // TC-6: Pause download via downloadId using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("pause"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("cancel"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
}

/* Test Case for pausing failed using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the pause method using the JSON RPC handler, passing downloadId
 * Verify pause method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, pauseMethodusingJsonRpcFailure) {

    initforJsonRpc();

    // TC-7: Failure in pausing download using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("pause"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

    deinitforJsonRpc();
}

/* Test Case for pausing download via ID using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, notificatios/events, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Register the notification using the COM RPC interface and set the status params via the SetStatusParams()
 * Call the Download method using the COM RPC interface along with the required parameters and wait
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the pause method using the COM RPC interface, passing the downloadId
 * Verify successful pause by asserting that it returns Core::ERROR_NONE
 * Call the cancel method using the COM RPC interface, passing the downloadId for cancelling download
 * Verify successful cancel by asserting that it returns Core::ERROR_NONE
 * Unregister the notification using the COM RPC interface
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, pauseMethodusingComRpcSuccess) {

    initforComRpc();

    getDownloadParams();

    Core::Sink<NotificationTest> notification;
    uint32_t signal = PackageManager_invalidStatus;

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    // Initialize the status params
    StatusParams statusParams;

    // Register the notification
    pkgdownloaderInterface->Register(&notification);

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

    statusParams.downloadId = downloadId.downloadId;
    notification.SetStatusParams(statusParams);
    signal = notification.WaitForStatusSignal(TIMEOUT, PackageManager_AppDownloadStatus);

    EXPECT_EQ(downloadId.downloadId, "1001");

    string downloadId = "1001";

    // TC-8: Pause download via downloadId using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Pause(downloadId));

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Cancel(downloadId));

    // Unregister the notification
    pkgdownloaderInterface->Unregister(&notification);

	deinitforComRpc();    
}

/* Test Case for pausing failed using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the pause method using the COM RPC interface, passing downloadId
 * Verify pause method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, pauseMethodusingComRpcFailure) {

    initforComRpc();

    string downloadId = "1001";

    // TC-9: Failure in pausing download using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkgdownloaderInterface->Pause(downloadId));

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

TEST_F(PackageManagerTest, resumeMethodusingJsonRpcSuccess) {

    initforJsonRpc();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"url\": \"https://www.examplefile.com/file-download/328\"}"), mJsonRpcResponse));

    waitforJsonRpc(200);

    EXPECT_NE(mJsonRpcResponse.find("1001"), std::string::npos);

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("pause"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

    // TC-10: Resume download via downloadId using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("resume"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("cancel"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

	deinitforJsonRpc();    
}

 /* Test Case for resuming failed using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the resume method using the JSON RPC handler, passing downloadId
 * Verify resume method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

 TEST_F(PackageManagerTest, resumeMethodusingJsonRpcFailure) {

    initforJsonRpc();

    // TC-11: Failure in resuming download using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("resume"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
}

 /* Test Case for resuming download via ID using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, notifications/events, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Register the notification using the COM RPC interface and set the status params via the SetStatusParams()
 * Call the Download method using the COM RPC interface along with the required parameters and wait
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the pause method using the COM RPC interface, passing the downloadId
 * Verify successful pause by asserting that it returns Core::ERROR_NONE
 * Call the resume method using the COM RPC interface, passing the downloadId
 * Verify successful resume by asserting that it returns Core::ERROR_NONE
 * Call the cancel method using the COM RPC interface, passing the downloadId for cancelling download
 * Verify successful cancel by asserting that it returns Core::ERROR_NONE
 * Unregister the notification using the COM RPC interface
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, resumeMethodusingComRpcSuccess) {

    initforComRpc();

    getDownloadParams();

    Core::Sink<NotificationTest> notification;
    uint32_t signal = PackageManager_invalidStatus;
    uint32_t timeout_ms = 300;

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    // Initialize the status params
    StatusParams statusParams;

    // Register the notification
    pkgdownloaderInterface->Register(&notification);

   	EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

    statusParams.downloadId = downloadId.downloadId;
    notification.SetStatusParams(statusParams);
    signal = notification.WaitForStatusSignal(timeout_ms, PackageManager_AppDownloadStatus);

    EXPECT_EQ(downloadId.downloadId, "1001");

    string downloadId = "1001";
    
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Pause(downloadId));

    // TC-12: Resume download via downloadId using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Resume(downloadId));

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Cancel(downloadId));

    // Unregister the notification
    pkgdownloaderInterface->Unregister(&notification);

    deinitforComRpc();
}

 /* Test Case for resuming failed using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the resume method using the COM RPC interface, passing downloadId
 * Verify resume method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

  TEST_F(PackageManagerTest, resumeMethodusingComRpcFailure) {

    initforComRpc();

    string downloadId = "1001";

    // TC-13: Failure in resuming download using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkgdownloaderInterface->Resume(downloadId));

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

TEST_F(PackageManagerTest, cancelMethodusingJsonRpcSuccess) {

    initforJsonRpc();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"url\": \"https://www.examplefile.com/file-download/328\"}"), mJsonRpcResponse));

    waitforJsonRpc(200);

    EXPECT_NE(mJsonRpcResponse.find("1001"), std::string::npos);

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("pause"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

    // TC-14: Cancel download via downloadId using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("cancel"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));
	
    deinitforJsonRpc();
}

/* Test Case for cancelling failed using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the cancel method using the JSON RPC handler, passing downloadId
 * Verify cancel method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

 TEST_F(PackageManagerTest, cancelMethodusingJsonRpcFailure) {

    initforJsonRpc();

    // TC-15: Failure in cancelling download using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("cancel"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
}

/* Test Case for cancelling download via ID using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Register the notification using the COM RPC interface and set the status params via the SetStatusParams()
 * Call the Download method using the COM RPC interface along with the required parameters and wait
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the pause method using the COM RPC interface, passing the downloadId
 * Verify successful pause by asserting that it returns Core::ERROR_NONE
 * Call the cancel method using the COM RPC interface, passing the downloadId
 * Verify successful cancel by asserting that it returns Core::ERROR_NONE
 * Unregister the notification using the COM RPC interface
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, cancelMethodusingComRpcSuccess) {

    initforComRpc();

    getDownloadParams();
    
    Core::Sink<NotificationTest> notification;
    uint32_t signal = PackageManager_invalidStatus;
    uint32_t timeout_ms = 300;

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    // Initialize the status params
    StatusParams statusParams;

    // Register the notification
    pkgdownloaderInterface->Register(&notification);

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

    statusParams.downloadId = downloadId.downloadId;
    notification.SetStatusParams(statusParams);
    signal = notification.WaitForStatusSignal(timeout_ms, PackageManager_AppDownloadStatus);

    EXPECT_EQ(downloadId.downloadId, "1001");

    string downloadId = "1001";
    
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Pause(downloadId));

    // TC-16: Cancel download via downloadId using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Cancel(downloadId));

    // Unregister the notification
    pkgdownloaderInterface->Unregister(&notification);

	deinitforComRpc();
}

/* Test Case for cancelling failed using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the cancel method using the COM RPC interface, passing downloadId
 * Verify cancel method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, cancelMethodusingComRpcFailure) {

    initforComRpc();

    string downloadId = "1001";

    // TC-17: Failure in cancelling download using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkgdownloaderInterface->Cancel(downloadId));

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

TEST_F(PackageManagerTest, deleteMethodusingJsonRpcSuccess) {

    initforJsonRpc();

    Core::Event onAppDownloadStatus(false, true);

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
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

    EVENT_SUBSCRIBE(0, _T("onAppDownloadStatus"), _T("org.rdk.PackageManagerRDKEMS"), message);

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"url\": \"https://httpbin.org/bytes/1024\"}"), mJsonRpcResponse));

    EXPECT_EQ(Core::ERROR_NONE, onAppDownloadStatus.Lock());
    EVENT_UNSUBSCRIBE(0, _T("onAppDownloadStatus"), _T("org.rdk.PackageManagerRDKEMS"), message);

    EXPECT_NE(mJsonRpcResponse.find("1001"), std::string::npos);

    // TC-18: Delete download using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("delete"), _T("{\"fileLocator\": \"/opt/CDL/package1001\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
}

/* Test Case for delete failed using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the delete method using the JSON RPC handler, passing fileLocator
 * Verify delete method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, deleteMethodusingJsonRpcFailure) {

    initforJsonRpc();

    // TC-19: Failure in delete using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("delete"), _T("{\"fileLocator\": \"\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
}

/* Test Case for delete download using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Register the notification using the COM RPC interface and set the status params via the SetStatusParams()
 * Call the Download method using the COM RPC interface along with the required parameters and wait
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the delete method using the COM RPC interface, passing fileLocator
 * Verify successful delete by asserting that it returns Core::ERROR_NONE
 * Unregister the notification using the COM RPC interface
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, deleteMethodusingComRpcSuccess) {

    initforComRpc();

    getDownloadParams();

    Core::Sink<NotificationTest> notification;
    uint32_t signal = PackageManager_invalidStatus;
    uint32_t timeout_ms = 4000;

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    // Initialize the status params
    StatusParams statusParams;

    // Register the notification
    pkgdownloaderInterface->Register(&notification);

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

    statusParams.downloadId = downloadId.downloadId;
    notification.SetStatusParams(statusParams);
    signal = notification.WaitForStatusSignal(timeout_ms, PackageManager_AppDownloadStatus);

    EXPECT_EQ(downloadId.downloadId, "1001");

    string fileLocator = "/opt/CDL/package1001";

    // TC-20: Delete download failure when download in progress using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Delete(fileLocator));

    // Unregister the notification
    pkgdownloaderInterface->Unregister(&notification);

	deinitforComRpc();
}

/* Test Case for delete download failure using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the delete method using the COM RPC interface, passing fileLocator as empty string
 * Verify delete method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, deleteMethodusingComRpcFailure) {

    initforComRpc();

    string fileLocator = "";

    // TC-21: Failure in delete using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkgdownloaderInterface->Delete(fileLocator));

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

TEST_F(PackageManagerTest, progressMethodusingJsonRpcSuccess) {

    initforJsonRpc();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));
            
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"url\": \"https://www.examplefile.com/file-download/328\"}"), mJsonRpcResponse));

    waitforJsonRpc(200);

    EXPECT_NE(mJsonRpcResponse.find("1001"), std::string::npos);

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("pause"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

    // TC-22: Download progress via downloadId using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("progress"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

    EXPECT_NE(mJsonRpcResponse, "");

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("cancel"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
}

/* Test Case for download progress failure using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the progress method using the JSON RPC handler, passing downloadId and progress info
 * Verify progress method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, progressMethodusingJsonRpcFailure) {

    initforJsonRpc();

    // TC-23: Download progress failure using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("progress"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
}

/* Test Case for download progress via ID using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, notifications/events, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Register the notification using the COM RPC interface and set the status params via the SetStatusParams()
 * Call the Download method using the COM RPC interface along with the required parameters and wait
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the pause method using the COM RPC interface along with downloadId
 * Verify successful pause by asserting that it returns Core::ERROR_NONE
 * Call the progress method using the COM RPC interface, passing the downloadId and progress info
 * Verify successful progress by asserting that it returns Core::ERROR_NONE and checking that progress is non-zero
 * Call the cancel method using the COM RPC interface, passing the downloadId for cancelling download
 * Verify successful cancel by asserting that it returns Core::ERROR_NONE
 * Unregister the notification using the COM RPC interface
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

 TEST_F(PackageManagerTest, progressMethodusingComRpcSuccess) {

    initforComRpc();

    getDownloadParams();
    
    Core::Sink<NotificationTest> notification;
    uint32_t signal = PackageManager_invalidStatus;
    uint32_t timeout_ms = 200;

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    progress = {};

    // Initialize the status params
    StatusParams statusParams;

    // Register the notification
    pkgdownloaderInterface->Register(&notification);

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

    statusParams.downloadId = downloadId.downloadId;
    notification.SetStatusParams(statusParams);
    signal = notification.WaitForStatusSignal(timeout_ms, PackageManager_AppDownloadStatus);

    EXPECT_EQ(downloadId.downloadId, "1001");

    string downloadId = "1001";

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Pause(downloadId));

    // TC-24: Download progress via downloadId using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Progress(downloadId, progress));

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Cancel(downloadId));

    // Unregister the notification
    pkgdownloaderInterface->Unregister(&notification);
    
	deinitforComRpc();
}

/* Test Case for download progress failure using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the progress method using the COM RPC interface, passing downloadId and progress info
 * Verify progress method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, progressMethodusingComRpcFailure) {
    
    initforComRpc();

    progress = {};

    string downloadId = "1001";

    // TC-25: Progress failure via downloadId using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkgdownloaderInterface->Progress(downloadId, progress));

	deinitforComRpc();
}

/* Test Case for getting storage details using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the getStorageDetails method using the JSON RPC handler, passing required parameters
 * Verify getStorageDetails method success by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, getStorageDetailsusingJsonRpc) {

    initforJsonRpc();

    // TC-26: Get Storage Details using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("getStorageDetails"), _T("{\"quotaKB\": \"1024\", \"usedKB\": \"568\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
}

/* Test Case for getting storage details using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the getStorageDetails method using the COM RPC interface, passing required parameters
 * Verify getStorageDetails method success by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, getStorageDetailsusingComRpc) {

    initforComRpc();

    string quotaKB = "1024";
    string usedKB = "568";

    // TC-27: Get Storage Details using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->GetStorageDetails(quotaKB, usedKB));

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

TEST_F(PackageManagerTest, rateLimitusingJsonRpcSuccess) {

    initforJsonRpc();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"url\": \"https://www.examplefile.com/file-download/328\"}"), mJsonRpcResponse));

    waitforJsonRpc(200);

    EXPECT_NE(mJsonRpcResponse.find("1001"), std::string::npos);

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("pause"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

    // TC-28: Set rate limit via downloadID using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("rateLimit"), _T("{\"downloadId\": \"1001\", \"limit\": 1024}"), mJsonRpcResponse));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("cancel"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

	deinitforJsonRpc();    
}

 /* Test Case for setting rate limit failure using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the rateLimit method using the JSON RPC handler, passing downloadId and limit
 * Verify rateLimit method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, rateLimitusingJsonRpcFailure) {

    initforJsonRpc();

    // TC-29: Rate limit failure using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("rateLimit"), _T("{\"downloadId\": \"1001\", \"limit\": 1024}"), mJsonRpcResponse));

	deinitforJsonRpc();
}

/* Test Case for setting rate limit via ID using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Register the notification using the COM RPC interface and set the status params via the SetStatusParams()
 * Call the Download method using the COM RPC interface along with the required parameters and wait
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the Pause method using the COM RPC interface along with downloadId
 * Verify successful pause by asserting that it returns Core::ERROR_NONE
 * Call the rateLimit method using the COM RPC interface, passing the downloadId and limit
 * Verify rateLimit is set successfully by asserting that it returns Core::ERROR_NONE
 * Call the cancel method using the COM RPC interface, passing the downloadId for cancelling download
 * Verify successful cancel by asserting that it returns Core::ERROR_NONE
 * Unregister the notification using the COM RPC interface
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, rateLimitusingComRpcSuccess) {

    initforComRpc();

    getDownloadParams();
    
    Core::Sink<NotificationTest> notification;
    uint32_t signal = PackageManager_invalidStatus;
    uint32_t timeout_ms = 200;

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    uint64_t limit = 1024;

    // Initialize the status params
    StatusParams statusParams;

    // Register the notification
    pkgdownloaderInterface->Register(&notification);

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

    statusParams.downloadId = downloadId.downloadId;
    notification.SetStatusParams(statusParams);
    signal = notification.WaitForStatusSignal(timeout_ms, PackageManager_AppDownloadStatus);

    EXPECT_EQ(downloadId.downloadId, "1001");

    string downloadId = "1001";

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Pause(downloadId));

    // TC-30: Set rate limit via downloadID using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->RateLimit(downloadId, limit));

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Cancel(downloadId));

    // Unregister the notification
    pkgdownloaderInterface->Unregister(&notification);
    
	deinitforComRpc();
}

/* Test Case for failure in setting rateLimit using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the rateLimit method using the COM RPC interface, passing downloadId and limit
 * Verify rateLimit method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

 TEST_F(PackageManagerTest, rateLimitusingComRpcFailure) {

    initforComRpc();

    uint64_t limit = 1024;
    string downloadId = "1001";

    // TC-31: Rate limit failure using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkgdownloaderInterface->RateLimit(downloadId, limit));

	deinitforComRpc();
}

// IPackageInstaller methods

/* Test Case for error on install due to invalid signature using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the install method using the JSON RPC handler, passing the required parameters, keeping the file locator field empty
 * Verify that the install method fails by asserting that it returns Core::ERROR_INVALID_SIGNATURE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, installusingJsonRpcInvalidSignature) {

    initforJsonRpc();
    
    // TC-32: Error on install due to invalid signature using JsonRpc
    EXPECT_EQ(Core::ERROR_INVALID_SIGNATURE, mJsonRpcHandler.Invoke(connection, _T("install"), _T("{\"packageId\": \"testPackage\", \"version\": \"2.0\", \"additionalMetadata\": [{\"name\": \"testApp\", \"value\": \"2\"}], \"fileLocator\": \"\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
}

/* Test Case for install failure using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the install method using the JSON RPC handler, passing the required parameters
 * Verify that the install method fails by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, installusingJsonRpcFailure) {

    initforJsonRpc();

    // TC-33: Failure on install using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("install"), _T("{\"packageId\": \"testPackage\", \"version\": \"2.0\", \"additionalMetadata\": [{\"name\": \"testApp\", \"value\": \"2\"}], \"fileLocator\": \"/opt/CDL/package1001\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
}

 /* Test Case for error on install due to invalid signature using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the install method using the COM RPC interface, passing required parameters, keeping the fileLocator parameter as empty
 * Verify error on install by asserting that it returns Core::ERROR_INVALID_SIGNATURE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, installusingComRpcInvalidSignature) {

    initforComRpc();

    string packageId = "testPackage";
    string version = "2.0";
    string fileLocator = "";
    Exchange::IPackageInstaller::FailReason reason = Exchange::IPackageInstaller::FailReason::NONE;
    list<Exchange::IPackageInstaller::KeyValue> kv = { {"testapp", "2"} };

    auto additionalMetadata = Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IKeyValueIterator>>::Create<Exchange::IPackageInstaller::IKeyValueIterator>(kv);

    // TC-34: Error on install due to invalid signature using ComRpc
    EXPECT_EQ(Core::ERROR_INVALID_SIGNATURE, pkginstallerInterface->Install(packageId, version, additionalMetadata, fileLocator, reason));

    additionalMetadata->Release();

	deinitforComRpc();
}

/* Test Case for install failure using ComRpc
 * 
 * Set up and initialize required COM-RPC resources, configurations, notifications/events, mocks and expectations
 * Register the notification using the COM RPC interface and set the status params via the SetStatusParams()
 * Call the install method using the COM RPC interface, passing required parameters and wait for onAppInstallationStatus signal
 * Verify that the install method fails by asserting that it returns Core::ERROR_GENERAL
 * Unregister the notification using the COM RPC interface
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

 TEST_F(PackageManagerTest, installusingComRpcFailure) {

    initforComRpc();

    Core::Sink<NotificationTest> notification;
    uint32_t signal = PackageManager_invalidStatus;
    uint32_t timeout_ms = 1000;

    string packageId = "testPackage";
    string version = "2.0";
    string fileLocator = "/opt/CDL/package1001";
    Exchange::IPackageInstaller::FailReason reason = Exchange::IPackageInstaller::FailReason::NONE;
    list<Exchange::IPackageInstaller::KeyValue> kv = { {"testapp", "2"} };

    auto additionalMetadata = Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IKeyValueIterator>>::Create<Exchange::IPackageInstaller::IKeyValueIterator>(kv);

    // Initialize the status params
    StatusParams statusParams;
    statusParams.packageId = packageId;
    statusParams.version = version;

    // Register the notification
    pkginstallerInterface->Register(&notification);
    notification.SetStatusParams(statusParams);

    EXPECT_CALL(*mStorageManagerMock, CreateStorage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const uint32_t &size, string& path, string &errorReason) {
                return Core::ERROR_NONE;
            }));

    // TC-35: Failure on install using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkginstallerInterface->Install(packageId, version, additionalMetadata, fileLocator, reason));

    signal = notification.WaitForStatusSignal(timeout_ms, PackageManager_invalidStatus);

    signal = notification.WaitForStatusSignal(timeout_ms, PackageManager_AppInstallStatus);
    EXPECT_TRUE(signal == PackageManager_AppInstallStatus);

    signal = notification.WaitForStatusSignal(timeout_ms, PackageManager_AppInstallStatus);
    EXPECT_TRUE(signal == PackageManager_AppInstallStatus);

    // Unregister the notification
    pkginstallerInterface->Unregister(&notification);

    additionalMetadata->Release();

	deinitforComRpc();   
}

/* Test Case for uninstall failure using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the install method using the JSON RPC handler, passing the required parameters
 * Verify that the install method fails by asserting that it returns Core::ERROR_GENERAL
 * Invoke the uninstall method using the JSON RPC handler, passing the required parameters
 * Verify that the uninstall method fails by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, uninstallusingJsonRpcFailure) {

    initforJsonRpc();

    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("install"), _T("{\"packageId\": \"testPackage\", \"version\": \"2.0\", \"additionalMetadata\": [{\"name\": \"testApp\", \"value\": \"2\"}], \"fileLocator\": \"/opt/CDL/package1001\"}"), mJsonRpcResponse));

    // TC-36: Failure on uninstall using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("uninstall"), _T("{\"packageId\": \"testPackage\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
}

/* Test Case for uninstall failure using ComRpc
 * 
 * Set up and initialize required COM-RPC resources, configurations, notifications/events, mocks and expectations
 * Register the notification using the COM RPC interface and set the status params via the SetStatusParams()
 * Call the install method using the COM RPC interface, passing required parameters and wait for onAppInstallationStatus signal
 * Verify that the install method fails by asserting that it returns Core::ERROR_GENERAL
 * Call the uninstall method using the COM RPC interface, passing required parameters and wait for onAppInstallationStatus signal
 * Verify that the uninstall method fails by asserting that it returns Core::ERROR_GENERAL
 * Unregister the notification using the COM RPC interface
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, uninstallusingComRpcFailure) {

    initforComRpc();

    Core::Sink<NotificationTest> notification;
    uint32_t signal = PackageManager_invalidStatus;
    uint32_t timeout_ms = 1000;

    string packageId = "testPackage";
    string errorReason = "no error";
    string version = "2.0";
    string fileLocator = "/opt/CDL/package1001";
    Exchange::IPackageInstaller::FailReason reason = Exchange::IPackageInstaller::FailReason::NONE;
    list<Exchange::IPackageInstaller::KeyValue> kv = { {"testapp", "2"} };

    auto additionalMetadata = Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IKeyValueIterator>>::Create<Exchange::IPackageInstaller::IKeyValueIterator>(kv);

    // Initialize the status params
    StatusParams statusParams;
    statusParams.packageId = packageId;
    statusParams.version = version;

    // Register the notification
    pkginstallerInterface->Register(&notification);
    notification.SetStatusParams(statusParams);
    
    EXPECT_CALL(*mStorageManagerMock, CreateStorage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const uint32_t &size, string& path, string &errorReason) {
                return Core::ERROR_NONE;
            }));

    EXPECT_CALL(*mStorageManagerMock, DeleteStorage(::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, string &errorReason) {
                return Core::ERROR_NONE;
            }));

    EXPECT_EQ(Core::ERROR_GENERAL, pkginstallerInterface->Install(packageId, version, additionalMetadata, fileLocator, reason));

    signal = notification.WaitForStatusSignal(timeout_ms, PackageManager_invalidStatus);

    signal = notification.WaitForStatusSignal(timeout_ms, PackageManager_AppInstallStatus);
    EXPECT_TRUE(signal == PackageManager_AppInstallStatus);

    signal = notification.WaitForStatusSignal(timeout_ms, PackageManager_AppInstallStatus);
    EXPECT_TRUE(signal == PackageManager_AppInstallStatus);
    
    signal = PackageManager_invalidStatus;

	// TC-37: Failure on uninstall using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkginstallerInterface->Uninstall(packageId, errorReason));
    
    signal = notification.WaitForStatusSignal(timeout_ms, PackageManager_invalidStatus);

    signal = notification.WaitForStatusSignal(timeout_ms, PackageManager_AppInstallStatus);
    EXPECT_TRUE(signal == PackageManager_AppInstallStatus);

    signal = notification.WaitForStatusSignal(timeout_ms, PackageManager_AppInstallStatus);
    EXPECT_TRUE(signal == PackageManager_AppInstallStatus);

    // Unregister the notification
    pkginstallerInterface->Unregister(&notification);

    additionalMetadata->Release();

	deinitforComRpc();
}

/* Test Case for list packages method success using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the listPackages method using the JSON RPC handler, passing the required parameters
 * Verify that the listPackages method is successful by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, listPackagesusingJsonRpcSuccess) {

    initforJsonRpc();

	// TC-38: list packages using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("listPackages"), _T("{\"packages\": {}}"), mJsonRpcResponse));

	deinitforJsonRpc();   
}

/* Test Case for list packages method success using ComRpc
 * 
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the ListPackages method using the COM RPC interface, passing the required parameters
 * Verify that the ListPackages method is successful by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, listPackagesusingComRpcSuccess) {

    initforComRpc();

    list<Exchange::IPackageInstaller::Package> packageList = { {} };

    auto packages = Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IPackageIterator>>::Create<Exchange::IPackageInstaller::IPackageIterator>(packageList);

	// TC-39: list packages using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkginstallerInterface->ListPackages(packages));

    packages->Release();

	deinitforComRpc();
}

/* Test Case for config method failure using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the install method using the JSON RPC handler, passing the required parameters
 * Verify install method failure by asserting that it returns Core::ERROR_GENERAL
 * Invoke the config method using the JSON RPC handler, passing the required parameters
 * Verify config method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, configMethodusingJsonRpcSuccess) {

    initforJsonRpc();

    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("install"), _T("{\"packageId\": \"testPackage\", \"version\": \"2.0\", \"additionalMetadata\": [{\"name\": \"testApp\", \"value\": \"2\"}], \"fileLocator\": \"/opt/CDL/package1001\"}"), mJsonRpcResponse));

    // TC-40: Success in config using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("config"), _T("{\"packageId\": \"testPackage\", \"version\": \"2.0\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
}

/* Test Case for config method failure using ComRpc
 * 
 * Set up and initialize required COM-RPC resources, configurations, notifications/events, mocks and expectations
 * Register the notification using the COM RPC interface and set the status params via the SetStatusParams()
 * Call the install method using the COM RPC interface, passing the required parameters and wait for onAppInstallationStatus signal
 * Verify install method failure by asserting that it returns Core::ERROR_GENERAL
 * Call the config method using the COM RPC interface, passing the required parameters and wait
 * Verify config method failure by asserting that it returns Core::ERROR_GENERAL
 * Unregister the notification using the COM RPC interface
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, configMethodusingComRpcSuccess) {

    initforComRpc();

    Core::Sink<NotificationTest> notification;
    uint32_t signal = PackageManager_invalidStatus;
    uint32_t timeout_ms = 1000;

    string packageId = "testPackage";
    string version = "2.0";
    string fileLocator = "/opt/CDL/package1001";
    Exchange::IPackageInstaller::FailReason reason = Exchange::IPackageInstaller::FailReason::NONE;
    list<Exchange::IPackageInstaller::KeyValue> kv = { {"testapp", "2"} };
    Exchange::RuntimeConfig runtimeConfig = {};

    auto additionalMetadata = Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IKeyValueIterator>>::Create<Exchange::IPackageInstaller::IKeyValueIterator>(kv);

    // Initialize the status params
    StatusParams statusParams;
    statusParams.packageId = packageId;
    statusParams.version = version;

    // Register the notification
    pkginstallerInterface->Register(&notification);
    notification.SetStatusParams(statusParams);

    EXPECT_CALL(*mStorageManagerMock, CreateStorage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const uint32_t &size, string& path, string &errorReason) {
                return Core::ERROR_NONE;
            }));

    EXPECT_EQ(Core::ERROR_GENERAL, pkginstallerInterface->Install(packageId, version, additionalMetadata, fileLocator, reason));

    signal = notification.WaitForStatusSignal(timeout_ms, PackageManager_invalidStatus);

    signal = notification.WaitForStatusSignal(timeout_ms, PackageManager_AppInstallStatus);
    EXPECT_TRUE(signal == PackageManager_AppInstallStatus);

    signal = notification.WaitForStatusSignal(timeout_ms, PackageManager_AppInstallStatus);
    EXPECT_TRUE(signal == PackageManager_AppInstallStatus);

    signal = PackageManager_invalidStatus;

    // TC-41: Success in config using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkginstallerInterface->Config(packageId, version, runtimeConfig));

    signal = notification.WaitForStatusSignal(timeout_ms, PackageManager_invalidStatus);

    // Unregister the notification
    pkginstallerInterface->Unregister(&notification);

    additionalMetadata->Release();

	deinitforComRpc();
}

/* Test Case for package state failure using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the install method using the JSON RPC handler, passing the required parameters
 * Verify install method failure by asserting that it returns Core::ERROR_GENERAL
 * Invoke the packageState method using the JSON RPC handler, passing the required parameters
 * Verify packageState method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, packageStateusingJsonRpcSuccess) {

    initforJsonRpc();

    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("install"), _T("{\"packageId\": \"testPackage\", \"version\": \"2.0\", \"additionalMetadata\": [{\"name\": \"testApp\", \"value\": \"2\"}], \"fileLocator\": \"/opt/CDL/package1001\"}"), mJsonRpcResponse));

    // TC-42: Failure in package state using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("packageState"), _T("{\"packageId\": \"testPackage\", \"version\": \"2.0\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
}

/* Test Case for package state failure using ComRpc
 * 
 * Set up and initialize required COM-RPC resources, configurations, notifications/events, mocks and expectations
 * Register the notification using the COM RPC interface and set the status params via the SetStatusParams()
 * Call the install method using the COM RPC interface, passing the required parameters and wait for onAppInstallationStatus signal
 * Verify install method failure by asserting that it returns Core::ERROR_GENERAL
 * Call the PackageState method using the COM RPC interface, passing the required parameters and wait
 * Verify package state method failure by asserting that it returns Core::ERROR_GENERAL
 * Unregister the notification using the COM RPC interface
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, packageStateusingComRpcSuccess) {

    initforComRpc();

    Core::Sink<NotificationTest> notification;
    uint32_t signal = PackageManager_invalidStatus;
    uint32_t timeout_ms = 1000;

    string packageId = "testPackage";
    string version = "2.0";
    string fileLocator = "/opt/CDL/package1001";
    Exchange::IPackageInstaller::FailReason reason = Exchange::IPackageInstaller::FailReason::NONE;
    list<Exchange::IPackageInstaller::KeyValue> kv = { {"testapp", "2"} };
    Exchange::IPackageInstaller::InstallState state = Exchange::IPackageInstaller::InstallState::INSTALLING;

    auto additionalMetadata = Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IKeyValueIterator>>::Create<Exchange::IPackageInstaller::IKeyValueIterator>(kv);

    // Initialize the status params
    StatusParams statusParams;
    statusParams.packageId = packageId;
    statusParams.version = version;

    // Register the notification
    pkginstallerInterface->Register(&notification);
    notification.SetStatusParams(statusParams);

    EXPECT_CALL(*mStorageManagerMock, CreateStorage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const uint32_t &size, string& path, string &errorReason) {
                return Core::ERROR_NONE;
            }));

    EXPECT_EQ(Core::ERROR_GENERAL, pkginstallerInterface->Install(packageId, version, additionalMetadata, fileLocator, reason));

    signal = notification.WaitForStatusSignal(timeout_ms, PackageManager_invalidStatus);

    signal = notification.WaitForStatusSignal(timeout_ms, PackageManager_AppInstallStatus);
    EXPECT_TRUE(signal == PackageManager_AppInstallStatus);

    signal = notification.WaitForStatusSignal(timeout_ms, PackageManager_AppInstallStatus);
    EXPECT_TRUE(signal == PackageManager_AppInstallStatus);

    signal = PackageManager_invalidStatus;

    // TC-43: Failure in package state using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkginstallerInterface->PackageState(packageId, version, state));

    signal = notification.WaitForStatusSignal(timeout_ms, PackageManager_invalidStatus);

    // Unregister the notification
    pkginstallerInterface->Unregister(&notification);

    additionalMetadata->Release();

	deinitforComRpc();
}

 /* Test Case for get config for package error due to invalid signature using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the getConfigforPackages method using the JSON RPC handler, passing the required parameters, keeping the file locator field empty
 * Verify getConfigforPackages method error by asserting that it returns Core::ERROR_INVALID_SIGNATURE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, getConfigforPackageusingJsonRpcInvalidSignature) {

    initforJsonRpc();

    // TC-44: Error in get config for packages due to empty file locator using JsonRpc
    EXPECT_EQ(Core::ERROR_INVALID_SIGNATURE, mJsonRpcHandler.Invoke(connection, _T("getConfigForPackage"), _T("{\"fileLocator\": \"\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
}

/* Test Case for get config for package failure using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the getConfigforPackages method using the JSON RPC handler, passing the required parameters
 * Verify that the getConfigforPackages method fails by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, getConfigforPackageusingJsonRpcFailure) {

    initforJsonRpc();

    // TC-45: Failure in get config for packages using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("getConfigForPackage"), _T("{\"fileLocator\": \"/opt/CDL/package1001\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
}

/* Test Case for get config for packages error due to invalid signature using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the GetConfigforPackage method using the COM RPC interface, passing required parameters, keeping the fileLocator parameter as empty
 * Verify GetConfigforPackage error by asserting that it returns Core::ERROR_INVALID_SIGNATURE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, getConfigforPackageusingComRpcInvalidSignature) {

    initforComRpc();

    string fileLocator = "";
    string id = "1001";
    string version = "2.0";

    Exchange::RuntimeConfig config = {};

    // TC-46: Error in get config for packages due to empty file locator using ComRpc
    EXPECT_EQ(Core::ERROR_INVALID_SIGNATURE, pkginstallerInterface->GetConfigForPackage(fileLocator, id, version, config));

	deinitforComRpc();
}

/* Test Case for get config for package failure using ComRpc
 * 
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the GetConfigforPackage method using the COM RPC interface, passing required parameters
 * Verify that the GetConfigforPackage method fails by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, getConfigforPackageusingComRpcFailure) {

    initforComRpc();

    string fileLocator = "/opt/CDL/package1001";
    string id = "1001";
    string version = "2.0";

    Exchange::RuntimeConfig config = {};

    // TC-47: Failure in get config for packages using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkginstallerInterface->GetConfigForPackage(fileLocator, id, version, config));

	deinitforComRpc();
}

// IPackageHandler methods

/* Test Case for lock error using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the lock method using the JSON RPC handler, passing the required parameters
 * Verify lock method error by asserting that it returns Core::ERROR_BAD_REQUEST
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, lockmethodusingJsonRpcError) {

    initforJsonRpc();
    
    // TC-48: Error on lock using JsonRpc
    EXPECT_EQ(Core::ERROR_BAD_REQUEST, mJsonRpcHandler.Invoke(connection, _T("lock"), _T("{\"packageId\": \"testPackage\", \"version\": \"2.0\", \"lockReason\": 0 }"), mJsonRpcResponse));

	deinitforJsonRpc();
}

/* Test Case for lock error using ComRpc
 * 
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the Lock method using the COM RPC interface, passing required parameters
 * Verify Lock method error by asserting that it returns Core::ERROR_BAD_REQUEST
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, lockmethodusingComRpcError) {

    initforComRpc();

    string packageId = "testPackage";
    string version = "2.0";
    Exchange::IPackageHandler::LockReason lockReason = Exchange::IPackageHandler::LockReason::SYSTEM_APP;
    uint32_t lockId = 132;
    string unpackedPath = "testPath";
    Exchange::RuntimeConfig configMetadata = {};
    list<Exchange::IPackageHandler::AdditionalLock> additionalLock = { {} };

    auto appMetadata = Core::Service<RPC::IteratorType<Exchange::IPackageHandler::ILockIterator>>::Create<Exchange::IPackageHandler::ILockIterator>(additionalLock);

	// TC-49: Error on lock using ComRpc
    EXPECT_EQ(Core::ERROR_BAD_REQUEST, pkghandlerInterface->Lock(packageId, version, lockReason, lockId, unpackedPath, configMetadata, appMetadata));

    appMetadata->Release();

	deinitforComRpc();
}

/* Test Case for unlock error using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the unlock method using the JSON RPC handler, passing the required parameters
 * Verify unlock method error by asserting that it returns Core::ERROR_BAD_REQUEST
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, unlockmethodusingJsonRpcError) {

    initforJsonRpc();

	// TC-50: Error on unlock using JsonRpc
    EXPECT_EQ(Core::ERROR_BAD_REQUEST, mJsonRpcHandler.Invoke(connection, _T("unlock"), _T("{\"packageId\": \"testPackage\", \"version\": \"2.0\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
}

/* Test Case for unlock error using ComRpc
 * 
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the Unlock method using the COM RPC interface, passing required parameters
 * Verify Unlock method error by asserting that it returns Core::ERROR_BAD_REQUEST
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, unlockmethodusingComRpcError) {

    initforComRpc();

    string packageId = "testPackage";
    string version = "2.0";

    // TC-51: Error on unlock using ComRpc
    EXPECT_EQ(Core::ERROR_BAD_REQUEST, pkghandlerInterface->Unlock(packageId, version));

	deinitforComRpc();
}

/* Test Case for get locked info error using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the getLockedInfo method using the JSON RPC handler, passing the required parameters
 * Verify getLockedInfo method error by asserting that it returns Core::ERROR_BAD_REQUEST
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, getLockedInfousingJsonRpcError) {

    initforJsonRpc();

    // TC-52: Error on get locked info using JsonRpc
    EXPECT_EQ(Core::ERROR_BAD_REQUEST, mJsonRpcHandler.Invoke(connection, _T("getLockedInfo"), _T("{\"packageId\": \"testPackage\", \"version\": \"2.0\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
}

/* Test Case for get locked info error using ComRpc
 * 
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the GetLockedInfo method using the COM RPC interface, passing required parameters
 * Verify GetLockedInfo method error by asserting that it returns Core::ERROR_BAD_REQUEST
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, getLockedInfousingComRpcError) {

    initforComRpc();

    string packageId = "testPackage";
    string version = "2.0";
    string unpackedPath = "testPath";
    Exchange::RuntimeConfig configMetadata = {};
    string gatewayMetadataPath = "testgatewayMetadataPath";
    bool locked = true;

    // TC-53: Error on get locked info using ComRpc
    EXPECT_EQ(Core::ERROR_BAD_REQUEST, pkghandlerInterface->GetLockedInfo(packageId, version, unpackedPath, configMetadata, gatewayMetadataPath, locked));

	deinitforComRpc();
}
