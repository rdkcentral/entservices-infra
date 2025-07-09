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

#include "AppManager.h"
#include "AppManagerImplementation.h"
#include "ServiceMock.h"
#include "LifecycleManagerMock.h"
#include "PackageManagerMock.h"
#include "Store2Mock.h"
#include "COMLinkMock.h"
#include "ThunderPortability.h"
#include "Module.h"
#include "WorkerPoolImplementation.h"

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);

#define APPMANAGER_APP_ID           "com.test.app"
#define APPMANAGER_EMPTY_APP_ID     ""
#define APPMANAGER_APP_VERSION      "1.2.8"
#define APPMANAGER_APP_DIGEST       ""
#define APPMANAGER_APP_STATE        Exchange::IPackageInstaller::InstallState::INSTALLED
#define APPMANAGER_APP_STATE_STR    "APPLICATION_TYPE_INTERACTIVE"
#define APPMANAGER_APP_SIZE         0
#define APPMANAGER_WRONG_APP_ID     "com.wrongtest.app"
#define APPMANAGER_APP_INTENT       "test.intent"
#define APPMANAGER_APP_LAUNCHARGS   "test.arguments"
#define APPMANAGER_APP_INSTANCE     "testAppInstance"
#define APPMANAGER_APP_UNPACKEDPATH "/media/apps/sky/packages/Hulu/data.img"
#define PERSISTENT_STORE_KEY        "DUMMY"
#define PERSISTENT_STORE_VALUE      "DUMMY_VALUE"
#define APPMANAGER_PACKAGEID        "testPackageID"
#define APPMANAGER_INSTALLSTATUS_INSTALLED    "INSTALLED"
#define APPMANAGER_INSTALLSTATUS_UNINSTALLED    "UNINSTALLED"

using ::testing::NiceMock;
using namespace WPEFramework;

namespace {
const string callSign = _T("AppManager");
}
class AppManagerTest : public ::testing::Test {
protected:
    ServiceMock* mServiceMock = nullptr;
    LifecycleManagerMock* mLifecycleManagerMock = nullptr;
    LifecycleManagerStateMock* mLifecycleManagerStateMock = nullptr;
    PackageManagerMock* mPackageManagerMock = nullptr;
    PackageInstallerMock* mPackageInstallerMock = nullptr;
    Store2Mock* mStore2Mock = nullptr;

    Core::ProxyType<Plugin::AppManager> mAppManagerPlugin;
    Plugin::AppManagerImplementation *mAppManagerImpl;

    Core::ProxyType<WorkerPoolImplementation> workerPool;

    Core::JSONRPC::Handler& mJsonRpcHandler;
    DECL_CORE_JSONRPC_CONX connection;
    string mJsonRpcResponse;

    void createAppManagerImpl()
    {
        mServiceMock = new NiceMock<ServiceMock>;

        TEST_LOG("In createAppManagerImpl!");
        EXPECT_EQ(string(""), mAppManagerPlugin->Initialize(mServiceMock));
        mAppManagerImpl = Plugin::AppManagerImplementation::getInstance();
    }

    void releaseAppManagerImpl()
    {
        TEST_LOG("In releaseAppManagerImpl!");
        mAppManagerPlugin->Deinitialize(mServiceMock);
        delete mServiceMock;
        mAppManagerImpl = nullptr;
    }

    Core::hresult createResources()
    {
        Core::hresult status = Core::ERROR_GENERAL;
        mServiceMock = new NiceMock<ServiceMock>;
        mLifecycleManagerMock = new NiceMock<LifecycleManagerMock>;
        mLifecycleManagerStateMock = new NiceMock<LifecycleManagerStateMock>;
        mPackageManagerMock = new NiceMock<PackageManagerMock>;
        mPackageInstallerMock = new NiceMock<PackageInstallerMock>;
        mStore2Mock = new NiceMock<Store2Mock>;

        TEST_LOG("In createResources!");

        EXPECT_CALL(*mServiceMock, QueryInterfaceByCallsign(::testing::_, ::testing::_))
          .Times(::testing::AnyNumber())
          .WillRepeatedly(::testing::Invoke(
              [&](const uint32_t id, const std::string& name) -> void* {
                if (name == "org.rdk.LifecycleManager") {
                    if(id == Exchange::ILifecycleManager::ID) {
                        return reinterpret_cast<void*>(mLifecycleManagerMock);
                    }
                    else if(id == Exchange::ILifecycleManagerState::ID) {
                        return reinterpret_cast<void*>(mLifecycleManagerStateMock);
                    }
                } else if (name == "org.rdk.PersistentStore") {
                   return reinterpret_cast<void*>(mStore2Mock);
                } else if (name == "org.rdk.PackageManagerRDKEMS") {
                    if (id == Exchange::IPackageHandler::ID) {
                        return reinterpret_cast<void*>(mPackageManagerMock);
                    }
                    else if (id == Exchange::IPackageInstaller::ID){
                        return reinterpret_cast<void*>(mPackageInstallerMock);
                    }
                }
            return nullptr;
        }));

        EXPECT_EQ(string(""), mAppManagerPlugin->Initialize(mServiceMock));
        mAppManagerImpl = Plugin::AppManagerImplementation::getInstance();
        TEST_LOG("createResources - All done!");
        status = Core::ERROR_NONE;

        return status;
    }

    void releaseResources()
    {
        TEST_LOG("In releaseResources!");
        if (mLifecycleManagerMock != nullptr)
        {
            EXPECT_CALL(*mLifecycleManagerMock, Release())
                .WillOnce(::testing::Invoke(
                [&]() {
                     delete mLifecycleManagerMock;
                     return 0;
                    }));
        }
        if (mLifecycleManagerStateMock != nullptr)
        {
            EXPECT_CALL(*mLifecycleManagerStateMock, Unregister(::testing::_))
                .WillOnce(::testing::Invoke(
                [&]() {
                     return 0;
                    }));
            EXPECT_CALL(*mLifecycleManagerStateMock, Release())
                .WillOnce(::testing::Invoke(
                [&]() {
                     delete mLifecycleManagerStateMock;
                     return 0;
                    }));
        }
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
        if (mStore2Mock != nullptr)
        {
            EXPECT_CALL(*mStore2Mock, Release())
                .WillOnce(::testing::Invoke(
                [&]() {
                     delete mStore2Mock;
                     return 0;
                    }));
        }

        mAppManagerPlugin->Deinitialize(mServiceMock);
        delete mServiceMock;
        mAppManagerImpl = nullptr;
    }


