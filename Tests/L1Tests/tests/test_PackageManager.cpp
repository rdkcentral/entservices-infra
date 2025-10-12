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

using ::testing::NiceMock;
using namespace WPEFramework;
using namespace std;

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
    
    // Timeout utility function (waits for 1 second)
        void waitForTimeout() {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

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

        EXPECT_CALL(*mServiceMock, SubSystems())
          .Times(::testing::AnyNumber())
          .WillRepeatedly(::testing::Return(mSubSystemMock));

		EXPECT_CALL(*mServiceMock, AddRef())
          .Times(::testing::AnyNumber());
    }

    void initforJsonRpc() 
    {    
        // Activate the dispatcher and initialize the plugin for JSON-RPC
        PluginHost::IFactories::Assign(&factoriesImplementation);
        dispatcher = static_cast<PLUGINHOST_DISPATCHER*>(plugin->QueryInterface(PLUGINHOST_DISPATCHER_ID));
        dispatcher->Activate(mServiceMock);
        plugin->Initialize(mServiceMock);
    }

    void initforComRpc() 
    {
        // Initialize the plugin for COM-RPC
        pkgdownloaderInterface->Initialize(mServiceMock);
    }

    void getDownloadParams()
    {
        // Initialize the parameters required for COM-RPC with default values
        uri = "http://test.com";

        options = { 
            true,2,1024
        };

        downloadId = {};
    }

    void releaseResources()
    {
	    // Clean up mocks
		if (mServiceMock != nullptr)
        {
			EXPECT_CALL(*mServiceMock, Release())
              .WillOnce(::testing::Invoke(
              [&]() {
						delete mServiceMock;
						mServiceMock = nullptr;
						return 0;
					}));    
        }

        if (mStorageManagerMock != nullptr)
        {
			EXPECT_CALL(*mStorageManagerMock, Release())
              .WillOnce(::testing::Invoke(
              [&]() {
						delete mStorageManagerMock;
						mStorageManagerMock = nullptr;
						return 0;
					}));
        }
    }

    void deinitforJsonRpc() 
    {
        // Deactivate the dispatcher and deinitialize the plugin for JSON-RPC
        dispatcher->Deactivate();
        dispatcher->Release();

        plugin->Deinitialize(mServiceMock);
    }

    void deinitforComRpc()
    {
        // Deinitialize the plugin for COM-RPC
        pkgdownloaderInterface->Deinitialize(mServiceMock);
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

    initforJsonRpc();

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
	
    releaseResources();
}

