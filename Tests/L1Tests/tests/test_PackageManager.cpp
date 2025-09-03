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
//include needed mock headers after creating
#include "StorageManagerMock.h"
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

    Core::ProxyType<Plugin::PackageManager> plugin;
    Core::JSONRPC::Handler& mJsonRpcHandler;
    Core::JSONRPC::Message message;
    DECL_CORE_JSONRPC_CONX connection;
    string mJsonRpcResponse;
    PLUGINHOST_DISPATCHER *dispatcher;
    FactoriesImplementation factoriesImplementation;

    Core::ProxyType<Plugin::PackageManagerImplementation> mPackageManagerImpl;
    Core::ProxyType<WorkerpoolImplementation> workerPool;

    Exchange::IPackageDownloader* pkgdownloaderInterface = nullptr;
    Exchange::IPackageInstaller* pkginstallerInterface = nullptr;
    Exchange::IPackageHandler* pkghandlerInterface = nullptr;
    Exchange::IPackageDownloader::Options options;
    Exchange::IPackageDownloader::Percent percent;

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

        EXPECT_CALL(*mServiceMock, QueryInterfaceByCallsign(::testing::_, ::testing::_))
          .Times(::testing::AnyNumber())
          .WillRepeatedly(::testing::Invoke(
              [&](const uint32_t, const std::string& name) -> void* {
                if (name == "org.rdk.StorageManager") {
                    return reinterpret_cast<void*>(mStorageManagerMock);
                } 
            return nullptr;
        }));

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

        // Initialize the parameters required for COM-RPC with default values
        string uri = "http://test.com";
        string downloadId = "testDownloadId";

        options = { 
            true,2,1024
        };
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
        dispatcher->Deactivate();
        dispatcher->Release();

        plugin->Deinitialize(mServiceMock);
    }

    void deinitforComRpc()
    {
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

    releaseResources();

    deinitforJsonRpc();
}

TEST_F(PackageManagerTest, downloadMethodusingJsonRpcSuccess) {

    createResources();

    initforJsonRpc();

    EXPECT_CALL(*mServiceMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& url, const Exchange::IPackageDownloader::Options &options, Exchange::IPackageDownloader::DownloadId &downloadId) {
                return Core::ERROR_NONE;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"uri\": \"http://test.com\", \"options\": {\"priority\": true, \"retries\": 2, \"rateLimit\": 1024}, \"downloadId\": \"testDownloadId\"}"), mJsonRpcResponse));

    releaseResources();

    deinitforJsonRpc();
}

TEST_F(PackageManagerTest, downloadMethodsusingComRpcSuccess) {

    createResources();

    initforComRpc();

    EXPECT_CALL(*mServiceMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& url, const Exchange::IPackageDownloader::Options &options, Exchange::IPackageDownloader::DownloadId &downloadId) {
                return Core::ERROR_NONE;
            }));

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

    releaseResources();

    deinitforComRpc();
}

TEST_F(PackageManagerTest, pauseMethodsusingJsonRpc) {

    createResources();

    initforJsonRpc();

    EXPECT_CALL(*mServiceMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& url, const Exchange::IPackageDownloader::Options &options, Exchange::IPackageDownloader::DownloadId &downloadId) {
                return Core::ERROR_NONE;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"uri\": \"http://test.com\", \"options\": {\"priority\": true, \"retries\": 2, \"rateLimit\": 1024}, \"downloadId\": \"testDownloadId\"}"), mJsonRpcResponse));
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("pause"), _T("{\"downloadId\": \"testDownloadId\"}"), mJsonRpcResponse));
    
    releaseResources();

    deinitforJsonRpc();
}

TEST_F(PackageManagerTest, pauseMethodsusingComRpc) {

    createResources();

    initforComRpc();

    EXPECT_CALL(*mServiceMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& url, const Exchange::IPackageDownloader::Options &options, Exchange::IPackageDownloader::DownloadId &downloadId) {
                return Core::ERROR_NONE;
            }));

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));
    EXPECT_EQ(Core::ERROR_GENERAL, pkgdownloaderInterface->Pause(downloadId));

    releaseResources();

    deinitforComRpc();
}

TEST_F(PackageManagerTest, resumeMethodsusingJsonRpc) {

    createResources();

    initforJsonRpc();

    EXPECT_CALL(*mServiceMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& url, const Exchange::IPackageDownloader::Options &options, Exchange::IPackageDownloader::DownloadId &downloadId) {
                return Core::ERROR_NONE;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"uri\": \"http://test.com\", \"options\": {\"priority\": true, \"retries\": 2, \"rateLimit\": 1024}, \"downloadId\": \"testDownloadId\"}"), mJsonRpcResponse));
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("pause"), _T("{\"downloadId\": \"testDownloadId\"}"), mJsonRpcResponse));
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("resume"), _T("{\"downloadId\": \"testDownloadId\"}"), mJsonRpcResponse));
    
    releaseResources();

    deinitforJsonRpc();
}

TEST_F(PackageManagerTest, resumeMethodsusingComRpc) {

    createResources();

    initforComRpc();

    EXPECT_CALL(*mServiceMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& url, const Exchange::IPackageDownloader::Options &options, Exchange::IPackageDownloader::DownloadId &downloadId) {
                return Core::ERROR_NONE;
            }));

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));
    EXPECT_EQ(Core::ERROR_GENERAL, pkgdownloaderInterface->Pause(downloadId));
    EXPECT_EQ(Core::ERROR_GENERAL, pkgdownloaderInterface->Resume(downloadId));

    releaseResources();

    deinitforComRpc();
}

