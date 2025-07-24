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

#include "LifecycleManager.h"
#include "LifecycleManagerImplementation.h"
#include "State.h"
#include "ServiceMock.h"
#include "RuntimeManagerMock.h"
#include "WindowManagerMock.h"
#include "COMLinkMock.h"
#include "ThunderPortability.h"
#include "WorkerPoolImplementation.h"

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);
#define DEBUG_PRINTF(fmt, ...) \
    std::printf("[DEBUG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

using ::testing::NiceMock;
using namespace WPEFramework;

#ifdef UNIT_TEST
void post_mAppRunningsem(Plugin::ApplicationContext *context);
#endif

class LifecycleManagerTest : public ::testing::Test {
protected:
    string appId;
    string launchIntent;
    Exchange::ILifecycleManager::LifecycleState targetLifecycleState;
    Exchange::RuntimeConfig runtimeConfigObject;
    string launchArgs;
    string appInstanceId;
    string errorReason;
    bool success;
    Core::ProxyType<Plugin::LifecycleManagerImplementation> mLifecycleManagerImpl;
    Exchange::ILifecycleManager* interface = nullptr;
    Exchange::IConfiguration* mLifecycleManagerConfigure = nullptr;
    RuntimeManagerMock* mRuntimeManagerMock = nullptr;
    WindowManagerMock* mWindowManagerMock = nullptr;
    ServiceMock* mServiceMock = nullptr;
    Core::ProxyType<WorkerPoolImplementation> workerPool;

    LifecycleManagerTest()
	: workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(
            2, Core::Thread::DefaultStackSize(), 16))
    {
	DEBUG_PRINTF("ERROR: RDKEMW-2806");
        mLifecycleManagerImpl = Core::ProxyType<Plugin::LifecycleManagerImplementation>::Create();
        
        interface = static_cast<Exchange::ILifecycleManager*>(mLifecycleManagerImpl->QueryInterface(Exchange::ILifecycleManager::ID));

	Core::IWorkerPool::Assign(&(*workerPool));
	workerPool->Run();
    }

    virtual ~LifecycleManagerTest() override
    {
	DEBUG_PRINTF("ERROR: RDKEMW-2806");

	Core::IWorkerPool::Assign(nullptr);
	workerPool.Release();

	interface->Release();

    DEBUG_PRINTF("ERROR: RDKEMW-2806");
    }

    void SetUp() override 
    {
	DEBUG_PRINTF("ERROR: RDKEMW-2806");
        // Initialize the parameters with default values
        appId = "com.test.app";
        launchIntent = "test.launch.intent";
        targetLifecycleState = Exchange::ILifecycleManager::LifecycleState::LOADING;
        launchArgs = "test.arguments";
        appInstanceId = "";
        errorReason = "";
        success = true;
        
        runtimeConfigObject = {
            true,true,true,1024,512,"test.env.variables",1,1,1024,true,"test.dial.id","test.command","test.app.type","test.app.path","test.runtime.path","test.logfile.path",1024,"test.log.levels",true,"test.fkps.files","test.firebolt.version",true,"test.unpacked.path"
        };
        
        ASSERT_TRUE(interface != nullptr);

        DEBUG_PRINTF("ERROR: RDKEMW-2806");
    }

    void TearDown() override
    {
	DEBUG_PRINTF("ERROR: RDKEMW-2806");
        ASSERT_TRUE(interface != nullptr);
        DEBUG_PRINTF("ERROR: RDKEMW-2806");
    }

    void createResources()
    {
	DEBUG_PRINTF("ERROR: RDKEMW-2806");
        // Set up mocks
        mServiceMock = new NiceMock<ServiceMock>;
        mRuntimeManagerMock = new NiceMock<RuntimeManagerMock>;
        mWindowManagerMock = new NiceMock<WindowManagerMock>;

        mLifecycleManagerConfigure = static_cast<Exchange::IConfiguration*>(mLifecycleManagerImpl->QueryInterface(Exchange::IConfiguration::ID));

        DEBUG_PRINTF("ERROR: RDKEMW-2806");

        EXPECT_CALL(*mServiceMock, QueryInterfaceByCallsign(::testing::_, ::testing::_))
          .Times(::testing::AnyNumber())
          .WillRepeatedly(::testing::Invoke(
              [&](const uint32_t, const std::string& name) -> void* {
                if (name == "org.rdk.RuntimeManager") {
                    return reinterpret_cast<void*>(mRuntimeManagerMock);
                } else if (name == "org.rdk.RDKWindowManager") {
                   return reinterpret_cast<void*>(mWindowManagerMock);
                } 
            return nullptr;
        }));

	//EXPECT_CALL(*mRuntimeManagerMock, AddRef())
     //       .Times(::testing::AnyNumber());

    EXPECT_CALL(*mServiceMock, AddRef())
            .Times(::testing::AnyNumber());

    DEBUG_PRINTF("ERROR: RDKEMW-2806");

	EXPECT_CALL(*mRuntimeManagerMock, Register(::testing::_))
            .WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    //DEBUG_PRINTF("ERROR: RDKEMW-2806");

	//EXPECT_CALL(*mWindowManagerMock, AddRef())
     //       .Times(::testing::AnyNumber());

    DEBUG_PRINTF("ERROR: RDKEMW-2806");

	EXPECT_CALL(*mWindowManagerMock, Register(::testing::_))
            .WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    DEBUG_PRINTF("ERROR: RDKEMW-2806");

        // Configure the LifecycleManager
        mLifecycleManagerConfigure->Configure(mServiceMock);
	DEBUG_PRINTF("ERROR: RDKEMW-2806");
    }

    void releaseResources()
    {
	DEBUG_PRINTF("ERROR: RDKEMW-2806");
        // Clean up mocks
        if (mServiceMock != nullptr)
        {
	    DEBUG_PRINTF("ERROR: RDKEMW-2806");

        EXPECT_CALL(*mServiceMock, Release())
                .WillOnce(::testing::Invoke(
                [&]() {
                     delete mServiceMock;
		     mServiceMock = nullptr;
                     return 0;
                    }));

            DEBUG_PRINTF("ERROR: RDKEMW-2806");

        }

        if (mRuntimeManagerMock != nullptr)
        {
	    DEBUG_PRINTF("ERROR: RDKEMW-2806");
		
	    EXPECT_CALL(*mRuntimeManagerMock, Unregister(::testing::_))
                .WillRepeatedly(::testing::Return(Core::ERROR_NONE));

        EXPECT_CALL(*mRuntimeManagerMock, Release())
                .WillOnce(::testing::Invoke(
                [&]() {
                     delete mRuntimeManagerMock;
		     mRuntimeManagerMock = nullptr;
                     return 0;
                    }));

        DEBUG_PRINTF("ERROR: RDKEMW-2806");

        }

        if (mWindowManagerMock != nullptr)
        {
	    DEBUG_PRINTF("ERROR: RDKEMW-2806");
		
	    EXPECT_CALL(*mWindowManagerMock, Unregister(::testing::_))
                .WillRepeatedly(::testing::Return(Core::ERROR_NONE));

        EXPECT_CALL(*mWindowManagerMock, Release())
                .WillOnce(::testing::Invoke(
                [&]() {
                     delete mWindowManagerMock;
		     mWindowManagerMock = nullptr;
                     return 0;
                    }));

        DEBUG_PRINTF("ERROR: RDKEMW-2806");
        }
	DEBUG_PRINTF("ERROR: RDKEMW-2806");
        mLifecycleManagerConfigure->Release();

        DEBUG_PRINTF("ERROR: RDKEMW-2806");

    }
};

