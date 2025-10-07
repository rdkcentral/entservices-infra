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

        downloadId = {
            "testDownloadId"
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

    releaseResources();

    deinitforJsonRpc();
}

TEST_F(PackageManagerTest, downloadMethodusingJsonRpcSuccess) {

    createResources();

    initforJsonRpc();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"uri\": \"http://test.com\", \"options\": {\"priority\": true, \"retries\": 2, \"rateLimit\": 1024}, \"downloadId\": {\"testDownloadId\"}}"), mJsonRpcResponse));

    releaseResources();

    deinitforJsonRpc();
}

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

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

    releaseResources();

    deinitforComRpc();
}

TEST_F(PackageManagerTest, pauseMethodusingJsonRpcFailure) {

    createResources();

    initforJsonRpc();

    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("pause"), _T("{\"downloadId\": \"testDownloadId\"}"), mJsonRpcResponse));
    
    releaseResources();

    deinitforJsonRpc();
}

TEST_F(PackageManagerTest, pauseMethodusingComRpcFailure) {

    createResources();

    initforComRpc();

    string downloadId = "testDownloadId";

    EXPECT_EQ(Core::ERROR_GENERAL, pkgdownloaderInterface->Pause(downloadId));

    releaseResources();

    deinitforComRpc();
}

TEST_F(PackageManagerTest, resumeMethodusingJsonRpc) {

    createResources();

    initforJsonRpc();

    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("resume"), _T("{\"downloadId\": \"testDownloadId\"}"), mJsonRpcResponse));
    
    releaseResources();

    deinitforJsonRpc();
}

TEST_F(PackageManagerTest, resumeMethodusingComRpc) {

    createResources();

    initforComRpc();

    string downloadId = "testDownloadId";

    EXPECT_EQ(Core::ERROR_GENERAL, pkgdownloaderInterface->Resume(downloadId));

    releaseResources();

    deinitforComRpc();
}

TEST_F(PackageManagerTest, cancelMethodusingJsonRpc) {

    createResources();

    initforJsonRpc();

    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("cancel"), _T("{\"downloadId\": \"testDownloadId\"}"), mJsonRpcResponse));
    
    releaseResources();

    deinitforJsonRpc();
}

TEST_F(PackageManagerTest, cancelMethodusingComRpc) {

    createResources();

    initforComRpc();

    string downloadId = "testDownloadId";

    EXPECT_EQ(Core::ERROR_GENERAL, pkgdownloaderInterface->Cancel(downloadId));

    releaseResources();

    deinitforComRpc();
}

TEST_F(PackageManagerTest, deleteMethodusingJsonRpc) {

    createResources();

    initforJsonRpc();

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("delete"), _T("{\"fileLocator\": \"/opt/CDL/packagetestDownloadId\"}"), mJsonRpcResponse));
    
    releaseResources();

    deinitforJsonRpc();
}

TEST_F(PackageManagerTest, deleteMethodusingComRpc) {

    createResources();

    initforComRpc();

    string fileLocator = "/opt/CDL/packagetestDownloadId";

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Delete(fileLocator));

    releaseResources();

    deinitforComRpc();
}

TEST_F(PackageManagerTest, progressMethodusingJsonRpc) {

    createResources();

    initforJsonRpc();

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("progress"), _T("{\"downloadId\": \"testDownloadId\", \"progress\": {\"progress\": 0}}"), mJsonRpcResponse));

    releaseResources();

    deinitforJsonRpc();
}

TEST_F(PackageManagerTest, progressMethodusingComRpc) {

    createResources();

    initforComRpc();

    progress = {
        0
    };

    string downloadId = "testDownloadId";

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Progress(downloadId, progress));

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

    string quotaKB = "1024";
    string usedKB = "568";

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->GetStorageDetails(quotaKB, usedKB));

    releaseResources();

    deinitforComRpc();
}

TEST_F(PackageManagerTest, rateLimitusingJsonRpc) {

    createResources();

    initforJsonRpc();

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("rateLimit"), _T("{\"downloadId\": \"testDownloadId\", \"limit\": 1024}"), mJsonRpcResponse));

    releaseResources();

    deinitforJsonRpc();
}

TEST_F(PackageManagerTest, rateLimitusingComRpc) {

    createResources();

    initforComRpc();

    uint64_t limit = 1024;
    string downloadId = "testDownloadId";

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->RateLimit(downloadId, limit));

    releaseResources();

    deinitforComRpc();
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
    
    releaseResources();

    deinitforJsonRpc();
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

    releaseResources();

    deinitforComRpc();
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
    
    releaseResources();

    deinitforJsonRpc();
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

    releaseResources();

    deinitforComRpc();
}

TEST_F(PackageManagerTest, listPackagesusingJsonRpc) {

    createResources();   

    initforJsonRpc();

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("listPackages"), _T("{\"packages\": {}}"), mJsonRpcResponse));
    
    releaseResources();

    deinitforJsonRpc();
}

