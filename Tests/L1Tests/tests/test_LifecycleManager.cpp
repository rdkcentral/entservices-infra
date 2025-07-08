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
#include "ServiceMock.h"
#include "RuntimeManagerMock.h"
#include "WindowManagerMock.h"
#include "COMLinkMock.h"
#include "ThunderPortability.h"

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);
#define DEBUG_PRINTF(fmt, ...) \
    std::printf("[DEBUG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

using ::testing::NiceMock;
using namespace WPEFramework;

namespace {
const string callSign = _T("LifecycleManager");
}

class LifecycleManagerTest : public ::testing::Test {
protected:
    string appId;
    string launchIntent;
    string environmentVars;
    bool enableDebugger;
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

    LifecycleManagerTest()
    {
	DEBUG_PRINTF("ERROR: RDKEMW-2806");
        mLifecycleManagerImpl = Core::ProxyType<Plugin::LifecycleManagerImplementation>::Create();
        
        interface = static_cast<Exchange::ILifecycleManager*>(mLifecycleManagerImpl->QueryInterface(Exchange::ILifecycleManager::ID));
    }

    virtual ~LifecycleManagerTest() override
    {
	DEBUG_PRINTF("ERROR: RDKEMW-2806");
        interface->Release();
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
	DEBUG_PRINTF("ERROR: RDKEMW-2806");
        
        ASSERT_TRUE(interface != nullptr);
    }

    void TearDown() override
    {
	DEBUG_PRINTF("ERROR: RDKEMW-2806");
        ASSERT_TRUE(interface != nullptr);
    }

    void createResources()
    {
	DEBUG_PRINTF("ERROR: RDKEMW-2806");
        // Set up mocks
        mServiceMock = new NiceMock<ServiceMock>;
        mRuntimeManagerMock = new NiceMock<RuntimeManagerMock>;
        mWindowManagerMock = new NiceMock<WindowManagerMock>;

        mLifecycleManagerConfigure = static_cast<Exchange::IConfiguration*>(mLifecycleManagerImpl->QueryInterface(Exchange::IConfiguration::ID));

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
                     return 0;
                    }));
        }

        if (mRuntimeManagerMock != nullptr)
        {
	    DEBUG_PRINTF("ERROR: RDKEMW-2806");
            EXPECT_CALL(*mRuntimeManagerMock, Release())
                .WillOnce(::testing::Invoke(
                [&]() {
                     delete mRuntimeManagerMock;
                     return 0;
                    }));
        }

        if (mWindowManagerMock != nullptr)
        {
	    DEBUG_PRINTF("ERROR: RDKEMW-2806");
            EXPECT_CALL(*mWindowManagerMock, Release())
                .WillOnce(::testing::Invoke(
                [&]() {
                     delete mWindowManagerMock;
                     return 0;
                    }));
        }
	DEBUG_PRINTF("ERROR: RDKEMW-2806");
        mLifecycleManagerConfigure->Release();
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
    
    releaseResources();
    DEBUG_PRINTF("ERROR: RDKEMW-2806");
}

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

    EXPECT_EQ(apps, "\"[{\\\"appId\\\":\\\"com.test.app\\\",\\\"type\\\":1,\\\"lifecycleState\\\":0,\\\"targetLifecycleState\\\":2,\\\"activeSessionId\\\":\\\"\\\",\\\"appInstanceId\\\":\\\"\\\"}]\"");

    releaseResources();
}

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

TEST_F(LifecycleManagerTest, unloadApp_afterSpawnApp)
{
    createResources();

    appInstanceId = "test.app.instance";

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    // TC-18: Unload the app after spawning
    EXPECT_EQ(Core::ERROR_NONE, interface->UnloadApp(appInstanceId, errorReason, success));

    releaseResources();
}

TEST_F(LifecycleManagerTest, unloadApp_onSpawnAppFailure)
{
    createResources();

    appInstanceId = "test.app.instance";

    EXPECT_EQ(Core::ERROR_GENERAL, interface->SpawnApp("", "", Exchange::ILifecycleManager::LifecycleState::UNLOADED, runtimeConfigObject, "", appInstanceId, errorReason, success));

    // TC-19: Unload the app after spawn fails
    EXPECT_EQ(Core::ERROR_GENERAL, interface->UnloadApp(appInstanceId, errorReason, success));

    releaseResources();
}

TEST_F(LifecycleManagerTest, killApp_afterSpawnApp)
{
    createResources();

    appInstanceId = "test.app.instance";

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    // TC-20: Kill the app after spawning
    EXPECT_EQ(Core::ERROR_NONE, interface->KillApp(appInstanceId, errorReason, success));

    releaseResources();
}

TEST_F(LifecycleManagerTest, killApp_onSpawnAppFailure)
{
    createResources();

    appInstanceId = "test.app.instance";

    EXPECT_EQ(Core::ERROR_GENERAL, interface->SpawnApp("", "", Exchange::ILifecycleManager::LifecycleState::UNLOADED, runtimeConfigObject, "", appInstanceId, errorReason, success));

    // TC-21: Kill the app after spawn fails
    EXPECT_EQ(Core::ERROR_GENERAL, interface->KillApp(appInstanceId, errorReason, success));

    releaseResources();
}

TEST_F(LifecycleManagerTest, sendIntenttoActiveApp_afterSpawnApp)
{
    createResources();

    appInstanceId = "test.app.instance";
    string intent = "test.intent";

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    // TC-22: Send intent to the app after spawning
    EXPECT_EQ(Core::ERROR_NONE, interface->SendIntentToActiveApp(appInstanceId, intent, errorReason, success));

    //EXPECT_EQ(, "\"[{\\\"loaded\\\":false}]\"");

    releaseResources();
}

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
