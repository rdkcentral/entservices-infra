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
#define TIMEOUT   (2000)

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

namespace {
    const string callsign = _T("PackageManager");
}

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
    //string ndownloadId;
    //string nfileLocator;
    //Exchange::IPackageDownloader::Reason nreason;
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

    //StatusParams statusParams;
    //Core::Sink<NotificationTest> downloadNotification;
    //uint32_t downloadSignal;
    
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
        Core::IWorkerPool::Assign(nullptr);
		workerPool.Release();
    }
	
	void createResources() 
	{		

        DEBUG_PRINTF("-----------------------DEBUG-2803------------------------");
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

		EXPECT_CALL(*mServiceMock, AddRef())
          .Times(::testing::AnyNumber());

        EXPECT_CALL(*mServiceMock, Register(::testing::_))
          .Times(::testing::AnyNumber());

        PluginHost::IFactories::Assign(&factoriesImplementation);
        dispatcher = static_cast<PLUGINHOST_DISPATCHER*>(plugin->QueryInterface(PLUGINHOST_DISPATCHER_ID));
        dispatcher->Activate(mServiceMock);
        plugin->Initialize(mServiceMock);

        DEBUG_PRINTF("-----------------------DEBUG-2803------------------------");
    }

    void getDownloadParams()
    {

        DEBUG_PRINTF("-----------------------DEBUG-2803------------------------");
        // Initialize the parameters required for COM-RPC with default values
        uri = "http://test.com";

        options = { 
            true,2,1024
        };

        downloadId = {};

        DEBUG_PRINTF("-----------------------DEBUG-2803------------------------");
    }

    void releaseResources()
    {	
		// Clean up mocks
		if (mServiceMock != nullptr)
        {
			EXPECT_CALL(*mServiceMock, Unregister(::testing::_))
              .Times(::testing::AnyNumber());
			
			EXPECT_CALL(*mServiceMock, Release())
              .WillOnce(::testing::Invoke(
              [&]() {
						delete mServiceMock;
						mServiceMock = nullptr;
						return 0;
					}));    
        }
		
        DEBUG_PRINTF("-----------------------DEBUG-2803------------------------");
        if (mStorageManagerMock != nullptr)
        {
            DEBUG_PRINTF("-----------------------DEBUG-2803------------------------");
			EXPECT_CALL(*mStorageManagerMock, Release())
              .WillOnce(::testing::Invoke(
              [&]() {
						delete mStorageManagerMock;
						mStorageManagerMock = nullptr;
						return 0;
					}));
        }

        DEBUG_PRINTF("-----------------------DEBUG-2803------------------------");
		
		if(mSubSystemMock != nullptr)
        {
            delete mSubSystemMock;
            mSubSystemMock = nullptr;
		}

        DEBUG_PRINTF("-----------------------DEBUG-2803------------------------");

        dispatcher->Deactivate();
        dispatcher->Release();

        plugin->Deinitialize(mServiceMock);

        DEBUG_PRINTF("-----------------------DEBUG-2803------------------------");
    }

    #if 0
    void handleDownloadNotification()
    {
        downloadSignal = PackageManager_invalidStatus;

        ndownloadId = "1001";
        nfileLocator = "/opt/CDL/package1001";
        nreason = Exchange::IPackageDownloader::FailReason::NONE;

        // Initialize the status params
        statusParams.downloadId = ndownloadId;
        statusParams.fileLocator = nfileLocator;
        statusParams.reason = nreason;

        // Register the notification
        plugin->Register(static_cast<Exchange::IPackageDownloader::INotification*>(&downloadNotification));
        downloadNotification.SetStatusParams(statusParams);
    }
    #endif
};

class NotificationTest : public Exchange::IPackageInstaller::INotification
{
    private:
        BEGIN_INTERFACE_MAP(Notification)
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

        #if 0
        void OnAppDownloadStatus(const string& downloadId, const string& fileLocator, const Exchange::IPackageDownloader::Reason reason)
        {
            m_status_signal = PackageManager_AppDownloadStatus;

            std::unique_lock<std::mutex> lock(m_mutex);
            EXPECT_EQ(m_status_param.downloadId, downloadId);
            EXPECT_EQ(m_status_param.fileLocator, fileLocator);
            EXPECT_EQ(m_status_param.reason, reason);

            m_condition_variable.notify_one();
        }
        #endif