TEST_F(PackageManagerTest, cancelMethodsusingJsonRpc) {

    createResources();

    initforJsonRpc();

    EXPECT_CALL(*mServiceMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& url, const Exchange::IPackageDownloader::Options &options, Exchange::IPackageDownloader::DownloadId &downloadId) {
                return Core::ERROR_NONE;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"uri\": \"http://test.com\", \"options\": {\"priority\": true, \"retries\": 2, \"rateLimit\": 1024}, \"downloadId\": \"testDownloadId\"}"), mJsonRpcResponse));
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("cancel"), _T("{\"downloadId\": \"testDownloadId\"}"), mJsonRpcResponse));
    
    releaseResources();

    deinitforJsonRpc();
}

TEST_F(PackageManagerTest, cancelMethodusingComRpc) {

    createResources();

    initforComRpc();

    EXPECT_CALL(*mServiceMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& url, const Exchange::IPackageDownloader::Options &options, Exchange::IPackageDownloader::DownloadId &downloadId) {
                return Core::ERROR_NONE;
            }));
    
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));
    EXPECT_EQ(Core::ERROR_GENERAL, pkgdownloaderInterface->Cancel(downloadId));

    releaseResources();

    deinitforComRpc();
}

TEST_F(PackageManagerTest, deleteMethodsusingJsonRpc) {

    createResources();

    initforJsonRpc();

    EXPECT_CALL(*mServiceMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& url, const Exchange::IPackageDownloader::Options &options, Exchange::IPackageDownloader::DownloadId &downloadId) {
                return Core::ERROR_NONE;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"uri\": \"http://test.com\", \"options\": {\"priority\": true, \"retries\": 2, \"rateLimit\": 1024}, \"downloadId\": \"testDownloadId\"}"), mJsonRpcResponse));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("delete"), _T("{\"fileLocator\": \"/opt/CDL/packagetestDownloadId\"}"), mJsonRpcResponse));
    
    releaseResources();

    deinitforJsonRpc();
}

TEST_F(PackageManagerTest, deleteMethodsusingComRpc) {

    createResources();

    initforComRpc();

    string fileLocator = "/opt/CDL/packagetestDownloadId";

    EXPECT_CALL(*mServiceMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& url, const Exchange::IPackageDownloader::Options &options, Exchange::IPackageDownloader::DownloadId &downloadId) {
                return Core::ERROR_NONE;
            }));

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Delete(fileLocator));

    releaseResources();

    deinitforComRpc();
}

TEST_F(PackageManagerTest, progressMethodusingJsonRpc) {

    createResources();

    initforJsonRpc();

    EXPECT_CALL(*mServiceMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& url, const Exchange::IPackageDownloader::Options &options, Exchange::IPackageDownloader::DownloadId &downloadId) {
                return Core::ERROR_NONE;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"uri\": \"http://test.com\", \"options\": {\"priority\": true, \"retries\": 2, \"rateLimit\": 1024}, \"downloadId\": \"testDownloadId\"}"), mJsonRpcResponse));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("progress"), _T("{\"downloadId\": \"testDownloadId\", \"percent\": {\"percent\": 0}}"), mJsonRpcResponse));

    releaseResources();

    deinitforJsonRpc();
}

TEST_F(PackageManagerTest, progressMethodusingComRpc) {

    createResources();

    initforComRpc();

    percent = {
        0
    };

    EXPECT_CALL(*mServiceMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& url, const Exchange::IPackageDownloader::Options &options, Exchange::IPackageDownloader::DownloadId &downloadId) {
                return Core::ERROR_NONE;
            }));

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Progress(downloadId, percent));

    releaseResources();

    deinitforComRpc();
}

TEST_F(PackageManagerTest, getStorageDetailsusingJsonRpc) {

    createResources();

    initforJsonRpc();

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("getStorageDetails"), _T("{\"quotaKB\": \"1024\", \"usedKB\": \"568\"}"), mJsonRpcResponse));

    releaseResources();

    deinitforJsonRpc();
}

TEST_F(PackageManagerTest, getStorageDetailsusingComRpc) {

    createResources();

    initforComRpc();

    uint32_t quotaKB = 1024;
    uint32_t usedKB = 568;

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->GetStorageDetails(quotaKB, usedKB));

    releaseResources();

    deinitforComRpc();
}

TEST_F(PackageManagerTest, rateLimitusingJsonRpc) {

    createResources();

    initforJsonRpc();

    EXPECT_CALL(*mServiceMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& url, const Exchange::IPackageDownloader::Options &options, Exchange::IPackageDownloader::DownloadId &downloadId) {
                return Core::ERROR_NONE;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"uri\": \"http://test.com\", \"options\": {\"priority\": true, \"retries\": 2, \"rateLimit\": 1024}, \"downloadId\": \"testDownloadId\"}"), mJsonRpcResponse));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("rateLimit"), _T("{\"downloadId\": \"testDownloadId\", \"limit\": 1024}"), mJsonRpcResponse));

    releaseResources();

    deinitforJsonRpc();
}

TEST_F(PackageManagerTest, rateLimitusingComRpc) {

    createResources();

    initforComRpc();

    uint64_t limit = 1024;

    EXPECT_CALL(*mServiceMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& url, const Exchange::IPackageDownloader::Options &options, Exchange::IPackageDownloader::DownloadId &downloadId) {
                return Core::ERROR_NONE;
            }));

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->RateLimit(downloadId, limit));

    releaseResources();

    deinitforComRpc();
}

#if 0
// IPackageInstaller methods

TEST_F(PackageManagerTest, installusingJsonRpc) {

    createResources();

    initforJsonRpc();




}
#endif