    AppManagerTest()
    : mAppManagerPlugin(Core::ProxyType<Plugin::AppManager>::Create()),
      workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(2, Core::Thread::DefaultStackSize(), 16)),
      mJsonRpcHandler(*mAppManagerPlugin),
      INIT_CONX(1, 0)
    {
        Core::IWorkerPool::Assign(&(*workerPool));
        workerPool->Run();
    }

    virtual ~AppManagerTest() override
    {
        TEST_LOG("Delete ~AppManagerTest Instance!");
        Core::IWorkerPool::Assign(nullptr);
        workerPool.Release();
    }
    std::string GetPackageInfoInJSON()
    {
        std::string apps_str = "";
        JsonObject package;
        JsonArray installedAppsArray;

        package["appId"] = APPMANAGER_APP_ID;
        package["versionString"] = APPMANAGER_APP_VERSION;
        package["type"] = APPMANAGER_APP_STATE_STR;
        package["lastActiveTime"] = "";
        package["lastActiveIndex"] = "";

        installedAppsArray.Add(package);
        installedAppsArray.ToString(apps_str);

        return apps_str;
    }

     auto FillPackageIterator()
    {
        std::list<Exchange::IPackageInstaller::Package> packageList;
        Exchange::IPackageInstaller::Package package_1;

        package_1.packageId = APPMANAGER_APP_ID;
        package_1.version = APPMANAGER_APP_VERSION;
        package_1.digest = APPMANAGER_APP_DIGEST;
        package_1.state = APPMANAGER_APP_STATE;
        package_1.sizeKb = APPMANAGER_APP_SIZE;

        packageList.emplace_back(package_1);
        return Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IPackageIterator>>::Create<Exchange::IPackageInstaller::IPackageIterator>(packageList);
    }

    void LaunchAppPreRequisite(Exchange::ILifecycleManager::LifecycleState state)
    {
        const std::string launchArgs = APPMANAGER_APP_LAUNCHARGS;
        auto mockIterator = FillPackageIterator(); // Fill the package Info

        EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
        .WillRepeatedly([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
            packages = mockIterator;
            return Core::ERROR_NONE;
        });

        EXPECT_CALL(*mPackageManagerMock, Lock(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &packageId, const string &version, const Exchange::IPackageHandler::LockReason &lockReason, uint32_t &lockId /* @out */, string &unpackedPath /* @out */, Exchange::RuntimeConfig &configMetadata /* @out */, Exchange::IPackageHandler::ILockIterator*& appMetadata /* @out */) {
            lockId = 1;
            unpackedPath = APPMANAGER_APP_UNPACKEDPATH;
            return Core::ERROR_NONE;
        });

        EXPECT_CALL(*mLifecycleManagerMock, IsAppLoaded(::testing::_, ::testing::_))
        .WillRepeatedly([&](const std::string &appId, bool &loaded) {
            loaded = true;
            return Core::ERROR_NONE;
        });

        EXPECT_CALL(*mLifecycleManagerMock, SetTargetAppState(::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string& appInstanceId , const Exchange::ILifecycleManager::LifecycleState targetLifecycleState , const string& launchIntent) {
            return Core::ERROR_NONE;
        });
        EXPECT_CALL(*mLifecycleManagerMock, SpawnApp(APPMANAGER_APP_ID, ::testing::_, ::testing::_, ::testing::_, launchArgs, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce([&](const string& appId, const string& launchIntent, const Exchange::ILifecycleManager::LifecycleState targetLifecycleState,
            const Exchange::RuntimeConfig& runtimeConfigObject, const string& launchArgs, string& appInstanceId, string& errorReason, bool& success) {
            appInstanceId = APPMANAGER_APP_INSTANCE;
            errorReason = "";
            success = true;
            return Core::ERROR_NONE;
        });
    }

    void UnloadAppAndUnlock()
    {
        EXPECT_CALL(*mLifecycleManagerMock, UnloadApp(::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string& appInstanceId, string& errorReason, bool& success) {
            success = true;
            return Core::ERROR_NONE;
        });

        EXPECT_CALL(*mPackageManagerMock, Unlock(APPMANAGER_APP_ID, ::testing::_))
        .WillOnce([&](const string &packageId, const string &version) {
            return Core::ERROR_NONE;
        });
    }
};

/*******************************************************************************************************************
 * Test Case for RegisteredMethodsUsingJsonRpcSuccess
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Verifying whether all methods exists or not
 * Releasing the AppManager interface and all related test resources
 ********************************************************************************************************************/
TEST_F(AppManagerTest, RegisteredMethodsUsingJsonRpcSuccess)
{
    Core::hresult status;

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("getInstalledApps")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("isInstalled")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("getLoadedApps")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("launchApp")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("preloadApp")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("closeApp")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("terminateApp")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("startSystemApp")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("stopSystemApp")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("killApp")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("sendIntent")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("clearAppData")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("clearAllAppData")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("getAppMetadata")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("getAppProperty")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("setAppProperty")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("getMaxRunningApps")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("getMaxHibernatedApps")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("getMaxHibernatedFlashUsage")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("getMaxInactiveRamUsage")));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for GetInstalledAppsUsingComRpcSuccess
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Calling FillPackageIterator() to fill one package info in the package iterator
 * Setting Mock for ListPackages() to simulate getting installed package list
 * Verifying the return of the API
 * Verifying whether it returns the mocked package list filled or not
 * Releasing the AppManager Interface object and all related test resources
 */
TEST_F(AppManagerTest, GetInstalledAppsUsingComRpcSuccess)
{
    Core::hresult status;
    std::string apps = "";
    std::string jsonStr = "";

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);
    auto mockIterator = FillPackageIterator(); // Fill the package Info

    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
    .WillRepeatedly([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
        packages = mockIterator;
        return Core::ERROR_NONE;
    });

    EXPECT_EQ(Core::ERROR_NONE, mAppManagerImpl->GetInstalledApps(apps));
    TEST_LOG("GetInstalledApps :%s", apps.c_str());
    jsonStr = GetPackageInfoInJSON();
    TEST_LOG("jsonStr :%s", jsonStr.c_str());
    EXPECT_STREQ(jsonStr.c_str(), apps.c_str());

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for GetInstalledAppsUsingComRpcFailurePackageManagerObjectIsNull
 * Setting up only AppManager Plugin and creating required COM-RPC resources
 * PackageManager Interface object is not created and hence the API should return error
 * Releasing the AppManager Interface object only
 */
TEST_F(AppManagerTest, GetInstalledAppsUsingComRpcFailurePackageManagerObjectIsNull)
{
    std::string apps = APPMANAGER_APP_ID;

    createAppManagerImpl();
    EXPECT_EQ(Core::ERROR_GENERAL, mAppManagerImpl->GetInstalledApps(apps));
    releaseAppManagerImpl();
}