TEST_F(PackageManagerTest, listPackagesusingComRpc) {

    createResources();   

    initforComRpc();

    list<Exchange::IPackageInstaller::Package> packageList = { {} };

    auto packages = Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IPackageIterator>>::Create<Exchange::IPackageInstaller::IPackageIterator>(packageList);

    EXPECT_EQ(Core::ERROR_NONE, pkginstallerInterface->ListPackages(packages));
    
    releaseResources();

    deinitforComRpc();
}

TEST_F(PackageManagerTest, configusingJsonRpc) {

    createResources();   

    initforJsonRpc();

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("config"), _T("{\"packageId\": \"testPackageId\", \"version\": \"2.0\", \"configMetadata\": {}"), mJsonRpcResponse));
    
    releaseResources();

    deinitforJsonRpc();
}

TEST_F(PackageManagerTest, configusingComRpc) {

    createResources();   

    initforComRpc();

    string packageId = "testPackageId";
    string version = "2.0";

    Exchange::RuntimeConfig runtimeConfig = {};

    EXPECT_EQ(Core::ERROR_NONE, pkginstallerInterface->Config(packageId, version, runtimeConfig));

    releaseResources();

    deinitforComRpc();
}

TEST_F(PackageManagerTest, packageStateusingJsonRpc) {

    createResources();   

    initforJsonRpc();

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("packageState"), _T("{\"packageId\": \"testPackageId\", \"version\": \"2.0\", \"state\": 0}"), mJsonRpcResponse));
    
    releaseResources();

    deinitforJsonRpc();
}

TEST_F(PackageManagerTest, packageStateusingComRpc) {

    createResources();   

    initforComRpc();

    string packageId = "testPackageId";
    string version = "2.0";
    Exchange::IPackageInstaller::InstallState state = Exchange::IPackageInstaller::InstallState::INSTALLING;

    EXPECT_EQ(Core::ERROR_NONE, pkginstallerInterface->PackageState(packageId, version, state));
    
    releaseResources();

    deinitforComRpc();
}

TEST_F(PackageManagerTest, getConfigforPackageusingJsonRpc) {

    createResources();   

    initforJsonRpc();

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("getConfigForPackage"), _T("{\"fileLocator\": \"/opt/CDL/packagetestDownloadId\", \"id\": \"testID\", \"version\": \"2.0\", \"config\": {}}"), mJsonRpcResponse));
    
    releaseResources();

    deinitforJsonRpc();
}

TEST_F(PackageManagerTest, getConfigforPackageusingComRpc) {

    createResources();   

    initforComRpc();

    string fileLocator = "/opt/CDL/packagetestDownloadId";
    string id = "testID";
    string version = "2.0";

    Exchange::RuntimeConfig config = {};

    EXPECT_EQ(Core::ERROR_NONE, pkginstallerInterface->GetConfigForPackage(fileLocator, id, version, config));
    
    releaseResources();

    deinitforComRpc();
}

// IPackageHandler methods

TEST_F(PackageManagerTest, lockusingJsonRpc) {

    createResources();   

    initforJsonRpc();

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("lock"), _T("{\"packageId\": \"testPackageId\", \"version\": \"2.0\", \"lockReason\": 0, \"lockId\": 132, \"unpackedPath\": \"testPath\", \"configMetadata\": {}, \"appMetadata\": {}}"), mJsonRpcResponse));
    
    releaseResources();

    deinitforJsonRpc();
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
    
    releaseResources();

    deinitforComRpc();
}

TEST_F(PackageManagerTest, unlockusingJsonRpc) {

    createResources();   

    initforJsonRpc();

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("unlock"), _T("{\"packageId\": \"testPackageId\", \"version\": \"2.0\"}"), mJsonRpcResponse));
    
    releaseResources();

    deinitforJsonRpc();
}

TEST_F(PackageManagerTest, unlockusingComRpc) {

    createResources();   

    initforComRpc();

    string packageId = "testPackageId";
    string version = "2.0";

    EXPECT_EQ(Core::ERROR_NONE, pkghandlerInterface->Unlock(packageId, version));
    
    releaseResources();

    deinitforComRpc();
}

TEST_F(PackageManagerTest, getLockedInfousingJsonRpc) {

    createResources();   

    initforJsonRpc();

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("getLockedInfo"), _T("{\"packageId\": \"testPackageId\", \"version\": \"2.0\", \"unpackedPath\": \"testPath\", \"configMetadata\": {}, \"gatewayMetadataPath\": \"testgatewayMetadataPath\", \"locked\": true}"), mJsonRpcResponse));
    
    releaseResources();

    deinitforJsonRpc();
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
    
    releaseResources();

    deinitforComRpc();
}