TEST_F(LifecycleManagerTest, spawnApp_withValidParams)
{
    DEBUG_PRINTF("ERROR: RDKEMW-2806");
    createResources();
    DEBUG_PRINTF("ERROR: RDKEMW-2806");

    // TC-1: Spawn an app with all parameters valid
    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    DEBUG_PRINTF("ERROR: RDKEMW-2806");	
    ::testing::Mock::VerifyAndClearExpectations(mServiceMock);
    DEBUG_PRINTF("ERROR: RDKEMW-2806");
    ::testing::Mock::VerifyAndClearExpectations(mRuntimeManagerMock);
    DEBUG_PRINTF("ERROR: RDKEMW-2806");
    ::testing::Mock::VerifyAndClearExpectations(mWindowManagerMock);
    
    releaseResources();
    DEBUG_PRINTF("ERROR: RDKEMW-2806");
}

#if 0
TEST_F(LifecycleManagerTest, spawnApp_withInvalidParams)
{
    createResources();
    
    // TC-2: Spawn an app with all parameters invalid 
    EXPECT_EQ(Core::ERROR_GENERAL, interface->SpawnApp("", "", Exchange::ILifecycleManager::LifecycleState::UNLOADED, runtimeConfigObject, "", appInstanceId, errorReason, success));

    // TC-3: Spawn an app with appId as invalid
    EXPECT_EQ(Core::ERROR_GENERAL, interface->SpawnApp("", launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    // TC-4: Spawn an app with launchIntent as invalid
    EXPECT_EQ(Core::ERROR_GENERAL, interface->SpawnApp(appId, "", targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));
	
    // TC-5: Spawn an app with targetLifecycleState as invalid
    EXPECT_EQ(Core::ERROR_GENERAL, interface->SpawnApp(appId, launchIntent, Exchange::ILifecycleManager::LifecycleState::UNLOADED, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));
    
    // TC-6: Spawn an app with launchArgs as invalid
    EXPECT_EQ(Core::ERROR_GENERAL, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, "", appInstanceId, errorReason, success));

    releaseResources();
}
#endif

TEST_F(LifecycleManagerTest, isAppLoaded_onSpawnAppSuccess) 
{
    createResources();

    bool loaded = false;

    // TC-7: Check if app is loaded after spawning
    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    EXPECT_EQ(Core::ERROR_NONE, interface->IsAppLoaded(appId, loaded));

    EXPECT_EQ(loaded, true);

    releaseResources();
}

#if 0
TEST_F(LifecycleManagerTest, isAppLoaded_onSpawnAppFailure)
{
    createResources();

    bool loaded = true;

    // TC-8: Check that app is not loaded after spawnApp fails
    EXPECT_EQ(Core::ERROR_GENERAL, interface->SpawnApp("", "", Exchange::ILifecycleManager::LifecycleState::UNLOADED, runtimeConfigObject, "", appInstanceId, errorReason, success));

    EXPECT_EQ(Core::ERROR_NONE, interface->IsAppLoaded(appId, loaded));

    EXPECT_EQ(loaded, false);

    releaseResources();
}

TEST_F(LifecycleManagerTest, isAppLoaded_oninvalidAppId)
{
    createResources();
	
    bool loaded = true;

    // TC-9: Verify error on passing an invalid appId
    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    EXPECT_EQ(Core::ERROR_GENERAL, interface->IsAppLoaded("", loaded));

    releaseResources();
}
#endif

TEST_F(LifecycleManagerTest, getLoadedApps_verboseEnabled)
{
    createResources();

    bool verbose = true;
    string apps = "";

    // TC-10: Get loaded apps with verbose enabled
    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    EXPECT_EQ(Core::ERROR_NONE, interface->GetLoadedApps(verbose, apps));

    //EXPECT_EQ(apps, "\"[{\\\"appId\\\":\\\"com.test.app\\\",\\\"type\\\":1,\\\"lifecycleState\\\":0,\\\"targetLifecycleState\\\":2,\\\"activeSessionId\\\":\\\"\\\",\\\"appInstanceId\\\":\\\"\\\"}]\"");

    releaseResources();
}

TEST_F(LifecycleManagerTest, getLoadedApps_verboseDisabled)
{
    createResources();

    bool verbose = false;
    string apps = "";

    // TC-11: Get loaded apps with verbose disabled
    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    EXPECT_EQ(Core::ERROR_NONE, interface->GetLoadedApps(verbose, apps));

    //EXPECT_EQ(apps, "\"[{\\\"appId\\\":\\\"com.test.app\\\",\\\"type\\\":1,\\\"lifecycleState\\\":0,\\\"targetLifecycleState\\\":2,\\\"activeSessionId\\\":\\\"\\\",\\\"appInstanceId\\\":\\\"\\\"}]\"");

    releaseResources();
}
#if 0
TEST_F(LifecycleManagerTest, getLoadedApps_noAppsLoaded)
{
    createResources();

    bool verbose = true;
    string apps = "";

    // TC-12: Check that no apps are loaded
    EXPECT_EQ(Core::ERROR_NONE, interface->GetLoadedApps(verbose, apps));

    EXPECT_EQ(apps, "\"[]\"");

    releaseResources();
}
#endif

TEST_F(LifecycleManagerTest, setTargetAppState_withValidParams)
{
    createResources();

    appInstanceId = "test.app.instance";

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    // TC-13: Set the target state of a loaded app with all parameters valid
    EXPECT_EQ(Core::ERROR_NONE, interface->SetTargetAppState(appInstanceId, targetLifecycleState, launchIntent));

    // TC-14: Set the target state of a loaded app with only required parameters valid
    EXPECT_EQ(Core::ERROR_NONE, interface->SetTargetAppState(appInstanceId, targetLifecycleState, ""));

    releaseResources();
}

#if 0
TEST_F(LifecycleManagerTest, setTargetAppState_withinvalidParams)
{
    createResources();

    appInstanceId = "test.app.instance";

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    // TC-15: Set the target state of a loaded app with invalid appInstanceId
    EXPECT_EQ(Core::ERROR_GENERAL, interface->SetTargetAppState("", targetLifecycleState, launchIntent));

    // TC-16: Set the target state of a loaded app with invalid targetLifecycleState
    EXPECT_EQ(Core::ERROR_GENERAL, interface->SetTargetAppState(appInstanceId, Exchange::ILifecycleManager::LifecycleState::UNLOADED, launchIntent));

    // TC-17: Set the target state of a loaded app with all parameters invalid
    EXPECT_EQ(Core::ERROR_GENERAL, interface->SetTargetAppState("", Exchange::ILifecycleManager::LifecycleState::UNLOADED, launchIntent));

    releaseResources();
}
#endif


TEST_F(LifecycleManagerTest, unloadApp_withValidParams)
{
    createResources();

    //appInstanceId = "test.app.instance";

    EXPECT_CALL(*mRuntimeManagerMock, Run(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const string& appInstanceId, const uint32_t userId, const uint32_t groupId, Exchange::IRuntimeManager::IValueIterator* const& ports, Exchange::IRuntimeManager::IStringIterator* const& paths, Exchange::IRuntimeManager::IStringIterator* const& debugSettings, const Exchange::RuntimeConfig& runtimeConfigObject) {
                return Core::ERROR_NONE;
          }));

    #if 0
    EXPECT_CALL(*mRuntimeManagerMock, Resume(appInstanceId))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appInstanceId) {
                return Core::ERROR_NONE;
          }));
    #endif

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));
    
    Plugin::LifecycleManagerImplementation LifecycleManagerImpl;
    Plugin::ApplicationContext *context = LifecycleManagerImpl.getContext("", appId);

    // TC-18: Unload the app after spawning
    EXPECT_EQ(Core::ERROR_NONE, interface->UnloadApp(appInstanceId, errorReason, success));

    post_mAppRunningsem(context);

    releaseResources();
}