/* Test Case for adding download request to a queue(priority/regular) using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters as json string and setting priority as true
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the download method using the JSON RPC handler, passing the required parameters as json string and setting priority as false
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, downloadMethodusingJsonRpcSuccess) {
    
    createResources();

    initforJsonRpc();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));
    
    // TC-1: Add download request to priority queue using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"uri\": \"http://test.com\", \"options\": {\"priority\": true, \"retries\": 2, \"rateLimit\": 1024}, \"downloadId\": {}}"), mJsonRpcResponse));

    EXPECT_NE(mJsonRpcResponse.find("1001"), std::string::npos);

    // TC-2: Add download request to regular queue using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"uri\": \"http://test.com\", \"options\": {\"priority\": false, \"retries\": 2, \"rateLimit\": 1024}, \"downloadId\": {\"testDownloadId\"}}"), mJsonRpcResponse));

    EXPECT_NE(mJsonRpcResponse.find("1002"), std::string::npos);

    deinitforJsonRpc();

    releaseResources();
}

/* Test Case for checking download request error when internet is unavailable using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the method using the JSON RPC handler, passing the required parameters as json string and setting priority as true
 * Verify download method error due to unavailability of internet by asserting that it returns Core::ERROR_UNAVAILABLE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, downloadMethodusingJsonRpcError) {
    
    createResources();

    initforJsonRpc();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return false;
            }));
    
    // TC-3: Download request error when internet is unavailable using JsonRpc
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"uri\": \"http://test.com\", \"options\": {\"priority\": true, \"retries\": 2, \"rateLimit\": 1024}, \"downloadId\": {\"testDownloadId\"}}"), mJsonRpcResponse));

    deinitforJsonRpc();

    releaseResources();
}

/* Test Case for adding download request to a queue(priority/regular) using ComRpc
 * 
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the Download method using the COM RPC interface again along with the required parameters, setting priority as false
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, downloadMethodsusingComRpcSuccess) {

    createResources();

    initforComRpc();

    getDownloadParams();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type){
                return true;
            }));
    
    // TC-4: Add download request to priority queue using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

    EXPECT_EQ(downloadId.downloadId, "1001");

    options.priority = false;

    // TC-5: Add download request to regular queue using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

    EXPECT_EQ(downloadId.downloadId, "1002");

	deinitforComRpc();
	
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

    initforComRpc();

    getDownloadParams();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type){
                return false;
            }));
    
    // TC-6: Download request error when internet is unavailable using ComRpc
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, pkgdownloaderInterface->Download(uri, options, downloadId));

	deinitforComRpc();
	
    releaseResources();
}

/* Test Case for pausing download via ID using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters as json string and setting priority as true
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the pause method using the JSON RPC handler, passing the downloadId
 * Verify that the pause method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, pauseMethodusingJsonRpcSuccess) {

    createResources();

    initforJsonRpc();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));
    
    
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"uri\": \"http://test.com\", \"options\": {\"priority\": true, \"retries\": 2, \"rateLimit\": 1024}, \"downloadId\": {}}"), mJsonRpcResponse));
    
    EXPECT_NE(mJsonRpcResponse.find("1001"), std::string::npos);

    // TC-7: Pause download via downloadId using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("pause"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
	
    releaseResources();
}

/* Test Case for error while pausing download due to mismatch in ID using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters as json string and setting priority as true
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the pause method using the JSON RPC handler, passing different downloadId
 * Verify pause method error by asserting that it returns Core::ERROR_UNKNOWN_KEY
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, pauseMethodusingJsonRpcError) {

    createResources();

    initforJsonRpc();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"uri\": \"http://test.com\", \"options\": {\"priority\": true, \"retries\": 2, \"rateLimit\": 1024}, \"downloadId\": {}}"), mJsonRpcResponse));

    EXPECT_NE(mJsonRpcResponse.find("1001"), std::string::npos);

    // TC-8: Error while pausing download via different downloadId using JsonRpc
    EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, mJsonRpcHandler.Invoke(connection, _T("pause"), _T("{\"downloadId\": \"1002\"}"), mJsonRpcResponse));

    deinitforJsonRpc();

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

    initforJsonRpc();

    // TC-9: Failure in pausing download using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("pause"), _T("{\"downloadId\": \"testDownloadId\"}"), mJsonRpcResponse));

    deinitforJsonRpc();

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

    initforComRpc();

    getDownloadParams();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));
    
    
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

    EXPECT_EQ(downloadId.downloadId, "1001");

    // TC-10: Pause download via downloadId using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Pause(downloadId.downloadId));

	deinitforComRpc();

    releaseResources();
}

/* Test Case for error while pausing download due to mismatch in ID using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the pause method using the COM RPC interface, passing different downloadId
 * Verify error while pausing by asserting that it returns Core::ERROR_UNKNOWN_KEY
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, pauseMethodusingComRpcError) {

    createResources();

    initforComRpc();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

    EXPECT_EQ(downloadId.downloadId, "1001");

    string downloadId = "1002";

    // TC-11: Error while pausing download via different downloadId using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Pause(downloadId));

    deinitforComRpc();

    releaseResources();
}

/* Test Case for pausing failed using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the pause method using the COM RPC interface, passing downloadId
 * Verify pause method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, pauseMethodusingComRpcFailure) {

    createResources();

    initforComRpc();

    string downloadId = "testDownloadId";

    // TC-12: Failure in pausing download using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkgdownloaderInterface->Pause(downloadId));

	deinitforComRpc();
	
    releaseResources();
}

/* Test Case for resuming download via ID using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters as json string and setting priority as true
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the resume method using the JSON RPC handler, passing the downloadId
 * Verify that the resume method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, resumeMethodusingJsonRpcSuccess) {

    createResources();

    initforJsonRpc();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"uri\": \"http://test.com\", \"options\": {\"priority\": true, \"retries\": 2, \"rateLimit\": 1024}, \"downloadId\": {}}"), mJsonRpcResponse));

    EXPECT_NE(mJsonRpcResponse.find("1001"), std::string::npos);

    // TC-13: Resume download via downloadId using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("resume"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
	
    releaseResources();
}

 /* Test Case for error while resuming download due to mismatch in ID using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters as json string and setting priority as true
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the resume method using the JSON RPC handler, passing different downloadId
 * Verify resume method error by asserting that it returns Core::ERROR_UNKNOWN_KEY
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

 TEST_F(PackageManagerTest, resumeMethodusingJsonRpcError) {

    createResources();

    initforJsonRpc();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"uri\": \"http://test.com\", \"options\": {\"priority\": true, \"retries\": 2, \"rateLimit\": 1024}, \"downloadId\": {}}"), mJsonRpcResponse));

    EXPECT_NE(mJsonRpcResponse.find("1001"), std::string::npos);

    // TC-14: Error while resuming download via different downloadId using JsonRpc
    EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, mJsonRpcHandler.Invoke(connection, _T("resume"), _T("{\"downloadId\": \"1002\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
	
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

    initforJsonRpc();

    // TC-15: Failure in resuming download using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("resume"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
	
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

    initforComRpc();

    getDownloadParams();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

   	EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

    EXPECT_EQ(downloadId.downloadId, "1001");

    // TC-16: Resume download via downloadId using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Resume(downloadId.downloadId));

	deinitforComRpc();
	
    releaseResources();
}

 /* Test Case for error while resuming download due to mismatch in ID using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the resume method using the COM RPC interface, passing different downloadId
 * Verify error while resuming by asserting that it returns Core::ERROR_UNKNOWN_KEY
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

 TEST_F(PackageManagerTest, resumeMethodusingComRpcError) {

    createResources();

    initforComRpc();

    getDownloadParams();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

   	EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

    EXPECT_EQ(downloadId.downloadId, "1001");

    string downloadId = "1002";

    // TC-17: Error while resuming download via different downloadId using ComRpc
    EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, pkgdownloaderInterface->Resume(downloadId));

	deinitforComRpc();
	
    releaseResources();
}

 /* Test Case for resuming failed using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the resume method using the COM RPC interface, passing downloadId
 * Verify resume method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

  TEST_F(PackageManagerTest, resumeMethodusingComRpcFailure) {

    createResources();

    initforComRpc();

    string downloadId = "testDownloadId";

    // TC-18: Failure in resuming download using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkgdownloaderInterface->Resume(downloadId));

	deinitforComRpc();
	
    releaseResources();
}

/* Test Case for cancelling download via ID using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters as json string and setting priority as true
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the cancel method using the JSON RPC handler, passing the downloadId
 * Verify that the cancel method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, cancelMethodusingJsonRpcSuccess) {

    createResources();

    initforJsonRpc();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"uri\": \"http://test.com\", \"options\": {\"priority\": true, \"retries\": 2, \"rateLimit\": 1024}, \"downloadId\": {}}"), mJsonRpcResponse));

    EXPECT_NE(mJsonRpcResponse.find("1001"), std::string::npos);

    // TC-19: Cancel download via downloadId using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("cancel"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
	
    releaseResources();
}

 /* Test Case for error while cancelling download due to mismatch in ID using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters as json string and setting priority as true
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the cancel method using the JSON RPC handler, passing different downloadId
 * Verify cancel method error by asserting that it returns Core::ERROR_UNKNOWN_KEY
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, cancelMethodusingJsonRpcError) {

    createResources();

    initforJsonRpc();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"uri\": \"http://test.com\", \"options\": {\"priority\": true, \"retries\": 2, \"rateLimit\": 1024}, \"downloadId\": {}}"), mJsonRpcResponse));

    EXPECT_NE(mJsonRpcResponse.find("1001"), std::string::npos);

    // TC-20: Error while cancelling download via different downloadId using JsonRpc
    EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, mJsonRpcHandler.Invoke(connection, _T("cancel"), _T("{\"downloadId\": \"1002\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
	
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

    initforJsonRpc();

    // TC-21: Failure in cancelling download using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("cancel"), _T("{\"downloadId\": \"1002\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
	
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

    initforComRpc();

    getDownloadParams();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

    EXPECT_EQ(downloadId.downloadId, "1001");

    // TC-22: Cancel download via downloadId using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Cancel(downloadId.downloadId));

	deinitforComRpc();
	
    releaseResources();
}

 /* Test Case for error while cancelling download due to mismatch in ID using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the cancel method using the COM RPC interface, passing different downloadId
 * Verify error while cancelling by asserting that it returns Core::ERROR_UNKNOWN_KEY
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

 TEST_F(PackageManagerTest, cancelMethodusingComRpcError) {

    createResources();

    initforComRpc();

    getDownloadParams();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

    EXPECT_EQ(downloadId.downloadId, "1001");

    string downloadId = "1002";

    // TC-23: Error while cancelling download via different downloadId using ComRpc
    EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, pkgdownloaderInterface->Cancel(downloadId));

	deinitforComRpc();
	
    releaseResources();
}

 /* Test Case for cancelling failed using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the cancel method using the COM RPC interface, passing downloadId
 * Verify cancel method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, cancelMethodusingComRpcFailure) {

    createResources();

    initforComRpc();

    string downloadId = "testDownloadId";

    // TC-24: Failure in cancelling download using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkgdownloaderInterface->Cancel(downloadId));

	deinitforComRpc();
	
    releaseResources();
}

/* Test Case for delete download failure when download in progress using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters as json string and setting priority as true
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the delete method using the JSON RPC handler, passing the fileLocator
 * Verify failure in delete by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, deleteMethodusingJsonRpcInProgress) {

    createResources();

    initforJsonRpc();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"uri\": \"http://test.com\", \"options\": {\"priority\": true, \"retries\": 2, \"rateLimit\": 1024}, \"downloadId\": {}}"), mJsonRpcResponse));

    EXPECT_NE(mJsonRpcResponse.find("1001"), std::string::npos);

    // TC-25: Delete download failure when download in progress using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("delete"), _T("{\"fileLocator\": \"/opt/CDL/package1001\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
	
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

    initforJsonRpc();

    // TC-26: Failure in delete using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("delete"), _T("{\"fileLocator\": \"/opt/CDL/package1001\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
	
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

TEST_F(PackageManagerTest, deleteMethodusingComRpcInProgress) {

    createResources();

    initforComRpc();

    getDownloadParams();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

    EXPECT_EQ(downloadId.downloadId, "1001");

    string fileLocator = "/opt/CDL/package1001";

    // TC-27: Delete download failure when download in progress using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkgdownloaderInterface->Delete(fileLocator));

	deinitforComRpc();
	
    releaseResources();
}

 /* Test Case for deleting failed using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the delete method using the COM RPC interface, passing fileLocator
 * Verify delete method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, deleteMethodusingComRpcFailure) {

    createResources();

    initforComRpc();

    string fileLocator = "/opt/CDL/packagetestDownloadId";

    // TC-28: Failure in delete using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkgdownloaderInterface->Delete(fileLocator));

	deinitforComRpc();
	
    releaseResources();
}

/* Test Case for download progress via ID using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters as json string and setting priority as true
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the progress method using the JSON RPC handler, passing the downloadId and progress info
 * Verify that the progress method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, progressMethodusingJsonRpcSuccess) {

    createResources();

    initforJsonRpc();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"uri\": \"http://test.com\", \"options\": {\"priority\": true, \"retries\": 2, \"rateLimit\": 1024}, \"downloadId\": {}}"), mJsonRpcResponse));

    EXPECT_NE(mJsonRpcResponse.find("1001"), std::string::npos);

    // TC-29: Download progress via downloadId using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("progress"), _T("{\"downloadId\": \"1001\", \"progress\": {\"progress\": 0}}"), mJsonRpcResponse));

	deinitforJsonRpc();
	
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

    initforJsonRpc();

    // TC-30: Download progress failure using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("progress"), _T("{\"downloadId\": \"testDownloadId\", \"progress\": {\"progress\": 0}}"), mJsonRpcResponse));

	deinitforJsonRpc();
	
    releaseResources();
}

 /* Test Case for download progress via ID using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the progress method using the COM RPC interface, passing the downloadId and progress info
 * Verify successful progress by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

 TEST_F(PackageManagerTest, progressMethodusingComRpcSuccess) {

    createResources();

    initforComRpc();

    getDownloadParams();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    progress = {
        0
    };

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

    EXPECT_EQ(downloadId.downloadId, "1001");

    // TC-31: Download progress via downloadId using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Progress(downloadId.downloadId, progress));

	deinitforComRpc();
	
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

    initforComRpc();

    progress = {
        0
    };

    string downloadId = "testDownloadId";

    // TC-32: Progress failure via downloadId using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkgdownloaderInterface->Progress(downloadId, progress));

	deinitforComRpc();
	
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

    initforJsonRpc();

    // TC-33: Get Storage Details using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("getStorageDetails"), _T("{\"quotaKB\": \"1024\", \"usedKB\": \"568\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
	
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

    initforComRpc();

    string quotaKB = "1024";
    string usedKB = "568";

    // TC-34: Get Storage Details using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->GetStorageDetails(quotaKB, usedKB));

	deinitforComRpc();
	
    releaseResources();
}

/* Test Case for setting rate limit via ID using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters as json string and setting priority as true
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the rateLimit method using the JSON RPC handler, passing the downloadId and the limit
 * Verify that the rateLimit method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, rateLimitusingJsonRpcSuccess) {

    createResources();

    initforJsonRpc();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"uri\": \"http://test.com\", \"options\": {\"priority\": true, \"retries\": 2, \"rateLimit\": 1024}, \"downloadId\": {}}"), mJsonRpcResponse));

    EXPECT_NE(mJsonRpcResponse.find("1001"), std::string::npos);

    // TC-35: Set rate limit via downloadID using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("rateLimit"), _T("{\"downloadId\": \"1001\", \"limit\": 1024}"), mJsonRpcResponse));

	deinitforJsonRpc();
	
    releaseResources();
}

 /* Test Case for error while setting rate limit due to mismatch in ID using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters as json string and setting priority as true
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the rateLimit method using the JSON RPC handler, passing different downloadId and limit
 * Verify rateLimit method error by asserting that it returns Core::ERROR_UNKNOWN_KEY
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

 TEST_F(PackageManagerTest, rateLimitusingJsonRpcError) {

    createResources();

    initforJsonRpc();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"uri\": \"http://test.com\", \"options\": {\"priority\": true, \"retries\": 2, \"rateLimit\": 1024}, \"downloadId\": {}}"), mJsonRpcResponse));

    EXPECT_NE(mJsonRpcResponse.find("1001"), std::string::npos);

    // TC-36: Rate limit error when passing different downloadID using JsonRpc
    EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, mJsonRpcHandler.Invoke(connection, _T("rateLimit"), _T("{\"downloadId\": \"1002\", \"limit\": 1024}"), mJsonRpcResponse));

	deinitforJsonRpc();
	
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

    initforJsonRpc();

    // TC-37: Rate limit failure using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("rateLimit"), _T("{\"downloadId\": \"1001\", \"limit\": 1024}"), mJsonRpcResponse));

	deinitforJsonRpc();
	
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

    initforComRpc();

    getDownloadParams();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    uint64_t limit = 1024;

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

    EXPECT_EQ(downloadId.downloadId, "1001");

    // TC-38: Set rate limit via downloadID using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->RateLimit(downloadId.downloadId, limit));

	deinitforComRpc();
	
    releaseResources();
}

 /* Test Case for error while setting rate limit due to mismatch in ID using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the rateLimit method using the COM RPC interface, passing different downloadId and limit
 * Verify error while setting rate limit by asserting that it returns Core::ERROR_UNKNOWN_KEY
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

 TEST_F(PackageManagerTest, rateLimitusingComRpcError) {

    createResources();

    initforComRpc();

    getDownloadParams();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    uint64_t limit = 1024;

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

    EXPECT_EQ(downloadId.downloadId, "1001");

    string downloadId = "1002";

    // TC-39: Rate limit error when passing different downloadID using ComRpc
    EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, pkgdownloaderInterface->RateLimit(downloadId, limit));

	deinitforComRpc();
	
    releaseResources();
}

 /* Test Case for failure in setting rateLimit using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the rateLimit method using the COM RPC interface, passing downloadId and limit
 * Verify rateLimit method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

 TEST_F(PackageManagerTest, rateLimitusingComRpcError) {

    createResources();

    initforComRpc();

    uint64_t limit = 1024;
    string downloadId = "1001";

    // TC-40: Rate limit failure using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkgdownloaderInterface->RateLimit(downloadId, limit));

	deinitforComRpc();
	
    releaseResources();
}

// IPackageInstaller methods

TEST_F(PackageManagerTest, installusingJsonRpc) {

    createResources();   

    initforJsonRpc();

    EXPECT_CALL(*mStorageManagerMock, CreateStorage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const uint32_t &size, string& path, string &errorReason) {
                return Core::ERROR_NONE;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("install"), _T("{\"packageId\": \"testPackageId\", \"version\": \"2.0\", \"additionalMetadata\": {\"name\": \"testapp\", \"value\": \"2\", \"INTERFACE_ID\": 3}, \"fileLocator\": \"/opt/CDL/testpackageDownload\", \"reason\": 1}"), mJsonRpcResponse));

	deinitforJsonRpc();
	
    releaseResources();
}

TEST_F(PackageManagerTest, installusingComRpc) {

    createResources();   

    initforComRpc();

    string packageId = "testPackageId";
    string version = "2.0";
    string fileLocator = "/opt/CDL/testpackageDownload";
    Exchange::IPackageInstaller::FailReason reason = Exchange::IPackageInstaller::FailReason::NONE;
    list<Exchange::IPackageInstaller::KeyValue> kv = { {"testapp", "2"} };

    auto additionalMetadata = Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IKeyValueIterator>>::Create<Exchange::IPackageInstaller::IKeyValueIterator>(kv);

    EXPECT_CALL(*mStorageManagerMock, CreateStorage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const uint32_t &size, string& path, string &errorReason) {
                return Core::ERROR_NONE;
            }));

    EXPECT_EQ(Core::ERROR_NONE, pkginstallerInterface->Install(packageId, version, additionalMetadata, fileLocator, reason));

	deinitforComRpc();
	
    releaseResources();
}

TEST_F(PackageManagerTest, uninstallusingJsonRpc) {

    createResources();   

    initforJsonRpc();

    EXPECT_CALL(*mStorageManagerMock, DeleteStorage(::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, string &errorReason) {
                return Core::ERROR_NONE;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("uninstall"), _T("{\"packageId\": \"testPackageId\", \"errorReason\": \"no error\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
	
    releaseResources();
}

TEST_F(PackageManagerTest, uninstallusingComRpc) {

    createResources();   

    initforComRpc();

    string packageId = "testPackageId";
    string errorReason = "no error";

    EXPECT_CALL(*mStorageManagerMock, DeleteStorage(::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, string &errorReason) {
                return Core::ERROR_NONE;
            }));

    EXPECT_EQ(Core::ERROR_NONE, pkginstallerInterface->Uninstall(packageId, errorReason));

	deinitforComRpc();
	
    releaseResources();
}

TEST_F(PackageManagerTest, listPackagesusingJsonRpc) {

    createResources();   

    initforJsonRpc();

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("listPackages"), _T("{\"packages\": {}}"), mJsonRpcResponse));

	deinitforJsonRpc();
	
    releaseResources();
}

TEST_F(PackageManagerTest, listPackagesusingComRpc) {

    createResources();   

    initforComRpc();

    list<Exchange::IPackageInstaller::Package> packageList = { {} };

    auto packages = Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IPackageIterator>>::Create<Exchange::IPackageInstaller::IPackageIterator>(packageList);

    EXPECT_EQ(Core::ERROR_NONE, pkginstallerInterface->ListPackages(packages));

	deinitforComRpc();
	
    releaseResources();
}

TEST_F(PackageManagerTest, configusingJsonRpc) {

    createResources();   

    initforJsonRpc();

    waitForTimeout(); // 1 second timeout

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("config"), _T("{\"packageId\": \"testPackageId\", \"version\": \"2.0\", \"configMetadata\": {}}"), mJsonRpcResponse));

	deinitforJsonRpc();
	
    releaseResources();
}

TEST_F(PackageManagerTest, configusingComRpc) {

    createResources();   

    initforComRpc();

    string packageId = "testPackageId";
    string version = "2.0";

    Exchange::RuntimeConfig runtimeConfig = {};

    EXPECT_EQ(Core::ERROR_NONE, pkginstallerInterface->Config(packageId, version, runtimeConfig));

	deinitforComRpc();
	
    releaseResources();
}

TEST_F(PackageManagerTest, packageStateusingJsonRpc) {

    createResources();   

    initforJsonRpc();

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("packageState"), _T("{\"packageId\": \"testPackageId\", \"version\": \"2.0\", \"state\": 0}"), mJsonRpcResponse));

	deinitforJsonRpc();
	
    releaseResources();
}

TEST_F(PackageManagerTest, packageStateusingComRpc) {

    createResources();   

    initforComRpc();

    string packageId = "testPackageId";
    string version = "2.0";
    Exchange::IPackageInstaller::InstallState state = Exchange::IPackageInstaller::InstallState::INSTALLING;

    EXPECT_EQ(Core::ERROR_NONE, pkginstallerInterface->PackageState(packageId, version, state));

	deinitforComRpc();
	
    releaseResources();
}

TEST_F(PackageManagerTest, getConfigforPackageusingJsonRpc) {

    createResources();   

    initforJsonRpc();

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("getConfigForPackage"), _T("{\"fileLocator\": \"/opt/CDL/packagetestDownloadId\", \"id\": \"testID\", \"version\": \"2.0\", \"config\": {}}"), mJsonRpcResponse));

	deinitforJsonRpc();
	
    releaseResources();
}

TEST_F(PackageManagerTest, getConfigforPackageusingComRpc) {

    createResources();   

    initforComRpc();

    string fileLocator = "/opt/CDL/packagetestDownloadId";
    string id = "testID";
    string version = "2.0";

    Exchange::RuntimeConfig config = {};

    EXPECT_EQ(Core::ERROR_NONE, pkginstallerInterface->GetConfigForPackage(fileLocator, id, version, config));

	deinitforComRpc();
	
    releaseResources();
}

// IPackageHandler methods

TEST_F(PackageManagerTest, lockusingJsonRpc) {

    createResources();   

    initforJsonRpc();

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("lock"), _T("{\"packageId\": \"testPackageId\", \"version\": \"2.0\", \"lockReason\": 0, \"lockId\": 132, \"unpackedPath\": \"testPath\", \"configMetadata\": {}, \"appMetadata\": {}}"), mJsonRpcResponse));

	deinitforJsonRpc();
	
    releaseResources();
}

TEST_F(PackageManagerTest, lockusingComRpc) {

    createResources();   

    initforComRpc();

    string packageId = "testPackageId";
    string version = "2.0";
    Exchange::IPackageHandler::LockReason lockReason = Exchange::IPackageHandler::LockReason::SYSTEM_APP;
    uint32_t lockId = 132;
    string unpackedPath = "testPath";
    Exchange::RuntimeConfig configMetadata = {};
    list<Exchange::IPackageHandler::AdditionalLock> additionalLock = { {} };

    auto appMetadata = Core::Service<RPC::IteratorType<Exchange::IPackageHandler::ILockIterator>>::Create<Exchange::IPackageHandler::ILockIterator>(additionalLock);

    EXPECT_EQ(Core::ERROR_NONE, pkghandlerInterface->Lock(packageId, version, lockReason, lockId, unpackedPath, configMetadata, appMetadata));

	deinitforComRpc();
	
    releaseResources();
}

TEST_F(PackageManagerTest, unlockusingJsonRpc) {

    createResources();   

    initforJsonRpc();

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("unlock"), _T("{\"packageId\": \"testPackageId\", \"version\": \"2.0\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
	
    releaseResources();
}

TEST_F(PackageManagerTest, unlockusingComRpc) {

    createResources();   

    initforComRpc();

    string packageId = "testPackageId";
    string version = "2.0";

    EXPECT_EQ(Core::ERROR_NONE, pkghandlerInterface->Unlock(packageId, version));

	deinitforComRpc();
	
    releaseResources();
}

TEST_F(PackageManagerTest, getLockedInfousingJsonRpc) {

    createResources();   

    initforJsonRpc();

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("getLockedInfo"), _T("{\"packageId\": \"testPackageId\", \"version\": \"2.0\", \"unpackedPath\": \"testPath\", \"configMetadata\": {}, \"gatewayMetadataPath\": \"testgatewayMetadataPath\", \"locked\": true}"), mJsonRpcResponse));

	deinitforJsonRpc();
	
    releaseResources();
}

TEST_F(PackageManagerTest, getLockedInfousingComRpc) {

    createResources();   

    initforComRpc();

    string packageId = "testPackageId";
    string version = "2.0";
    string unpackedPath = "testPath";
    Exchange::RuntimeConfig configMetadata = {};
    string gatewayMetadataPath = "testgatewayMetadataPath";
    bool locked = true;

    EXPECT_EQ(Core::ERROR_NONE, pkghandlerInterface->GetLockedInfo(packageId, version, unpackedPath, configMetadata, gatewayMetadataPath, locked));

	deinitforComRpc();
	
    releaseResources();
}