/*
 * Test Case for GetInstalledAppsUsingComRpcFailurePackageListEmpty
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Setting Mock for ListPackages() to simulate getting empty package list
 * Verifying the return of the API
 * Releasing the AppManager Interface object and all related test resources
 */
TEST_F(AppManagerTest, GetInstalledAppsUsingComRpcFailurePackageListEmpty)
{
    Core::hresult status;
    std::string apps = "";

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
    .WillOnce([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
        packages = nullptr; // make Package list empty
        return Core::ERROR_NONE;
    });

    EXPECT_EQ(Core::ERROR_GENERAL, mAppManagerImpl->GetInstalledApps(apps));
    TEST_LOG("GetInstalledApps :%s", apps.c_str());

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for GetInstalledAppsUsingComRpcFailureListPackagesReturnError
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Calling FillPackageIterator() to fill one package info in the package iterator
 * Setting Mock for ListPackages() to simulate error return
 * Verifying the return of the API
 * Releasing the AppManager Interface object and all related test resources
 */

TEST_F(AppManagerTest, GetInstalledAppsUsingComRpcFailureListPackagesReturnError)
{
    Core::hresult status;
    std::string apps = "";

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);
    auto mockIterator = FillPackageIterator(); // Fill the package Info

    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
    .WillOnce([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
        packages = mockIterator;
        return Core::ERROR_GENERAL;
    });

    EXPECT_EQ(Core::ERROR_GENERAL, mAppManagerImpl->GetInstalledApps(apps));
    TEST_LOG("GetInstalledApps :%s", apps.c_str());

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for IsInstalledUsingComRpcSuccess
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Calling FillPackageIterator() to fill one package info in the package iterator
 * Setting Mock for ListPackages() to simulate getting installed package list
 * Verifying the return of the API as well the installed flag
 * Releasing the AppManager Interface object and all related test resources
 */
TEST_F(AppManagerTest, IsInstalledUsingComRpcSuccess)
{
    Core::hresult status;
    bool installed;

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);
    auto mockIterator = FillPackageIterator(); // Fill the package Info

    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
    .WillOnce([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
        packages = mockIterator;
        return Core::ERROR_NONE;
    });

    EXPECT_EQ(Core::ERROR_NONE, mAppManagerImpl->IsInstalled(APPMANAGER_APP_ID, installed));
    EXPECT_EQ(installed, true);

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for IsInstalledUsingComRpcFailureWrongAppID
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Calling FillPackageIterator() to fill one package info in the package iterator
 * Setting Mock for ListPackages() to simulate getting installed package list
 * Verifying the return of the API as well the installed flag
 * Releasing the AppManager Interface object and all related test resources
 */

TEST_F(AppManagerTest, IsInstalledUsingComRpcFailureWrongAppID)
{
    Core::hresult status;
    bool installed;

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);
    auto mockIterator = FillPackageIterator(); // Fill the package Info

    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
    .WillOnce([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
        packages = mockIterator;
        return Core::ERROR_NONE;
    });

    EXPECT_EQ(Core::ERROR_NONE, mAppManagerImpl->IsInstalled(APPMANAGER_WRONG_APP_ID, installed));
    EXPECT_EQ(installed, false);

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for IsInstalledUsingComRpcFailurePackageListEmpty
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Setting Mock for ListPackages() to simulate getting installed package list
 * Verifying the return of the API as well the installed flag
 * Releasing the AppManager Interface object and all related test resources
 */
TEST_F(AppManagerTest, IsInstalledUsingComRpcFailurePackageListEmpty)
{
    Core::hresult status;
    bool installed;

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    // When package list is empty
    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
    .WillOnce([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
        packages = nullptr;
        return Core::ERROR_NONE;
    });

    EXPECT_EQ(Core::ERROR_GENERAL, mAppManagerImpl->IsInstalled(APPMANAGER_APP_ID, installed));
    EXPECT_EQ(installed, false);

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for IsInstalledUsingComRpcFailureEmptyAppID
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Calling FillPackageIterator() to fill one package info in the package iterator
 * Setting Mock for ListPackages() to simulate getting installed package list
 * Verifying the return of the API as well the installed flag
 * Releasing the AppManager Interface object and all related test resources
 */

TEST_F(AppManagerTest, IsInstalledUsingComRpcFailureEmptyAppID)
{
    Core::hresult status;
    bool installed;

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);
    auto mockIterator = FillPackageIterator(); // Fill the package Info

    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
    .WillOnce([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
        packages = mockIterator;
        return Core::ERROR_NONE;
    });

    EXPECT_EQ(Core::ERROR_NONE, mAppManagerImpl->IsInstalled("", installed));
    EXPECT_EQ(installed, false);

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for IsInstalledUsingComRpcFailureListPackagesReturnError
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Calling FillPackageIterator() to fill one package info in the package iterator
 * Setting Mock for ListPackages() to simulate error return
 * Releasing the AppManager Interface object and all related test resources
 */
TEST_F(AppManagerTest, IsInstalledUsingComRpcFailureListPackagesReturnError)
{
    Core::hresult status;
    bool installed;

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);
    auto mockIterator = FillPackageIterator(); // Fill the package Info

    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
    .WillOnce([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
        packages = mockIterator;
        return Core::ERROR_GENERAL;
    });

    EXPECT_EQ(Core::ERROR_GENERAL, mAppManagerImpl->IsInstalled(APPMANAGER_APP_ID, installed));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for IsInstalledUsingComRpcFailurePackageManagerObjectIsNull
 * Setting up only AppManager Plugin and creating required COM-RPC resources
 * PackageManager Interface object is not created and hence the API should return error
 * Setting Mock for ListPackages() to simulate error return
 * Releasing the AppManager Interface object only
 */

TEST_F(AppManagerTest, IsInstalledUsingComRpcFailurePackageManagerObjectIsNull)
{
    bool installed;

    createAppManagerImpl();
    EXPECT_EQ(Core::ERROR_GENERAL, mAppManagerImpl->IsInstalled(APPMANAGER_APP_ID, installed));
    releaseAppManagerImpl();
}

/*
 * Test Case for LaunchAppUsingComRpcSuccess
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Setting Mock for ListPackages() to simulate getting installed package list
 * Setting Mock for Lock() to simulate lockId and unpacked path
 * Setting Mock for IsAppLoaded() to simulate the package is loaded or not
 * Setting Mock for SetTargetAppState() to simulate setting the state
 * Setting Mock for SpawnApp() to simulate spawning a app and gettign the appinstance id
 * Verifying the return of the API
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, DISABLED_LaunchAppUsingComRpcSuccess)
{
    Core::hresult status;

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    LaunchAppPreRequisite(Exchange::ILifecycleManager::LifecycleState::ACTIVE);
    EXPECT_EQ(Core::ERROR_NONE, mAppManagerImpl->LaunchApp(APPMANAGER_APP_ID, APPMANAGER_APP_INTENT, APPMANAGER_APP_LAUNCHARGS));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for LaunchAppUsingComRpcFailureWrongAppID
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Setting Mock for ListPackages() to simulate getting installed package list
 * Setting Mock for Lock() to simulate lockId and unpacked path
 * Setting Mock for IsAppLoaded() to simulate the package is loaded or not
 * Setting Mock for SetTargetAppState() to simulate setting the state
 * Setting Mock for SpawnApp() to simulate spawning a app and gettign the appinstance id
 * Verifying the return of the API by passing the wrong app id
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, DISABLED_LaunchAppUsingComRpcFailureWrongAppID)
{
    Core::hresult status;

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    LaunchAppPreRequisite(Exchange::ILifecycleManager::LifecycleState::ACTIVE);
    EXPECT_EQ(Core::ERROR_GENERAL, mAppManagerImpl->LaunchApp(APPMANAGER_WRONG_APP_ID, APPMANAGER_APP_INTENT, APPMANAGER_APP_LAUNCHARGS));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for LaunchAppUsingComRpcFailureIsAppLoadedReturnError
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Setting Mock for ListPackages() to simulate getting installed package list
 * Setting Mock for Lock() to simulate lockId and unpacked path
 * Setting Mock for IsAppLoaded() to simulate error return
 * Setting Mock for SetTargetAppState() to simulate setting the state
 * Setting Mock for SpawnApp() to simulate spawning a app and gettign the appinstance id
 * Verifying the return of the API
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, DISABLED_LaunchAppUsingComRpcFailureIsAppLoadedReturnError)
{
    Core::hresult status;

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    LaunchAppPreRequisite(Exchange::ILifecycleManager::LifecycleState::ACTIVE);
    EXPECT_CALL(*mLifecycleManagerMock, IsAppLoaded(::testing::_, ::testing::_))
    .WillOnce([&](const std::string &appId, bool &loaded) {
        loaded = false;
        return Core::ERROR_GENERAL;
    });
    EXPECT_EQ(Core::ERROR_GENERAL, mAppManagerImpl->LaunchApp(APPMANAGER_APP_ID, APPMANAGER_APP_INTENT, APPMANAGER_APP_LAUNCHARGS));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for LaunchAppUsingComRpcFailureLifecycleManagerRemoteObjectIsNull
 * Setting up only AppManager Plugin and creating required COM-RPC resources
 * LifecycleManager Interface object is not created and hence the API should return error
 * Verifying the return of the API
 * Releasing the AppManager Interface object only
 */
TEST_F(AppManagerTest, LaunchAppUsingComRpcFailureLifecycleManagerRemoteObjectIsNull)
{
    createAppManagerImpl();

    EXPECT_EQ(Core::ERROR_GENERAL, mAppManagerImpl->LaunchApp(APPMANAGER_APP_ID, APPMANAGER_APP_INTENT, APPMANAGER_APP_LAUNCHARGS));

    releaseAppManagerImpl();
}

/*
 * Test Case for PreloadAppUsingComRpcSuccess
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Setting Mock for ListPackages() to simulate getting installed package list
 * Setting Mock for Lock() to simulate lockId and unpacked path
 * Setting Mock for IsAppLoaded() to simulate the package is loaded or not
 * Setting Mock for SetTargetAppState() to simulate setting the state
 * Setting Mock for SpawnApp() to simulate spawning a app and gettign the appinstance id
 * Verifying the return of the API
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, DISABLED_PreloadAppUsingComRpcSuccess)
{
    Core::hresult status;
    std::string error = "";

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    LaunchAppPreRequisite(Exchange::ILifecycleManager::LifecycleState::PAUSED);
    EXPECT_EQ(Core::ERROR_NONE, mAppManagerImpl->PreloadApp(APPMANAGER_APP_ID, APPMANAGER_APP_LAUNCHARGS, error));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for PreloadAppUsingComRpcFailureWrongAppID
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Setting Mock for ListPackages() to simulate getting installed package list
 * Setting Mock for Lock() to simulate lockId and unpacked path
 * Setting Mock for IsAppLoaded() to simulate the package is loaded or not
 * Setting Mock for SetTargetAppState() to simulate setting the state
 * Setting Mock for SpawnApp() to simulate spawning a app and gettign the appinstance id
 * Verifying the return of the API by passing the wrong app id
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, DISABLED_PreloadAppUsingComRpcFailureWrongAppID)
{
    Core::hresult status;
    std::string error = "";

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    LaunchAppPreRequisite(Exchange::ILifecycleManager::LifecycleState::PAUSED);

    EXPECT_EQ(Core::ERROR_GENERAL, mAppManagerImpl->PreloadApp(APPMANAGER_WRONG_APP_ID, APPMANAGER_APP_LAUNCHARGS, error));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for PreloadAppUsingComRpcFailureIsAppLoadedReturnError
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Setting Mock for ListPackages() to simulate getting installed package list
 * Setting Mock for Lock() to simulate lockId and unpacked path
 * Setting Mock for IsAppLoaded() to simulate error return
 * Setting Mock for SetTargetAppState() to simulate setting the state
 * Setting Mock for SpawnApp() to simulate spawning a app and gettign the appinstance id
 * Verifying the return of the API by passing the wrong app id
 * Releasing the AppManager interface and all related test resources
 */

TEST_F(AppManagerTest, DISABLED_PreloadAppUsingComRpcFailureIsAppLoadedReturnError)
{
    Core::hresult status;
    std::string error = "";

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    LaunchAppPreRequisite(Exchange::ILifecycleManager::LifecycleState::PAUSED);

    EXPECT_CALL(*mLifecycleManagerMock, IsAppLoaded(::testing::_, ::testing::_))
    .WillOnce([&](const std::string &appId, bool &loaded) {
        loaded = false;
        return Core::ERROR_GENERAL;
    });
    EXPECT_EQ(Core::ERROR_GENERAL, mAppManagerImpl->PreloadApp(APPMANAGER_APP_ID, APPMANAGER_APP_LAUNCHARGS, error));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for PreloadAppUsingComRpcFailureLifecycleManagerRemoteObjectIsNull
 * Setting up only AppManager Plugin and creating required COM-RPC resources
 * LifecycleManager Interface object is not created and hence the API should return error
 * Verifying the return of the API
 * Releasing the AppManager Interface object only
 */
TEST_F(AppManagerTest, PreloadAppUsingComRpcFailureLifecycleManagerRemoteObjectIsNull)
{
    std::string error = "";

    createAppManagerImpl();

    EXPECT_EQ(Core::ERROR_GENERAL, mAppManagerImpl->PreloadApp(APPMANAGER_APP_ID, APPMANAGER_APP_LAUNCHARGS, error));

    releaseAppManagerImpl();
}

/*
 * Test Case for CloseAppUsingComRpcSuccess
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Setting Mock for ListPackages() to simulate getting installed package list
 * Setting Mock for Lock() to simulate lockId and unpacked path
 * Setting Mock for IsAppLoaded() to simulate the package is loaded or not
 * Setting Mock for SetTargetAppState() to simulate setting the state
 * Setting Mock for SpawnApp() to simulate spawning a app and gettign the appinstance id
 * Verifying the return of the API
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, DISABLED_CloseAppUsingComRpcSuccess)
{
    Core::hresult status;

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    LaunchAppPreRequisite(Exchange::ILifecycleManager::LifecycleState::PAUSED);
    EXPECT_EQ(Core::ERROR_NONE, mAppManagerImpl->LaunchApp(APPMANAGER_APP_ID, APPMANAGER_APP_INTENT, APPMANAGER_APP_LAUNCHARGS));

    /*TODO: Fix following error
        ERROR [LifecycleInterfaceConnector.cpp:470] closeApp: Timed out waiting for appId: com.test.app to reach PAUSED state

        EXPECT_EQ(Core::ERROR_NONE, mAppManagerImpl->CloseApp(APPMANAGER_APP_ID));
    */
    EXPECT_EQ(Core::ERROR_GENERAL, mAppManagerImpl->CloseApp(APPMANAGER_APP_ID));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for CloseAppUsingComRpcFailureWrongAppID
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Setting Mock for ListPackages() to simulate getting installed package list
 * Setting Mock for Lock() to simulate lockId and unpacked path
 * Setting Mock for IsAppLoaded() to simulate the package is loaded or not
 * Setting Mock for SetTargetAppState() to simulate setting the state
 * Setting Mock for SpawnApp() to simulate spawning a app and gettign the appinstance id
 * Verifying the return of the API by passing the wrong app id
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, DISABLED_CloseAppUsingComRpcFailureWrongAppID)
{
    Core::hresult status;

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    LaunchAppPreRequisite(Exchange::ILifecycleManager::LifecycleState::ACTIVE);
    EXPECT_EQ(Core::ERROR_NONE, mAppManagerImpl->LaunchApp(APPMANAGER_APP_ID, APPMANAGER_APP_INTENT, APPMANAGER_APP_LAUNCHARGS));

    EXPECT_EQ(Core::ERROR_GENERAL, mAppManagerImpl->CloseApp(APPMANAGER_WRONG_APP_ID));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for CloseAppUsingComRpcFailureSetTargetAppStateReturnError
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Setting Mock for ListPackages() to simulate getting installed package list
 * Setting Mock for Lock() to simulate lockId and unpacked path
 * Setting Mock for IsAppLoaded() to simulate the package is loaded or not
 * Setting Mock for SetTargetAppState() to simulate error return
 * Setting Mock for SpawnApp() to simulate spawning a app and gettign the appinstance id
 * Verifying the return of the API
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, DISABLED_CloseAppUsingComRpcFailureSetTargetAppStateReturnError)
{
    Core::hresult status;

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    LaunchAppPreRequisite(Exchange::ILifecycleManager::LifecycleState::ACTIVE);
    EXPECT_EQ(Core::ERROR_NONE, mAppManagerImpl->LaunchApp(APPMANAGER_APP_ID, APPMANAGER_APP_INTENT, APPMANAGER_APP_LAUNCHARGS));

    EXPECT_CALL(*mLifecycleManagerMock, SetTargetAppState(::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string& appInstanceId , const Exchange::ILifecycleManager::LifecycleState targetLifecycleState , const string& launchIntent) {
            return Core::ERROR_GENERAL;
        });

    EXPECT_EQ(Core::ERROR_GENERAL, mAppManagerImpl->CloseApp(APPMANAGER_APP_ID));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for CloseAppUsingComRpcFailureLifecycleManagerRemoteObjectIsNull
 * Setting up only AppManager Plugin and creating required COM-RPC resources
 * LifecycleManager Interface object is not created and hence the API should return error
 * Verifying the return of the API
 * Releasing the AppManager Interface object only
 */

TEST_F(AppManagerTest, CloseAppUsingComRpcFailureLifecycleManagerRemoteObjectIsNull)
{
    createAppManagerImpl();

    EXPECT_EQ(Core::ERROR_GENERAL, mAppManagerImpl->CloseApp(APPMANAGER_APP_ID));

    releaseAppManagerImpl();
}

/*
 * Test Case for TerminateAppUsingComRpcSuccess
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Setting Mock for ListPackages() to simulate getting installed package list
 * Setting Mock for Lock() to simulate lockId and unpacked path
 * Setting Mock for IsAppLoaded() to simulate the package is loaded or not
 * Setting Mock for SetTargetAppState() to simulate setting the state
 * Setting Mock for SpawnApp() to simulate spawning a app and gettign the appinstance id
 * Calling LaunchApp first so that all the prerequisite will be filled
 * Setting Mock for UnloadApp() to simulate unloading the app
 * Setting Mock for Unlock() to simulate reset the lock flag
 * Verifying the return of the API
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, DISABLED_TerminateAppUsingComRpcSuccess)
{
    Core::hresult status;

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    LaunchAppPreRequisite(Exchange::ILifecycleManager::LifecycleState::ACTIVE);
    EXPECT_EQ(Core::ERROR_NONE, mAppManagerImpl->LaunchApp(APPMANAGER_APP_ID, APPMANAGER_APP_INTENT, APPMANAGER_APP_LAUNCHARGS));
    UnloadAppAndUnlock();

    EXPECT_EQ(Core::ERROR_NONE, mAppManagerImpl->TerminateApp(APPMANAGER_APP_ID));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for TerminateAppUsingComRpcFailureWrongAppID
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Setting Mock for ListPackages() to simulate getting installed package list
 * Setting Mock for Lock() to simulate lockId and unpacked path
 * Setting Mock for IsAppLoaded() to simulate the package is loaded or not
 * Setting Mock for SetTargetAppState() to simulate setting the state
 * Setting Mock for SpawnApp() to simulate spawning a app and gettign the appinstance id
 * Verifying the return of the API by passing the wrong app id
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, TerminateAppUsingComRpcFailureWrongAppID)
{
    Core::hresult status;

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_EQ(Core::ERROR_GENERAL, mAppManagerImpl->TerminateApp(APPMANAGER_WRONG_APP_ID));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for TerminateAppUsingComRpcFailureEmptyAppID
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Setting Mock for ListPackages() to simulate getting installed package list
 * Setting Mock for Lock() to simulate lockId and unpacked path
 * Setting Mock for IsAppLoaded() to simulate the package is loaded or not
 * Setting Mock for SetTargetAppState() to simulate setting the state
 * Setting Mock for SpawnApp() to simulate spawning a app and gettign the appinstance id
 * Verifying the return of the API by passing the empty app id
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, TerminateAppUsingComRpcFailureEmptyAppID)
{
    Core::hresult status;

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_EQ(Core::ERROR_GENERAL, mAppManagerImpl->TerminateApp(""));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for TerminateAppUsingComRpcFailureUnloadAppReturnError
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Setting Mock for ListPackages() to simulate getting installed package list
 * Setting Mock for Lock() to simulate lockId and unpacked path
 * Setting Mock for IsAppLoaded() to simulate the package is loaded or not
 * Setting Mock for SetTargetAppState() to simulate setting the state
 * Setting Mock for SpawnApp() to simulate spawning a app and gettign the appinstance id
 * Verifying the return of the API by no calling launch so that mInfo will be empty
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, TerminateAppUsingComRpcFailureUnloadAppReturnError)
{
    Core::hresult status;

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_EQ(Core::ERROR_GENERAL, mAppManagerImpl->TerminateApp(APPMANAGER_APP_ID));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for TerminateAppUsingComRpcFailureLifecycleManagerRemoteObjectIsNull
 * Setting up only AppManager Plugin and creating required COM-RPC resources
 * LifecycleManager Interface object is not created and hence the API should return error
 * Verifying the return of the API
 * Releasing the AppManager Interface object only
 */

TEST_F(AppManagerTest, TerminateAppUsingComRpcFailureLifecycleManagerRemoteObjectIsNull)
{
    createAppManagerImpl();
    EXPECT_EQ(Core::ERROR_GENERAL, mAppManagerImpl->TerminateApp(APPMANAGER_APP_ID));
    releaseAppManagerImpl();
}

/*
 * Test Case for StartSystemAppUsingComRpcSuccess
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Implementation missing TODO: Testcase need to be added once implementation is done
 * Verifying the return of the API
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, StartSystemAppUsingComRpcSuccess)
{
    Core::hresult status;

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_EQ(Core::ERROR_NONE, mAppManagerImpl->StartSystemApp(APPMANAGER_APP_ID));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for StopSystemAppUsingComRpcSuccess
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Implementation missing TODO: Testcase need to be added once implementation is done
 * Verifying the return of the API
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, StopSystemAppUsingComRpcSuccess)
{
    Core::hresult status;

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_EQ(Core::ERROR_NONE, mAppManagerImpl->StopSystemApp(APPMANAGER_APP_ID));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for KillAppUsingComRpcSuccess
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Setting Mock for ListPackages() to simulate getting installed package list
 * Setting Mock for Lock() to simulate lockId and unpacked path
 * Setting Mock for IsAppLoaded() to simulate the package is loaded or not
 * Setting Mock for SetTargetAppState() to simulate setting the state
 * Setting Mock for SpawnApp() to simulate spawning a app and gettign the appinstance id
 * Calling LaunchApp first so that all the prerequisite will be filled
 * Setting Mock for UnloadApp() to simulate unloading the app
 * Setting Mock for Unlock() to simulate reset the lock flag
 * Verifying the return of the API
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, DISABLED_KillAppUsingComRpcSuccess)
{
    Core::hresult status;

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    LaunchAppPreRequisite(Exchange::ILifecycleManager::LifecycleState::ACTIVE);
    EXPECT_EQ(Core::ERROR_NONE, mAppManagerImpl->LaunchApp(APPMANAGER_APP_ID, APPMANAGER_APP_INTENT, APPMANAGER_APP_LAUNCHARGS));
    UnloadAppAndUnlock();
    EXPECT_EQ(Core::ERROR_NONE, mAppManagerImpl->KillApp(APPMANAGER_APP_ID));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for TerminateAppUsingComRpcSuccess
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Verifying the return of the API by passing the wrong app id
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, KillAppUsingComRpcFailureWrongAppID)
{
    Core::hresult status;

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_EQ(Core::ERROR_GENERAL, mAppManagerImpl->KillApp(APPMANAGER_WRONG_APP_ID));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for KillAppUsingComRpcFailureLifecycleManagerRemoteObjectIsNull
 * Setting up only AppManager Plugin and creating required COM-RPC resources
 * LifecycleManager Interface object is not created and hence the API should return error
 * Verifying the return of the API
 * Releasing the AppManager Interface object only
 */

TEST_F(AppManagerTest, KillAppUsingComRpcFailureLifecycleManagerRemoteObjectIsNull)
{
    createAppManagerImpl();

    EXPECT_EQ(Core::ERROR_GENERAL, mAppManagerImpl->KillApp(APPMANAGER_WRONG_APP_ID));

    releaseAppManagerImpl();
}

/*
 * Test Case for SendIntentUsingComRpcSuccess
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Setting Mock for ListPackages() to simulate getting installed package list
 * Setting Mock for Lock() to simulate lockId and unpacked path
 * Setting Mock for IsAppLoaded() to simulate the package is loaded or not
 * Setting Mock for SetTargetAppState() to simulate setting the state
 * Setting Mock for SpawnApp() to simulate spawning a app and gettign the appinstance id
 * Calling LaunchApp first so that all the prerequisite will be filled
 * Verifying the return of the API
 * Releasing the AppManager interface and all related test resources
 */
 TEST_F(AppManagerTest, DISABLED_SendIntentUsingComRpcSuccess)
{
    Core::hresult status;

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_CALL(*mLifecycleManagerMock, SendIntentToActiveApp(::testing::_, ::testing::_, ::testing::_, ::testing::_))
    .WillOnce([&](const string& appInstanceId, const string& intent, string& errorReason, bool& success) {
        success = true;
        return Core::ERROR_NONE;
    });

    LaunchAppPreRequisite(Exchange::ILifecycleManager::LifecycleState::ACTIVE);
    EXPECT_EQ(Core::ERROR_NONE, mAppManagerImpl->LaunchApp(APPMANAGER_APP_ID, APPMANAGER_APP_INTENT, APPMANAGER_APP_LAUNCHARGS));

    EXPECT_EQ(Core::ERROR_NONE, mAppManagerImpl->SendIntent(APPMANAGER_APP_ID, APPMANAGER_APP_INTENT));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for SendIntentUsingComRpcFailureWrongAppID
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Verifying the return of the API by passing the wrong app id
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, SendIntentUsingComRpcFailureWrongAppID)
{
    Core::hresult status;

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_EQ(Core::ERROR_GENERAL, mAppManagerImpl->SendIntent(APPMANAGER_WRONG_APP_ID, APPMANAGER_APP_INTENT));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for SendIntentUsingComRpcFailureEmptyAppID
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Verifying the return of the API by passing the empty app id
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, SendIntentUsingComRpcFailureEmptyAppID)
{
    Core::hresult status;

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_EQ(Core::ERROR_GENERAL, mAppManagerImpl->SendIntent("", APPMANAGER_APP_INTENT));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for SendIntentUsingComRpcFailureLifecycleManagerRemoteObjectIsNull
 * Setting up only AppManager Plugin and creating required COM-RPC resources
 * LifecycleManager Interface object is not created and hence the API should return error
 * Verifying the return of the API
 * Releasing the AppManager Interface object only
 */

TEST_F(AppManagerTest, SendIntentUsingComRpcFailureLifecycleManagerRemoteObjectIsNull)
{
    createAppManagerImpl();
    EXPECT_EQ(Core::ERROR_GENERAL, mAppManagerImpl->SendIntent(APPMANAGER_APP_ID, APPMANAGER_APP_INTENT));
    releaseAppManagerImpl();
}

/*
 * Test Case for ClearAppDataUsingComRpcSuccess
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Implementation missing TODO: Testcase need to be added once implementation is done
 * Verifying the return of the API
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, DISABLED_ClearAppDataUsingComRpcSuccess)
{
    Core::hresult status;

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_EQ(Core::ERROR_NONE, mAppManagerImpl->ClearAppData(APPMANAGER_APP_ID));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for ClearAllAppDataUsingComRpcSuccess
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Implementation missing TODO: Testcase need to be added once implementation is done
 * Verifying the return of the API
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, DISABLED_ClearAllAppDataUsingComRpcSuccess)
{
    Core::hresult status;

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_EQ(Core::ERROR_NONE, mAppManagerImpl->ClearAllAppData());

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for GetAppMetadataUsingComRpcSuccess
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Implementation missing TODO: Testcase need to be added once implementation is done
 * Verifying the return of the API
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, GetAppMetadataUsingComRpcSuccess)
{
    Core::hresult status;
    const std::string dummymetaData = "";
    std::string dummyResult = "";

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_EQ(Core::ERROR_NONE, mAppManagerImpl->GetAppMetadata(APPMANAGER_APP_ID, dummymetaData, dummyResult));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for GetAppPropertyUsingComRpcSuccess
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Setting Mock for GetValue() to simulate getting value from persistent store
 * Verifying the return of the API
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, GetAppPropertyUsingComRpcSuccess)
{
    Core::hresult status;
    const std::string key = PERSISTENT_STORE_KEY;
    std::string value = "";

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_CALL(*mStore2Mock, GetValue(Exchange::IStore2::ScopeType::DEVICE, APPMANAGER_APP_ID, key, ::testing::_, ::testing::_))
    .WillOnce([&](const Exchange::IStore2::ScopeType scope, const string& ns, const string& key, string& value, uint32_t& ttl) {
        value = PERSISTENT_STORE_VALUE;
        return Core::ERROR_NONE;
    });

    EXPECT_EQ(Core::ERROR_NONE, mAppManagerImpl->GetAppProperty(APPMANAGER_APP_ID, key, value));
    TEST_LOG("GetAppProperty :%s %s", value.c_str(), PERSISTENT_STORE_VALUE);
    EXPECT_STREQ(value.c_str(), PERSISTENT_STORE_VALUE);

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for GetAppPropertyUsingComRpcFailureEmptyAppID
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Verifying the return of the API by passing the empty app id
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, GetAppPropertyUsingComRpcFailureEmptyAppID)
{
    Core::hresult status;
    const std::string key = "";
    std::string value = "";

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_EQ(Core::ERROR_GENERAL, mAppManagerImpl->GetAppProperty(APPMANAGER_EMPTY_APP_ID, key, value));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for GetAppPropertyUsingComRpcFailureEmptyKey
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Verifying the return of the API by passing the empty key
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, GetAppPropertyUsingComRpcFailureEmptyKey)
{
    Core::hresult status;
    const std::string key = "";
    std::string value = "";

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_EQ(Core::ERROR_GENERAL, mAppManagerImpl->GetAppProperty(APPMANAGER_APP_ID, key, value));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for GetAppPropertyUsingComRpcFailureGetAppPropertyReturnError
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Setting Mock for GetValue() to simulate error return
 * Verifying the return of the API
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, GetAppPropertyUsingComRpcFailureGetAppPropertyReturnError)
{
    Core::hresult status;
    const std::string key = PERSISTENT_STORE_KEY;
    std::string value = "";

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_CALL(*mStore2Mock, GetValue(Exchange::IStore2::ScopeType::DEVICE, APPMANAGER_APP_ID, key, ::testing::_, ::testing::_))
    .WillOnce([&](const Exchange::IStore2::ScopeType scope, const string& ns, const string& key, string& value, uint32_t& ttl) {
        return Core::ERROR_GENERAL;
    });
    EXPECT_EQ(Core::ERROR_GENERAL, mAppManagerImpl->GetAppProperty(APPMANAGER_APP_ID, key, value));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for GetAppPropertyUsingComRpcFailureLifecycleManagerRemoteObjectIsNull
 * Setting up only AppManager Plugin and creating required COM-RPC resources
 * LifecycleManager Interface object is not created and hence the API should return error
 * Verifying the return of the API
 * Releasing the AppManager Interface object only
 */

TEST_F(AppManagerTest, GetAppPropertyUsingComRpcFailureLifecycleManagerRemoteObjectIsNull)
{
    const std::string key = PERSISTENT_STORE_KEY;
    std::string value = "";

    createAppManagerImpl();

    EXPECT_EQ(Core::ERROR_GENERAL, mAppManagerImpl->GetAppProperty(APPMANAGER_APP_ID, key, value));

    releaseAppManagerImpl();
}

/*
 * Test Case for SetAppPropertyUsingComRpcSuccess
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Setting Mock for SetValue() to simulate setting value from persistent store
 * Verifying the return of the API
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, SetAppPropertyUsingComRpcSuccess)
{
    Core::hresult status;
    const std::string key = PERSISTENT_STORE_KEY;
    std::string value = "";

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_CALL(*mStore2Mock, SetValue(Exchange::IStore2::ScopeType::DEVICE, APPMANAGER_APP_ID, key, value, 0))
    .WillOnce([&](const Exchange::IStore2::ScopeType scope, const string& ns, const string& key, const string& value, const uint32_t ttl) {
        return Core::ERROR_NONE;
    });

    EXPECT_EQ(Core::ERROR_NONE, mAppManagerImpl->SetAppProperty(APPMANAGER_APP_ID, key, value));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for SetAppPropertyUsingComRpcFailureEmptyAppID
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Verifying the return of the API by passing the empty app id
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, SetAppPropertyUsingComRpcFailureEmptyAppID)
{
    Core::hresult status;
    const std::string key = PERSISTENT_STORE_KEY;
    std::string value = "";

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_EQ(Core::ERROR_GENERAL, mAppManagerImpl->SetAppProperty(APPMANAGER_EMPTY_APP_ID, key, value));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for SetAppPropertyUsingComRpcFailureEmptyKey
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Verifying the return of the API by passing the empty key
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, SetAppPropertyUsingComRpcFailureEmptyKey)
{
    Core::hresult status;
    const std::string key = "";
    std::string value = "";

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_EQ(Core::ERROR_GENERAL, mAppManagerImpl->SetAppProperty(APPMANAGER_APP_ID, key, value));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for SetAppPropertyUsingComRpcFailureSetValueReturnError
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Setting Mock for SetValue() to simulate error return
 * Verifying the return of the API
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, SetAppPropertyUsingComRpcFailureSetValueReturnError)
{
    Core::hresult status;
    const std::string key = PERSISTENT_STORE_KEY;
    std::string value = "";

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_CALL(*mStore2Mock, SetValue(Exchange::IStore2::ScopeType::DEVICE, APPMANAGER_APP_ID, key, value, 0))
    .WillOnce([&](const Exchange::IStore2::ScopeType scope, const string& ns, const string& key, const string& value, const uint32_t ttl) {
        return Core::ERROR_GENERAL;
    });
    EXPECT_EQ(Core::ERROR_GENERAL, mAppManagerImpl->SetAppProperty(APPMANAGER_APP_ID, key, value));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for SetAppPropertyUsingComRpcFailureLifecycleManagerRemoteObjectIsNull
 * Setting up only AppManager Plugin and creating required COM-RPC resources
 * LifecycleManager Interface object is not created and hence the API should return error
 * Verifying the return of the API
 * Releasing the AppManager Interface object only
 */
TEST_F(AppManagerTest, SetAppPropertyUsingComRpcFailureLifecycleManagerRemoteObjectIsNull)
{
    const std::string key = PERSISTENT_STORE_KEY;
    std::string value = "";

    createAppManagerImpl();

    EXPECT_EQ(Core::ERROR_GENERAL, mAppManagerImpl->SetAppProperty(APPMANAGER_APP_ID, key, value));

    releaseAppManagerImpl();
}

/*
 * Test Case for GetMaxRunningAppUsingComRpcSuccess
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Implementation missing TODO: Testcase need to be added once implementation is done
 * Verifying the return of the API
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, GetMaxRunningAppUsingComRpcSuccess)
{
    Core::hresult status;
    int32_t maxRunningApps = 0;

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_EQ(Core::ERROR_NONE, mAppManagerImpl->GetMaxRunningApps(maxRunningApps));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for GetMaxHibernatedAppsUsingComRpcSuccess
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Implementation missing TODO: Testcase need to be added once implementation is done
 * Verifying the return of the API
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, GetMaxHibernatedAppsUsingComRpcSuccess)
{
    Core::hresult status;
    int32_t maxHibernatedApps = 0;

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_EQ(Core::ERROR_NONE, mAppManagerImpl->GetMaxHibernatedApps(maxHibernatedApps));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for GetMaxHibernatedFlashUsageUsingComRpcSuccess
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Implementation missing TODO: Testcase need to be added once implementation is done
 * Verifying the return of the API
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, GetMaxHibernatedFlashUsageUsingComRpcSuccess)
{
    Core::hresult status;
    int32_t maxHibernatedFlashUsage = 0;

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_EQ(Core::ERROR_NONE, mAppManagerImpl->GetMaxHibernatedFlashUsage(maxHibernatedFlashUsage));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for GetMaxInactiveRamUsageUsingComRpcSuccess
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Implementation missing TODO: Testcase need to be added once implementation is done
 * Verifying the return of the API
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, GetMaxInactiveRamUsageUsingComRpcSuccess)
{
    Core::hresult status;
    int32_t maxInactiveRamUsage = 0;

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_EQ(Core::ERROR_NONE, mAppManagerImpl->GetMaxInactiveRamUsage(maxInactiveRamUsage));

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for updateCurrentActionUsingComRpcSuccess
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Setting Mock for ListPackages() to simulate getting installed package list
 * Setting Mock for Lock() to simulate lockId and unpacked path
 * Setting Mock for IsAppLoaded() to simulate the package is loaded or not
 * Setting Mock for SetTargetAppState() to simulate setting the state
 * Setting Mock for SpawnApp() to simulate spawning a app and gettign the appinstance id
 * Calling LaunchApp first so that all the prerequisite will be filled
 * Verifying the return of the API
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, DISABLED_updateCurrentActionUsingComRpcSuccess)
{
    Core::hresult status;

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    LaunchAppPreRequisite(Exchange::ILifecycleManager::LifecycleState::ACTIVE);
    EXPECT_EQ(Core::ERROR_NONE, mAppManagerImpl->LaunchApp(APPMANAGER_APP_ID, APPMANAGER_APP_INTENT, APPMANAGER_APP_LAUNCHARGS));

    mAppManagerImpl->updateCurrentAction(APPMANAGER_APP_ID, Plugin::AppManagerImplementation::CurrentAction::APP_ACTION_LAUNCH);

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for updateCurrentActionUsingComRpcFailureAppIDNotExist
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Verifying the return of the API when app id doesn't exist
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, updateCurrentActionUsingComRpcFailureAppIDNotExist)
{
    Core::hresult status;

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    mAppManagerImpl->updateCurrentAction(APPMANAGER_APP_ID, Plugin::AppManagerImplementation::CurrentAction::APP_ACTION_LAUNCH);

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

/*
 * Test Case for handleOnAppLifecycleStateChangedUsingComRpcSuccess
 * Setting up AppManager/LifecycleManager/LifecycleManagerState/PersistentStore/PackageManagerRDKEMS Plugin and creating required COM-RPC resources
 * Verifying the return of the API
 * Releasing the AppManager interface and all related test resources
 */
TEST_F(AppManagerTest, DISABLED_handleOnAppLifecycleStateChangedUsingComRpcSuccess)
{
    Core::hresult status;

    status = createResources();
    EXPECT_EQ(Core::ERROR_NONE, status);

    mAppManagerImpl->handleOnAppLifecycleStateChanged(
        APPMANAGER_APP_ID,
        APPMANAGER_APP_INSTANCE,
        Exchange::IAppManager::AppLifecycleState::APP_STATE_UNKNOWN,
        Exchange::IAppManager::AppLifecycleState::APP_STATE_UNLOADED,
        Exchange::IAppManager::AppErrorReason::APP_ERROR_NONE);

    if(status == Core::ERROR_NONE)
    {
        releaseResources();
    }
}