        void OnAppInstallationStatus(const string& jsonresponse) override
        {
            m_status_signal = PackageManager_AppInstallStatus;
            JsonValue packageId;
            JsonValue version;
            
            JsonObject obj;
            if(obj.IElement::FromString(jsonresponse)) {
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
            uint32_t status_signal = PackageManager_invalidStatus;
            std::unique_lock<std::mutex> lock(m_mutex);
            auto now = std::chrono::steady_clock::now();
            auto timeout = std::chrono::milliseconds(timeout_ms);
            if (m_condition_variable.wait_until(lock, now + timeout) == std::cv_status::timeout)
            {
                 TEST_LOG("Timeout waiting for request status event");
                 return m_status_signal;
            }
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

    createResources();

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

    releaseResources();
}

/* Test Case for adding download request to a queue(priority/regular) using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters and setting priority as true
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the download method using the JSON RPC handler, passing the required parameters and setting priority as false
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, downloadMethodusingJsonRpcSuccess) {
    
    createResources();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));
    
    // TC-2: Add download request to priority queue using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"uri\": \"http://test.com\", \"options\": {\"priority\": true, \"retries\": 2, \"rateLimit\": 1024}, \"downloadId\": {}}"), mJsonRpcResponse));

    EXPECT_NE(mJsonRpcResponse.find("1001"), std::string::npos);

    // TC-3: Add download request to regular queue using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"uri\": \"http://test.com\", \"options\": {\"priority\": false, \"retries\": 2, \"rateLimit\": 1024}, \"downloadId\": {}}"), mJsonRpcResponse));

    EXPECT_NE(mJsonRpcResponse.find("1001"), std::string::npos);

    releaseResources();   
}

/* Test Case for checking download request error when internet is unavailable using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the method using the JSON RPC handler, passing the required parameters and setting priority as true
 * Verify download method error due to unavailability of internet by asserting that it returns Core::ERROR_UNAVAILABLE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, downloadMethodusingJsonRpcError) {
    
    createResources();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return false;
            }));
    
    // TC-4: Download request error when internet is unavailable using JsonRpc
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"uri\": \"http://test.com\", \"options\": {\"priority\": true, \"retries\": 2, \"rateLimit\": 1024}, \"downloadId\": {}}"), mJsonRpcResponse));
    
    releaseResources();
}

/* Test Case for adding download request to a queue(priority/regular) using ComRpc
 * 
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters, setting priority as true
 * Verify successful download request by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the Download method using the COM RPC interface again along with the required parameters, setting priority as false
 * Verify successful download request by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, downloadMethodsusingComRpcSuccess) {

    createResources();

    getDownloadParams();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type){
                return true;
            }));
    
    #if 0
    handleDownloadNotification();
    #endif
    
    // TC-5: Add download request to priority queue using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));
    
    EXPECT_EQ(downloadId.downloadId, "1001");
    
    #if 0
    downloadNotification.OnAppDownloadStatus(ndownloadId, nfileLocator, nreason);
    downloadSignal = downloadNotification.WaitForStatusSignal(TIMEOUT, PackageManager_AppDownloadStatus);
    EXPECT_TRUE(downloadSignal & PackageManager_AppDownloadStatus);
    downloadSignal = PackageManager_invalidStatus;
    #endif

    options.priority = false;

    // TC-6: Add download request to regular queue using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

    EXPECT_EQ(downloadId.downloadId, "1001");
    
    #if 0
    downloadNotification.OnAppDownloadStatus(ndownloadId, nfileLocator, nreason);
    downloadSignal = downloadNotification.WaitForStatusSignal(TIMEOUT, PackageManager_AppDownloadStatus);
    EXPECT_TRUE(downloadSignal & PackageManager_AppDownloadStatus);
    
    // Unregister the notification
    plugin->Unregister(static_cast<Exchange::IPackageDownloader::INotification*>(&downloadNotification));
    #endif
    
    releaseResources();
}

/* Test Case for checking download request error when internet is unavailable using ComRpc
 * 
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters
 * Verify download method error due to unavailability of internet by asserting that it returns Core::ERROR_UNAVAILABLE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, downloadMethodsusingComRpcError) {

    createResources();

    getDownloadParams();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type){
                return false;
            }));

    //handleDownloadNotification();
    
    // TC-7: Download request error when internet is unavailable using ComRpc
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, pkgdownloaderInterface->Download(uri, options, downloadId));
    #if 0
    downloadNotification.OnAppDownloadStatus(ndownloadId, nfileLocator, nreason);
    downloadSignal = downloadNotification.WaitForStatusSignal(TIMEOUT, PackageManager_AppDownloadStatus);
    EXPECT_TRUE(downloadSignal & PackageManager_AppDownloadStatus);

    // Unregister the notification
    plugin->Unregister(static_cast<Exchange::IPackageDownloader::INotification*>(&downloadNotification));
    #endif

    releaseResources();
}

/* Test Case for pausing download via ID using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the pause method using the JSON RPC handler, passing the downloadId
 * Verify that the pause method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, pauseMethodusingJsonRpcSuccess) {

    createResources();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));
    
    
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"uri\": \"http://test.com\", \"options\": {\"priority\": true, \"retries\": 2, \"rateLimit\": 1024}, \"downloadId\": {}}"), mJsonRpcResponse));
    
    EXPECT_NE(mJsonRpcResponse.find("1001"), std::string::npos);

    // TC-8: Pause download via downloadId using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("pause"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));
 
    releaseResources();
}

/* Test Case for pausing failed using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the pause method using the JSON RPC handler, passing downloadId
 * Verify pause method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, pauseMethodusingJsonRpcFailure) {

    createResources();

    // TC-10: Failure in pausing download using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("pause"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

    releaseResources();
}

/* Test Case for pausing download via ID using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the pause method using the COM RPC interface, passing the downloadId
 * Verify successful pause by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, pauseMethodusingComRpcSuccess) {

    createResources();

    getDownloadParams();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    //handleDownloadNotification();
    
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

    EXPECT_EQ(downloadId.downloadId, "1001");
    
    #if 0
    downloadNotification.OnAppDownloadStatus(ndownloadId, nfileLocator, nreason);
    downloadSignal = downloadNotification.WaitForStatusSignal(TIMEOUT, PackageManager_AppDownloadStatus);
    EXPECT_TRUE(downloadSignal & PackageManager_AppDownloadStatus);
    #endif
    string downloadId = "1001";

    // TC-11: Pause download via downloadId using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Pause(downloadId));

    // Unregister the notification
    //plugin->Unregister(static_cast<Exchange::IPackageDownloader::INotification*>(&downloadNotification));

    releaseResources();	
}

/* Test Case for pausing failed using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the pause method using the COM RPC interface, passing downloadId
 * Verify pause method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, pauseMethodusingComRpcFailure) {

    createResources();

    string downloadId = "1001";

    // TC-13: Failure in pausing download using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkgdownloaderInterface->Pause(downloadId));

    releaseResources();
}

/* Test Case for resuming download via ID using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the resume method using the JSON RPC handler, passing the downloadId
 * Verify that the resume method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, resumeMethodusingJsonRpcSuccess) {

    createResources();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"uri\": \"http://test.com\", \"options\": {\"priority\": true, \"retries\": 2, \"rateLimit\": 1024}, \"downloadId\": {}}"), mJsonRpcResponse));

    EXPECT_NE(mJsonRpcResponse.find("1001"), std::string::npos);

    // TC-14: Resume download via downloadId using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("resume"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

    releaseResources();	
}

 /* Test Case for resuming failed using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the resume method using the JSON RPC handler, passing downloadId
 * Verify resume method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

 TEST_F(PackageManagerTest, resumeMethodusingJsonRpcFailure) {

    createResources();

    // TC-16: Failure in resuming download using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("resume"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

    releaseResources();	
}

 /* Test Case for resuming download via ID using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the resume method using the COM RPC interface, passing the downloadId
 * Verify successful resume by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, resumeMethodusingComRpcSuccess) {

    createResources();

    getDownloadParams();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    //handleDownloadNotification();

   	EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

    EXPECT_EQ(downloadId.downloadId, "1001");
    
    #if 0
    downloadNotification.OnAppDownloadStatus(ndownloadId, nfileLocator, nreason);
    downloadSignal = downloadNotification.WaitForStatusSignal(TIMEOUT, PackageManager_AppDownloadStatus);
    EXPECT_TRUE(downloadSignal & PackageManager_AppDownloadStatus);
    #endif
    // TC-17: Resume download via downloadId using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Resume(downloadId.downloadId));
    
    // Unregister the notification
    //plugin->Unregister(static_cast<Exchange::IPackageDownloader::INotification*>(&downloadNotification));

    releaseResources();
}

 /* Test Case for resuming failed using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the resume method using the COM RPC interface, passing downloadId
 * Verify resume method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

  TEST_F(PackageManagerTest, resumeMethodusingComRpcFailure) {

    createResources();

    string downloadId = "1001";

    // TC-19: Failure in resuming download using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkgdownloaderInterface->Resume(downloadId));

    releaseResources();
}

/* Test Case for cancelling download via ID using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the cancel method using the JSON RPC handler, passing the downloadId
 * Verify that the cancel method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, cancelMethodusingJsonRpcSuccess) {

    createResources();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"uri\": \"http://test.com\", \"options\": {\"priority\": true, \"retries\": 2, \"rateLimit\": 1024}, \"downloadId\": {}}"), mJsonRpcResponse));

    EXPECT_NE(mJsonRpcResponse.find("1001"), std::string::npos);

    // TC-20: Cancel download via downloadId using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("cancel"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

    releaseResources();	
}

/* Test Case for cancelling failed using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the cancel method using the JSON RPC handler, passing downloadId
 * Verify cancel method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

 TEST_F(PackageManagerTest, cancelMethodusingJsonRpcFailure) {

    createResources();

    // TC-22: Failure in cancelling download using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("cancel"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

    releaseResources();
}

/* Test Case for cancelling download via ID using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the cancel method using the COM RPC interface, passing the downloadId
 * Verify successful cancel by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, cancelMethodusingComRpcSuccess) {

    createResources();

    getDownloadParams();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    //handleDownloadNotification();

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

    EXPECT_EQ(downloadId.downloadId, "1001");
    
    #if 0
    downloadNotification.OnAppDownloadStatus(ndownloadId, nfileLocator, nreason);
    downloadSignal = downloadNotification.WaitForStatusSignal(TIMEOUT, PackageManager_AppDownloadStatus);
    EXPECT_TRUE(downloadSignal & PackageManager_AppDownloadStatus);
    #endif
    // TC-23: Cancel download via downloadId using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Cancel(downloadId.downloadId));
    
    // Unregister the notification
    //plugin->Unregister(static_cast<Exchange::IPackageDownloader::INotification*>(&downloadNotification));

    releaseResources();	    
}

/* Test Case for cancelling failed using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the cancel method using the COM RPC interface, passing downloadId
 * Verify cancel method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, cancelMethodusingComRpcFailure) {

    createResources();

    string downloadId = "1001";

    // TC-25: Failure in cancelling download using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkgdownloaderInterface->Cancel(downloadId));

    releaseResources();
}

/* Test Case for delete download failure when download in progress using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the delete method using the JSON RPC handler, passing the fileLocator
 * Verify failure in delete by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, deleteMethodusingJsonRpcInProgressFail) {

    createResources();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"uri\": \"http://test.com\", \"options\": {\"priority\": true, \"retries\": 2, \"rateLimit\": 1024}, \"downloadId\": {}}"), mJsonRpcResponse));

    EXPECT_NE(mJsonRpcResponse.find("1001"), std::string::npos);

    // TC-26: Delete download failure when download in progress using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("delete"), _T("{\"fileLocator\": \"/opt/CDL/package1001\"}"), mJsonRpcResponse));

    releaseResources();
}

/* Test Case for delete failed using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the delete method using the JSON RPC handler, passing fileLocator
 * Verify delete method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, deleteMethodusingJsonRpcFailure) {

    createResources();

    // TC-27: Failure in delete using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("delete"), _T("{\"fileLocator\": \"/opt/CDL/package1001\"}"), mJsonRpcResponse));

    releaseResources();
}

/* Test Case for delete download failure when download in progress using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the delete method using the COM RPC interface, passing fileLocator
 * Verify failure in delete by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, deleteMethodusingComRpcInProgressFail) {

    createResources();

    getDownloadParams();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    //handleDownloadNotification();

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

    EXPECT_EQ(downloadId.downloadId, "1001");
    #if 0
    downloadNotification.OnAppDownloadStatus(ndownloadId, nfileLocator, nreason);
    downloadSignal = downloadNotification.WaitForStatusSignal(TIMEOUT, PackageManager_AppDownloadStatus);
    EXPECT_TRUE(downloadSignal & PackageManager_AppDownloadStatus);   
    #endif
    string fileLocator = "/opt/CDL/package1001";

    // TC-28: Delete download failure when download in progress using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkgdownloaderInterface->Delete(fileLocator));

    // Unregister the notification
    //plugin->Unregister(static_cast<Exchange::IPackageDownloader::INotification*>(&downloadNotification));

    releaseResources();
}

/* Test Case for deleting failed using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the delete method using the COM RPC interface, passing fileLocator
 * Verify delete method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, deleteMethodusingComRpcFailure) {

    createResources();

    string fileLocator = "/opt/CDL/package1001";

    // TC-29: Failure in delete using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkgdownloaderInterface->Delete(fileLocator));

    releaseResources();	
}

/* Test Case for download progress via ID using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the progress method using the JSON RPC handler, passing the downloadId and progress info
 * Verify that the progress method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking that response is not empty string.
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, progressMethodusingJsonRpcSuccess) {

    createResources();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"uri\": \"http://test.com\", \"options\": {\"priority\": true, \"retries\": 2, \"rateLimit\": 1024}, \"downloadId\": {}}"), mJsonRpcResponse));

    EXPECT_NE(mJsonRpcResponse.find("1001"), std::string::npos);

    // TC-30: Download progress via downloadId using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("progress"), _T("{\"downloadId\": \"1001\", \"progress\": {}}"), mJsonRpcResponse));

    EXPECT_NE(mJsonRpcResponse, "");

    releaseResources();	
}

/* Test Case for download progress failure using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the progress method using the JSON RPC handler, passing downloadId and progress info
 * Verify progress method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, progressMethodusingJsonRpcFailure) {

    createResources();

    // TC-31: Download progress failure using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("progress"), _T("{\"downloadId\": \"testDownloadId\", \"progress\": {}}"), mJsonRpcResponse));

    releaseResources();
}

/* Test Case for download progress via ID using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the progress method using the COM RPC interface, passing the downloadId and progress info
 * Verify successful progress by asserting that it returns Core::ERROR_NONE and checking that progress is non-zero
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

 TEST_F(PackageManagerTest, progressMethodusingComRpcSuccess) {

    createResources();

    getDownloadParams();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    progress = {};

    //handleDownloadNotification();

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

    EXPECT_EQ(downloadId.downloadId, "1001");
    #if 0
    downloadNotification.OnAppDownloadStatus(ndownloadId, nfileLocator, nreason);
    downloadSignal = downloadNotification.WaitForStatusSignal(TIMEOUT, PackageManager_AppDownloadStatus);
    EXPECT_TRUE(downloadSignal & PackageManager_AppDownloadStatus);
    #endif
    // TC-32: Download progress via downloadId using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Progress(downloadId.downloadId, progress));

    EXPECT_NE(progress.progress, 0);

    // Unregister the notification
    //plugin->Unregister(static_cast<Exchange::IPackageDownloader::INotification*>(&downloadNotification));

    releaseResources();
}

/* Test Case for download progress failure using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the progress method using the COM RPC interface, passing downloadId and progress info
 * Verify progress method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, progressMethodusingComRpcFailure) {

    createResources();

    progress = {};

    string downloadId = "1001";

    // TC-33: Progress failure via downloadId using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkgdownloaderInterface->Progress(downloadId, progress));

    releaseResources();
}

/* Test Case for getting storage details using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the getStorageDetails method using the JSON RPC handler, passing required parameters
 * Verify getStorageDetails method success by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, getStorageDetailsusingJsonRpc) {

    createResources();

    // TC-34: Get Storage Details using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("getStorageDetails"), _T("{\"quotaKB\": \"1024\", \"usedKB\": \"568\"}"), mJsonRpcResponse));

    releaseResources();	
}

/* Test Case for getting storage details using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the getStorageDetails method using the COM RPC interface, passing required parameters
 * Verify getStorageDetails method success by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, getStorageDetailsusingComRpc) {

    createResources();

    string quotaKB = "1024";
    string usedKB = "568";

    // TC-35: Get Storage Details using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->GetStorageDetails(quotaKB, usedKB));

    releaseResources();
}

/* Test Case for setting rate limit via ID using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the rateLimit method using the JSON RPC handler, passing the downloadId and the limit
 * Verify that the rateLimit method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, rateLimitusingJsonRpcSuccess) {

    createResources();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"uri\": \"http://test.com\", \"options\": {\"priority\": true, \"retries\": 2, \"rateLimit\": 1024}, \"downloadId\": {}}"), mJsonRpcResponse));

    EXPECT_NE(mJsonRpcResponse.find("1001"), std::string::npos);

    // TC-36: Set rate limit via downloadID using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("rateLimit"), _T("{\"downloadId\": \"1001\", \"limit\": 1024}"), mJsonRpcResponse));

    releaseResources();	   
}

 /* Test Case for setting rate limit failure using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the rateLimit method using the JSON RPC handler, passing downloadId and limit
 * Verify rateLimit method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, rateLimitusingJsonRpcFailure) {

    createResources();

    // TC-38: Rate limit failure using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("rateLimit"), _T("{\"downloadId\": \"1001\", \"limit\": 1024}"), mJsonRpcResponse));

    releaseResources();
}

 /* Test Case for setting rate limit via ID using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the rateLimit method using the COM RPC interface, passing the downloadId and limit
 * Verify rateLimit is set successfully by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, rateLimitusingComRpcSuccess) {

    createResources();

    getDownloadParams();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    uint64_t limit = 1024;

    //handleDownloadNotification();

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

    EXPECT_EQ(downloadId.downloadId, "1001");
    #if 0
    downloadNotification.OnAppDownloadStatus(ndownloadId, nfileLocator, nreason);
    downloadSignal = downloadNotification.WaitForStatusSignal(TIMEOUT, PackageManager_AppDownloadStatus);
    EXPECT_TRUE(downloadSignal & PackageManager_AppDownloadStatus);
    #endif
    // TC-39: Set rate limit via downloadID using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->RateLimit(downloadId.downloadId, limit));
    
    // Unregister the notification
    //plugin->Unregister(static_cast<Exchange::IPackageDownloader::INotification*>(&downloadNotification));

    releaseResources();	
}

 /* Test Case for failure in setting rateLimit using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the rateLimit method using the COM RPC interface, passing downloadId and limit
 * Verify rateLimit method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

 TEST_F(PackageManagerTest, rateLimitusingComRpcFailure) {

    createResources();

    uint64_t limit = 1024;
    string downloadId = "1001";

    // TC-41: Rate limit failure using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkgdownloaderInterface->RateLimit(downloadId, limit));

    releaseResources();
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

    createResources();   

    // TC-42: Error on install due to invalid signature using JsonRpc
    EXPECT_EQ(Core::ERROR_INVALID_SIGNATURE, mJsonRpcHandler.Invoke(connection, _T("install"), _T("{\"packageId\": \"testPackage\", \"version\": \"2.0\", \"additionalMetadata\": {\"name\": \"testApp\", \"value\": \"2\", \"INTERFACE_ID\": 3}, \"fileLocator\": \"\", \"reason\": 1}"), mJsonRpcResponse));

    releaseResources();
}

/* Test Case for install failure using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the install method using the JSON RPC handler, passing the required parameters
 * Verify that the install method fails by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, installusingJsonRpcFailure) {

    createResources();   

    EXPECT_CALL(*mStorageManagerMock, CreateStorage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const uint32_t &size, string& path, string &errorReason) {
                return Core::ERROR_NONE;
            }));
    
    // TC-43: Failure on install using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("install"), _T("{\"packageId\": \"testPackage\", \"version\": \"2.0\", \"additionalMetadata\": {\"name\": \"testApp\", \"value\": \"2\", \"INTERFACE_ID\": 3}, \"fileLocator\": \"/opt/CDL/package1001\", \"reason\": 1}"), mJsonRpcResponse));

    releaseResources();
}

 /* Test Case for error on install due to invalid signature using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the install method using the COM RPC interface, passing required parameters, keeping the fileLocator parameter as empty
 * Verify error on install by asserting that it returns Core::ERROR_INVALID_SIGNATURE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, installusingComRpcInvalidSignature) {

    createResources();   

    string packageId = "testPackage";
    string version = "2.0";
    string fileLocator = "";
    Exchange::IPackageInstaller::FailReason reason = Exchange::IPackageInstaller::FailReason::NONE;
    list<Exchange::IPackageInstaller::KeyValue> kv = { {"testapp", "2"} };

    auto additionalMetadata = Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IKeyValueIterator>>::Create<Exchange::IPackageInstaller::IKeyValueIterator>(kv);

    // TC-44: Error on install due to invalid signature using ComRpc
    EXPECT_EQ(Core::ERROR_INVALID_SIGNATURE, pkginstallerInterface->Install(packageId, version, additionalMetadata, fileLocator, reason));

    releaseResources();
}

/* Test Case for install failure using ComRpc
 * 
 * Set up and initialize required COM-RPC resources, configurations, mocks, notifications and expectations
 * Register the notification using the COM RPC interface and set the status params via the SetStatusParams()
 * Call the install method using the COM RPC interface, passing required parameters
 * Verify that the install method fails by asserting that it returns Core::ERROR_GENERAL
 * Signal the onAppInstallationStatus method, passing the packageId and version as arguments and wait
 * Unregister the notification using the COM RPC interface
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

 TEST_F(PackageManagerTest, installusingComRpcFailure) {

    createResources();   

    Core::Sink<NotificationTest> notification;
    uint32_t signal = PackageManager_invalidStatus;

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

    // Initialize the json string
    JsonObject obj;
    obj["packageId"] = packageId;
    obj["version"] = version;

    JsonArray list;
    list.Add(obj);
    string jsonstr;
    list.ToString(jsonstr);

    // Register the notification
    mPackageManagerImpl->Register(&notification);

    EXPECT_CALL(*mStorageManagerMock, CreateStorage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const uint32_t &size, string& path, string &errorReason) {
                return Core::ERROR_NONE;
            }));

    // TC-45: Failure on install using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkginstallerInterface->Install(packageId, version, additionalMetadata, fileLocator, reason));

    signal = notification.WaitForStatusSignal(TIMEOUT, PackageManager_invalidStatus);
    notification.SetStatusParams(statusParams);
    notification.OnAppInstallationStatus(jsonstr);
    signal = notification.WaitForStatusSignal(TIMEOUT, PackageManager_AppInstallStatus);
    EXPECT_TRUE(signal & PackageManager_AppInstallStatus);
    signal = PackageManager_invalidStatus;
    notification.OnAppInstallationStatus(jsonstr);
    signal = notification.WaitForStatusSignal(TIMEOUT, PackageManager_AppInstallStatus);
    EXPECT_TRUE(signal & PackageManager_AppInstallStatus);

    // Unregister the notification
    mPackageManagerImpl->Unregister(&notification);

    releaseResources();
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

    createResources();   

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

    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("install"), _T("{\"packageId\": \"testPackage\", \"version\": \"2.0\", \"additionalMetadata\": {\"name\": \"testApp\", \"value\": \"2\", \"INTERFACE_ID\": 3}, \"fileLocator\": \"/opt/CDL/package1001\", \"reason\": 1}"), mJsonRpcResponse));

    // TC-46: Failure on uninstall using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("uninstall"), _T("{\"packageId\": \"testPackage\", \"errorReason\": \"no error\"}"), mJsonRpcResponse));

    releaseResources();
}

/* Test Case for uninstall failure using ComRpc
 * 
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Register the notification using the COM RPC interface and set the status params via the SetStatusParams()
 * Call the install method using the COM RPC interface, passing required parameters
 * Verify that the install method fails by asserting that it returns Core::ERROR_GENERAL
 * Signal the onAppInstallationStatus method, passing the packageId and version as arguments and wait
 * Call the uninstall method using the COM RPC interface, passing required parameters
 * Verify that the uninstall method fails by asserting that it returns Core::ERROR_GENERAL
 * Signal the onAppInstallationStatus method, passing the packageId and version as arguments and wait
 * Unregister the notification using the COM RPC interface
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, uninstallusingComRpcFailure) {

    createResources();   

    Core::Sink<NotificationTest> notification;
    uint32_t signal = PackageManager_invalidStatus;

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

    // Initialize the json string
    JsonObject obj;
    obj["packageId"] = packageId;
    obj["version"] = version;

    JsonArray list;
    list.Add(obj);
    string jsonstr;
    list.ToString(jsonstr);

    // Register the notification
    mPackageManagerImpl->Register(&notification);
    
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
    
    signal = notification.WaitForStatusSignal(TIMEOUT, PackageManager_invalidStatus);
    notification.SetStatusParams(statusParams);
    notification.OnAppInstallationStatus(jsonstr);
    signal = notification.WaitForStatusSignal(TIMEOUT, PackageManager_AppInstallStatus);
    EXPECT_TRUE(signal & PackageManager_AppInstallStatus);
    signal = PackageManager_invalidStatus;
    notification.OnAppInstallationStatus(jsonstr);
    signal = notification.WaitForStatusSignal(TIMEOUT, PackageManager_AppInstallStatus);
    EXPECT_TRUE(signal & PackageManager_AppInstallStatus);
    signal = PackageManager_invalidStatus;

	// TC-47: Failure on uninstall using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkginstallerInterface->Uninstall(packageId, errorReason));
    
    signal = notification.WaitForStatusSignal(TIMEOUT, PackageManager_invalidStatus);
    notification.OnAppInstallationStatus(jsonstr);
    signal = notification.WaitForStatusSignal(TIMEOUT, PackageManager_AppInstallStatus);
    EXPECT_TRUE(signal & PackageManager_AppInstallStatus);
    signal = PackageManager_invalidStatus;
    notification.OnAppInstallationStatus(jsonstr);
    signal = notification.WaitForStatusSignal(TIMEOUT, PackageManager_AppInstallStatus);
    EXPECT_TRUE(signal & PackageManager_AppInstallStatus);

    // Unregister the notification
    mPackageManagerImpl->Unregister(&notification);

    releaseResources();
}

/* Test Case for list packages method success using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the listPackages method using the JSON RPC handler, passing the required parameters
 * Verify that the listPackages method is successful by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, listPackagesusingJsonRpcSuccess) {

    createResources();   

	// TC-48: list packages using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("listPackages"), _T("{\"packages\": {}}"), mJsonRpcResponse));

    releaseResources();
}

/* Test Case for list packages method success using ComRpc
 * 
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the ListPackages method using the COM RPC interface, passing the required parameters
 * Verify that the ListPackages method is successful by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, listPackagesusingComRpcSuccess) {

    createResources();   

    list<Exchange::IPackageInstaller::Package> packageList = { {} };

    auto packages = Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IPackageIterator>>::Create<Exchange::IPackageInstaller::IPackageIterator>(packageList);

	// TC-49: list packages using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkginstallerInterface->ListPackages(packages));

    releaseResources();
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

TEST_F(PackageManagerTest, configMethodusingJsonRpcFailure) {

    createResources();   

    EXPECT_CALL(*mStorageManagerMock, CreateStorage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const uint32_t &size, string& path, string &errorReason) {
                return Core::ERROR_NONE;
            }));

    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("install"), _T("{\"packageId\": \"testPackage\", \"version\": \"2.0\", \"additionalMetadata\": {\"name\": \"testApp\", \"value\": \"2\", \"INTERFACE_ID\": 3}, \"fileLocator\": \"/opt/CDL/package1001\", \"reason\": 1}"), mJsonRpcResponse));

    // TC-50: Failure in config using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("config"), _T("{\"packageId\": \"testPackage\", \"version\": \"2.0\", \"configMetadata\": {}}"), mJsonRpcResponse)); 
    
    releaseResources();
}

/* Test Case for config method failure using ComRpc
 * 
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the install method using the COM RPC interface, passing the required parameters
 * Verify install method failure by asserting that it returns Core::ERROR_GENERAL
 * Signal the onAppInstallationStatus method, passing the packageId and version as arguments and wait
 * Call the config method using the COM RPC interface, passing the required parameters
 * Verify config method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, configMethodusingComRpcFailure) {

    createResources();   

    Core::Sink<NotificationTest> notification;
    uint32_t signal = PackageManager_invalidStatus;

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

    // Initialize the json string
    JsonObject obj;
    obj["packageId"] = packageId;
    obj["version"] = version;

    JsonArray list;
    list.Add(obj);
    string jsonstr;
    list.ToString(jsonstr);

    // Register the notification
    mPackageManagerImpl->Register(&notification);

    EXPECT_CALL(*mStorageManagerMock, CreateStorage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const uint32_t &size, string& path, string &errorReason) {
                return Core::ERROR_NONE;
            }));

    EXPECT_EQ(Core::ERROR_GENERAL, pkginstallerInterface->Install(packageId, version, additionalMetadata, fileLocator, reason));
    
    signal = notification.WaitForStatusSignal(TIMEOUT, PackageManager_invalidStatus);
    notification.SetStatusParams(statusParams);
    notification.OnAppInstallationStatus(jsonstr);
    signal = notification.WaitForStatusSignal(TIMEOUT, PackageManager_AppInstallStatus);
    EXPECT_TRUE(signal & PackageManager_AppInstallStatus);
    signal = PackageManager_invalidStatus;
    notification.OnAppInstallationStatus(jsonstr);
    signal = notification.WaitForStatusSignal(TIMEOUT, PackageManager_AppInstallStatus);
    EXPECT_TRUE(signal & PackageManager_AppInstallStatus);
    signal = PackageManager_invalidStatus;

    // TC-51: Failure in config using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkginstallerInterface->Config(packageId, version, runtimeConfig));

    signal = notification.WaitForStatusSignal(TIMEOUT, PackageManager_invalidStatus);

    // Unregister the notification
    mPackageManagerImpl->Unregister(&notification);

    releaseResources();	
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

TEST_F(PackageManagerTest, packageStateusingJsonRpcFailure) {

    createResources();   

    EXPECT_CALL(*mStorageManagerMock, CreateStorage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const uint32_t &size, string& path, string &errorReason) {
                return Core::ERROR_NONE;
            }));

    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("install"), _T("{\"packageId\": \"testPackage\", \"version\": \"2.0\", \"additionalMetadata\": {\"name\": \"testApp\", \"value\": \"2\", \"INTERFACE_ID\": 3}, \"fileLocator\": \"/opt/CDL/package1001\", \"reason\": 1}"), mJsonRpcResponse));

    // TC-52: Failure in package state using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("packageState"), _T("{\"packageId\": \"testPackage\", \"version\": \"2.0\", \"installState\": 0}"), mJsonRpcResponse));

    releaseResources();
}

/* Test Case for package state failure using ComRpc
 * 
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the install method using the COM RPC interface, passing the required parameters
 * Verify install method failure by asserting that it returns Core::ERROR_GENERAL
 * Signal the onAppInstallationStatus method, passing the packageId and version as arguments and wait
 * Call the PackageState method using the COM RPC interface, passing the required parameters
 * Verify package state method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, packageStateusingComRpcFailure) {

    createResources();   

    Core::Sink<NotificationTest> notification;
    uint32_t signal = PackageManager_invalidStatus;

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

    // Initialize the json string
    JsonObject obj;
    obj["packageId"] = packageId;
    obj["version"] = version;

    JsonArray list;
    list.Add(obj);
    string jsonstr;
    list.ToString(jsonstr);

    // Register the notification
    mPackageManagerImpl->Register(&notification);

    EXPECT_CALL(*mStorageManagerMock, CreateStorage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const uint32_t &size, string& path, string &errorReason) {
                return Core::ERROR_NONE;
            }));

    EXPECT_EQ(Core::ERROR_GENERAL, pkginstallerInterface->Install(packageId, version, additionalMetadata, fileLocator, reason));

    signal = notification.WaitForStatusSignal(TIMEOUT, PackageManager_invalidStatus);
    notification.SetStatusParams(statusParams);
    notification.OnAppInstallationStatus(jsonstr);
    signal = notification.WaitForStatusSignal(TIMEOUT, PackageManager_AppInstallStatus);
    EXPECT_TRUE(signal & PackageManager_AppInstallStatus);
    signal = PackageManager_invalidStatus;
    notification.OnAppInstallationStatus(jsonstr);
    signal = notification.WaitForStatusSignal(TIMEOUT, PackageManager_AppInstallStatus);
    EXPECT_TRUE(signal & PackageManager_AppInstallStatus);
    signal = PackageManager_invalidStatus;

    // TC-53: Failure in package state using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkginstallerInterface->PackageState(packageId, version, state));
    
    signal = notification.WaitForStatusSignal(TIMEOUT, PackageManager_invalidStatus);
    
    // Unregister the notification
    mPackageManagerImpl->Unregister(&notification);

    releaseResources();
}

 /* Test Case for get config for package error due to invalid signature using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the getConfigforPackages method using the JSON RPC handler, passing the required parameters, keeping the file locator field empty
 * Verify getConfigforPackages method error by asserting that it returns Core::ERROR_INVALID_SIGNATURE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, getConfigforPackageusingJsonRpcInvalidSignature) {

    createResources();   

    // TC-54: Error in get config for packages due to empty file locator using JsonRpc
    EXPECT_EQ(Core::ERROR_INVALID_SIGNATURE, mJsonRpcHandler.Invoke(connection, _T("getConfigForPackage"), _T("{\"fileLocator\": \"\", \"id\": \"1001\", \"version\": \"2.0\", \"config\": {}}"), mJsonRpcResponse));

    releaseResources();
}

/* Test Case for get config for package failure using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the getConfigforPackages method using the JSON RPC handler, passing the required parameters
 * Verify that the getConfigforPackages method fails by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, getConfigforPackageusingJsonRpcFailure) {

    createResources();   

    // TC-55: Failure in get config for packages using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("getConfigForPackage"), _T("{\"fileLocator\": \"/opt/CDL/package1001\", \"id\": \"1001\", \"version\": \"2.0\", \"config\": {}}"), mJsonRpcResponse));

    releaseResources();
}

/* Test Case for get config for packages error due to invalid signature using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the GetConfigforPackage method using the COM RPC interface, passing required parameters, keeping the fileLocator parameter as empty
 * Verify GetConfigforPackage error by asserting that it returns Core::ERROR_INVALID_SIGNATURE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, getConfigforPackageusingComRpcInvalidSignature) {

    createResources();   

    string fileLocator = "";
    string id = "1001";
    string version = "2.0";

    Exchange::RuntimeConfig config = {};

    // TC-56: Error in get config for packages due to empty file locator using ComRpc
    EXPECT_EQ(Core::ERROR_INVALID_SIGNATURE, pkginstallerInterface->GetConfigForPackage(fileLocator, id, version, config));

    releaseResources();
}

/* Test Case for get config for package failure using ComRpc
 * 
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the GetConfigforPackage method using the COM RPC interface, passing required parameters
 * Verify that the GetConfigforPackage method fails by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, getConfigforPackageusingComRpcFailure) {

    createResources();   
    
    string fileLocator = "/opt/CDL/package1001";
    string id = "1001";
    string version = "2.0";

    Exchange::RuntimeConfig config = {};

    // TC-57: Failure in get config for packages using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkginstallerInterface->GetConfigForPackage(fileLocator, id, version, config));

    releaseResources();
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

    createResources();   

    // TC-58: Error on lock using JsonRpc
    EXPECT_EQ(Core::ERROR_BAD_REQUEST, mJsonRpcHandler.Invoke(connection, _T("lock"), _T("{\"packageId\": \"testPackage\", \"version\": \"2.0\", \"lockReason\": 0, \"lockId\": 132, \"unpackedPath\": \"testPath\", \"configMetadata\": {}, \"appMetadata\": {}}"), mJsonRpcResponse));

    releaseResources();
}

/* Test Case for lock error using ComRpc
 * 
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the Lock method using the COM RPC interface, passing required parameters
 * Verify Lock method error by asserting that it returns Core::ERROR_BAD_REQUEST
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, lockmethodusingComRpcError) {

    createResources();   

    string packageId = "testPackage";
    string version = "2.0";
    Exchange::IPackageHandler::LockReason lockReason = Exchange::IPackageHandler::LockReason::SYSTEM_APP;
    uint32_t lockId = 132;
    string unpackedPath = "testPath";
    Exchange::RuntimeConfig configMetadata = {};
    list<Exchange::IPackageHandler::AdditionalLock> additionalLock = { {} };

    auto appMetadata = Core::Service<RPC::IteratorType<Exchange::IPackageHandler::ILockIterator>>::Create<Exchange::IPackageHandler::ILockIterator>(additionalLock);

	// TC-59: Error on lock using ComRpc
    EXPECT_EQ(Core::ERROR_BAD_REQUEST, pkghandlerInterface->Lock(packageId, version, lockReason, lockId, unpackedPath, configMetadata, appMetadata));

    releaseResources();	
}

/* Test Case for unlock error using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the unlock method using the JSON RPC handler, passing the required parameters
 * Verify unlock method error by asserting that it returns Core::ERROR_BAD_REQUEST
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, unlockmethodusingJsonRpcError) {

    createResources();   

	// TC-60: Error on unlock using JsonRpc
    EXPECT_EQ(Core::ERROR_BAD_REQUEST, mJsonRpcHandler.Invoke(connection, _T("unlock"), _T("{\"packageId\": \"testPackage\", \"version\": \"2.0\"}"), mJsonRpcResponse));

    releaseResources();	
}

/* Test Case for unlock error using ComRpc
 * 
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the Unlock method using the COM RPC interface, passing required parameters
 * Verify Unlock method error by asserting that it returns Core::ERROR_BAD_REQUEST
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, unlockmethodusingComRpcError) {

    createResources();   

    string packageId = "testPackage";
    string version = "2.0";

    // TC-61: Error on unlock using ComRpc
    EXPECT_EQ(Core::ERROR_BAD_REQUEST, pkghandlerInterface->Unlock(packageId, version));

    releaseResources();
}

/* Test Case for get locked info error using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the getLockedInfo method using the JSON RPC handler, passing the required parameters
 * Verify getLockedInfo method error by asserting that it returns Core::ERROR_BAD_REQUEST
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, getLockedInfousingJsonRpcError) {

    createResources();   

    // TC-62: Error on get locked info using JsonRpc
    EXPECT_EQ(Core::ERROR_BAD_REQUEST, mJsonRpcHandler.Invoke(connection, _T("getLockedInfo"), _T("{\"packageId\": \"testPackage\", \"version\": \"2.0\", \"unpackedPath\": \"testPath\", \"configMetadata\": {}, \"gatewayMetadataPath\": \"testgatewayMetadataPath\", \"locked\": true}"), mJsonRpcResponse));

    releaseResources();
}

/* Test Case for get locked info error using ComRpc
 * 
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the GetLockedInfo method using the COM RPC interface, passing required parameters
 * Verify GetLockedInfo method error by asserting that it returns Core::ERROR_BAD_REQUEST
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, getLockedInfousingComRpcError) {

    createResources();   

    string packageId = "testPackage";
    string version = "2.0";
    string unpackedPath = "testPath";
    Exchange::RuntimeConfig configMetadata = {};
    string gatewayMetadataPath = "testgatewayMetadataPath";
    bool locked = true;

    // TC-63: Error on get locked info using ComRpc
    EXPECT_EQ(Core::ERROR_BAD_REQUEST, pkghandlerInterface->GetLockedInfo(packageId, version, unpackedPath, configMetadata, gatewayMetadataPath, locked));

    releaseResources();	
}