#if 0
TEST_F(LifecycleManagerTest, unloadApp_onSpawnAppFailure)
{
    createResources();

    appInstanceId = "test.app.instance";

    EXPECT_EQ(Core::ERROR_GENERAL, interface->SpawnApp("", "", Exchange::ILifecycleManager::LifecycleState::UNLOADED, runtimeConfigObject, "", appInstanceId, errorReason, success));

    // TC-19: Unload the app after spawn fails
    EXPECT_EQ(Core::ERROR_GENERAL, interface->UnloadApp(appInstanceId, errorReason, success));

    releaseResources();
}
#endif

TEST_F(LifecycleManagerTest, killApp_withValidParams)
{
    createResources();

    //appInstanceId = "test.app.instance";

    EXPECT_CALL(*mRuntimeManagerMock, Run(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const string& appInstanceId, const uint32_t userId, const uint32_t groupId, Exchange::IRuntimeManager::IValueIterator* const& ports, Exchange::IRuntimeManager::IStringIterator* const& paths, Exchange::IRuntimeManager::IStringIterator* const& debugSettings, const Exchange::RuntimeConfig& runtimeConfigObject) {
                return Core::ERROR_NONE;
          }));

    #if 0
    EXPECT_CALL(*mRuntimeManagerMock, Resume(appInstanceId))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appInstanceId) {
                return Core::ERROR_NONE;
          }));
    #endif

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    // TC-20: Kill the app after spawning
    EXPECT_EQ(Core::ERROR_NONE, interface->KillApp(appInstanceId, errorReason, success));

    releaseResources();
}

#if 0
TEST_F(LifecycleManagerTest, killApp_onSpawnAppFailure)
{
    createResources();

    appInstanceId = "test.app.instance";

    EXPECT_EQ(Core::ERROR_GENERAL, interface->SpawnApp("", "", Exchange::ILifecycleManager::LifecycleState::UNLOADED, runtimeConfigObject, "", appInstanceId, errorReason, success));

    // TC-21: Kill the app after spawn fails
    EXPECT_EQ(Core::ERROR_GENERAL, interface->KillApp(appInstanceId, errorReason, success));

    releaseResources();
}
#endif

TEST_F(LifecycleManagerTest, sendIntenttoActiveApp_withValidParams)
{
    createResources();

    //appInstanceId = "test.app.instance";
    string intent = "test.intent";

    EXPECT_CALL(*mRuntimeManagerMock, Run(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const string& appInstanceId, const uint32_t userId, const uint32_t groupId, Exchange::IRuntimeManager::IValueIterator* const& ports, Exchange::IRuntimeManager::IStringIterator* const& paths, Exchange::IRuntimeManager::IStringIterator* const& debugSettings, const Exchange::RuntimeConfig& runtimeConfigObject) {
                return Core::ERROR_NONE;
          }));

    #if 0
    EXPECT_CALL(*mRuntimeManagerMock, Resume(appInstanceId))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appInstanceId) {
                return Core::ERROR_NONE;
          }));
    #endif

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    // TC-22: Send intent to the app after spawning
    EXPECT_EQ(Core::ERROR_NONE, interface->SendIntentToActiveApp(appInstanceId, intent, errorReason, success));
    //EXPECT_EQ(, "\"[{\\\"loaded\\\":false}]\"");

    releaseResources();
}

#if 0
TEST_F(LifecycleManagerTest, sendIntenttoActiveApp_onSpawnAppFailure)
{
    createResources();

    appInstanceId = "test.app.instance";
    string intent = "test.intent";

    EXPECT_EQ(Core::ERROR_GENERAL, interface->SpawnApp("", "", Exchange::ILifecycleManager::LifecycleState::UNLOADED, runtimeConfigObject, "", appInstanceId, errorReason, success));

    // TC-23: Send intent to the app after spawn fails
    EXPECT_EQ(Core::ERROR_GENERAL, interface->SendIntentToActiveApp(appInstanceId, intent, errorReason, success));

    releaseResources();
}

TEST_F(LifecycleManagerTest, sendIntenttoActiveApp_withinvalidParams)
{
    createResources();

    appInstanceId = "test.app.instance";
    string intent = "test.intent";

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    // TC-24: Send intent to the app after spawn fails with both parameters invalid
    EXPECT_EQ(Core::ERROR_GENERAL, interface->SendIntentToActiveApp("", "", errorReason, success));

    // TC-25: Send intent to the app after spawn fails with appInstanceId invalid
    EXPECT_EQ(Core::ERROR_GENERAL, interface->SendIntentToActiveApp("", intent, errorReason, success));

    // TC-26: Send intent to the app after spawn fails with intent invalid
    EXPECT_EQ(Core::ERROR_GENERAL, interface->SendIntentToActiveApp(appInstanceId, "", errorReason, success));

    releaseResources();
}
#endif
