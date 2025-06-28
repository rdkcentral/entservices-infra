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
#include "LifecycleManagerMock.h"
#include "RuntimeManagerMock.h"
#include "WindowManagerMock.h"
#include "COMLinkMock.h"
#include "ThunderPortability.h"

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);

using ::testing::NiceMock;
using namespace WPEFramework;

namespace {
const string callSign = _T("LifecycleManager");
}

class LifecycleManagerTest : public ::testing::Test {
protected:
    Core::ProxyType<Plugin::LifecycleManager> mPlugin;
    Core::JSONRPC::Handler& mHandler;
    DECL_CORE_JSONRPC_CONX connection;
    string mResponse;
    Core::ProxyType<Plugin::LifecycleManagerImplementation> LifecycleManagerImpl;
    WPEFramework::Exchange::RuntimeConfig runtimeConfigObject;
    NiceMock<COMLinkMock> mComLinkMock;
    NiceMock<ServiceMock> mService;
    RuntimeManagerMock* mRuntimeManagerMock = nullptr;
    WindowManagerMock* mWindowManagerMock = nullptr;
    ServiceMock* mServiceMock = nullptr;
    // Currently, this is used for TerminateApp test cases, as it depends on the LifecycleManager state change.
    // This should be removed once the LifecycleManager notification is handled here.
    //Plugin::LifecycleManagerImplementation *mLifecycleManagerImpl;

    void SetUp() override 
    {
        // Initialize the runtimeConfigObject with default values
        runtimeConfigObject = {
            true,true,true,1024,512,"test.env.variables",1,1,1024,true,"test.dial.id","test.command","test.app.type","test.app.path","test.runtime.path","test.logfile.path",1024,"test.log.levels",true,"test.fkps.files","test.firebolt.version",true,"test.unpacked.path"
        };
    }

    void TearDown() override
    {

    }

    void createResources()
    {
        // Set up mocks and dependencies
        ON_CALL(mService, COMLink())
            .WillByDefault(::testing::Invoke(
                [this]() {
                    TEST_LOG("Pass created comLinkMock: %p ", &mComLinkMock);
                    return &mComLinkMock;
                }));

        mServiceMock = new NiceMock<ServiceMock>;
        mRuntimeManagerMock = new NiceMock<RuntimeManagerManagerMock>;
        mWindowManagerMock = new NiceMock<WindowManagerMock>;

        EXPECT_CALL(mService, QueryInterfaceByCallsign(::testing::_, ::testing::_))
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

        // Initialize the plugin
        EXPECT_EQ(string(""), mPlugin->Initialize(&mService));

        // Get the LifecycleManagerImplementation instance
        // This should be removed once the LifecycleManager notification is handled here.
        //mLifecycleManagerImpl = Plugin::LifecycleManagerImplementation::getInstance();
    }

    void releaseResources()
    {
        // Deinitialize the plugin
        mPlugin->Deinitialize(&mService);
        // This should be removed once the LifecycleManager notification is handled here.
        //mLifecycleManagerImpl = nullptr;

        // Clean up mocks
        if (mServiceMock != nullptr)
        {
            delete mServiceMock;
            mServiceMock = nullptr;
        }
        if (mRuntimeManagerMock != nullptr)
        {
            delete mRuntimeManagerMock;
            mRuntimeManagerMock = nullptr;
        }
        if (mWindowManagerMock != nullptr)
        {
            delete mWindowManagerMock;
            mWindowManagerMock = nullptr;
        }
    }

    std::string runtimeConfigtoJson(const WPEFramework::Exchange::RuntimeConfig &runtimeConfigObject)
    {
        std::ostringstream oss;
            oss << "{";
            oss << "\"dial\":"   << (runtimeConfigObject.dial ? "true" : "false") << ",";
            oss << "\"wanLanAccess\":"  << (runtimeConfigObject.wanLanAccess ? "true" : "false") << ",";
            oss << "\"thunder\":"     << (runtimeConfigObject.thunder ? "true" : "false") << ",";
            oss << "\"systemMemoryLimit\":"     << runtimeConfigObject.systemMemoryLimit << ",";
            oss << "\"gpuMemoryLimit\":"     << runtimeConfigObject.gpuMemoryLimit << ",";
            oss << "\"envVariables\":\""   << runtimeConfigObject.envVariables << "\",";
            oss << "\"userId\":"     << runtimeConfigObject.userId << ",";
            oss << "\"groupId\":"     << runtimeConfigObject.groupId << ",";
            oss << "\"dataImageSize\":"     << runtimeConfigObject.dataImageSize << ",";
            oss << "\"resourceManagerClientEnabled\":"   << (runtimeConfigObject.resourceManagerClientEnabled ? "true" : "false") << ",";
            oss << "\"dialId\":\""  << runtimeConfigObject.dialId << "\",";
            oss << "\"command\":\""   << runtimeConfigObject.command << "\",";
            oss << "\"appType\":\""    << runtimeConfigObject.appType << "\",";
            oss << "\"appPath\":\""  << runtimeConfigObject.appPath << "\",";
            oss << "\"runtimePath\":\""<< runtimeConfigObject.runtimePath << "\",";
            oss << "\"logFilePath\":\""   << runtimeConfigObject.logFilePath << "\",";
            oss << "\"logFileMaxSize\":"    << runtimeConfigObject.logFileMaxSize << ",";
            oss << "\"logLevels\":\""  << runtimeConfigObject.logLevels << "\",";
            oss << "\"mapi\":"     << (runtimeConfigObject.mapi ? "true" : "false") << ",";
            oss << "\"fkpsFiles\":\""  << runtimeConfigObject.fkpsFiles << "\",";
            oss << "\"fireboltVersion\":\""<< runtimeConfigObject.fireboltVersion << "\",";
            oss << "\"enableDebugger\":"  << (runtimeConfigObject.enableDebugger ? "true" : "false") << ",";
            oss << "\"unpackedPath\":\"" << runtimeConfigObject.unpackedPath << "\"";
            oss << "}";
            return oss.str();
    }


    LifecycleManagerTest()
        : mPlugin(Core::ProxyType<Plugin::LifecycleManager>::Create())
        , mHandler(*(mPlugin))
        , INIT_CONX(1, 0)
    {

    }

    virtual ~LifecycleManagerTest() override
    {

    }
};



TEST_F(LifecycleManagerTest, RegisteredMethods)
{
    createResources();
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Exists(_T("spawnApp")));
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Exists(_T("getLoadedApps")));
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Exists(_T("isAppLoaded")));
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Exists(_T("setTargetAppState")));
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Exists(_T("unloadApp")));
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Exists(_T("killApp")));
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Exists(_T("sendIntenttoActiveApp")));
    releaseResources();
}

TEST_F(LifecycleManagerTest, spawnApp_withValidParams)
{
    createResources();

    // TC-1: Spawn an app with all parameters valid
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("spawnApp"),
        _T("{\"appId\":\"com.test.app\",\"appPath\":\"test.app.path\",\"appConfig\":\"test.app.config\",\"runtimeAppId\":\"test.runtime.app\",\"runtimePath\":\"test.runtime.path\",\"runtimeConfig\":\"test.runtime.config\",\"launchIntent\":\"test.launch.intent\",\"environmentVars\":\"test.env.vars\",\"enableDebugger\":true,\"targetLifecycleState\":Exchange::ILifecycleManager::LifecycleState::ACTIVE,\"runtimeConfigObject\":" + runtimeConfigtoJson(runtimeConfigObject) + ",\"launchArgs\":\"test.arguments\",\"appInstanceId\":\"\",\"errorReason\":\"\",\"success\":true}"),
         mResponse));

    // TC-2: Spawn an app with only required parameters as valid
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("spawnApp"),
        _T("{\"appId\":\"com.test.app\",\"appPath\":\"test.app.path\",\"appConfig\":\"test.app.config\",\"runtimeAppId\":\"\",\"runtimePath\":\"\",\"runtimeConfig\":\"\",\"launchIntent\":\"\",\"environmentVars\":\"test.env.vars\",\"enableDebugger\":true,\"targetLifecycleState\":Exchange::ILifecycleManager::LifecycleState::ACTIVE,\"runtimeConfigObject\":" + runtimeConfigtoJson(runtimeConfigObject) + ",\"launchArgs\":\"test.arguments\",\"appInstanceId\":\"\",\"errorReason\":\"\",\"success\":true}"),
         mResponse));
    
    releaseResources();
}

TEST_F(LifecycleManagerTest, spawnApp_withInvalidParams)
{
    createResources();

    // TC-3: Spawn an app with all parameters invalid 
    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection,
        _T("spawnApp"),
        _T("{\"appId\":\"\",\"appPath\":\"\",\"appConfig\":\"\",\"runtimeAppId\":\"\",\"runtimePath\":\"\",\"runtimeConfig\":\"\",\"launchIntent\":\"\",\"environmentVars\":\"\",\"enableDebugger\":false,\"targetLifecycleState\":Exchange::ILifecycleManager::LifecycleState::UNLOADED,\"runtimeConfigObject\":null,\"launchArgs\":\"\",\"appInstanceId\":\"\",\"errorReason\":\"\",\"success\":false}"),
         mResponse));

    // TC-4: Spawn an app with appId as invalid
    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection,
        _T("spawnApp"),
        _T("{\"appId\":\"\",\"appPath\":\"test.app.path\",\"appConfig\":\"test.app.config\",\"runtimeAppId\":\"test.runtime.app\",\"runtimePath\":\"test.runtime.path\",\"runtimeConfig\":\"test.runtime.config\",\"launchIntent\":\"test.launch.intent\",\"environmentVars\":\"test.env.vars\",\"enableDebugger\":true,\"targetLifecycleState\":Exchange::ILifecycleManager::LifecycleState::ACTIVE,\"runtimeConfigObject\":null,\"launchArgs\":\"test.arguments\",\"appInstanceId\":\"\",\"errorReason\":\"\",\"success\":true}"),
         mResponse));

    // TC-5: Spawn an app with appPath as invalid
    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection,
        _T("spawnApp"),
        _T("{\"appId\":\"com.test.app\",\"appPath\":\"\",\"appConfig\":\"test.app.config\",\"runtimeAppId\":\"test.runtime.app\",\"runtimePath\":\"test.runtime.path\",\"runtimeConfig\":\"test.runtime.config\",\"launchIntent\":\"test.launch.intent\",\"environmentVars\":\"test.env.vars\",\"enableDebugger\":true,\"targetLifecycleState\":Exchange::ILifecycleManager::LifecycleState::ACTIVE,\"runtimeConfigObject\":null,\"launchArgs\":\"test.arguments\",\"appInstanceId\":\"\",\"errorReason\":\"\",\"success\":true}"),
         mResponse));
    
    // TC-6: Spawn an app with appConfig as invalid
    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection,
        _T("spawnApp"),
        _T("{\"appId\":\"com.test.app\",\"appPath\":\"test.app.path\",\"appConfig\":\"\",\"runtimeAppId\":\"test.runtime.app\",\"runtimePath\":\"test.runtime.path\",\"runtimeConfig\":\"test.runtime.config\",\"launchIntent\":\"test.launch.intent\",\"environmentVars\":\"test.env.vars\",\"enableDebugger\":true,\"targetLifecycleState\":Exchange::ILifecycleManager::LifecycleState::ACTIVE,\"runtimeConfigObject\":null,\"launchArgs\":\"test.arguments\",\"appInstanceId\":\"\",\"errorReason\":\"\",\"success\":true}"),
         mResponse));

    // TC-7: Spawn an app with targetLifecycleState as invalid
    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection,
        _T("spawnApp"),
        _T("{\"appId\":\"com.test.app\",\"appPath\":\"test.app.path\",\"appConfig\":\"test.app.config\",\"runtimeAppId\":\"test.runtime.app\",\"runtimePath\":\"test.runtime.path\",\"runtimeConfig\":\"test.runtime.config\",\"launchIntent\":\"test.launch.intent\",\"environmentVars\":\"test.env.vars\",\"enableDebugger\":true,\"targetLifecycleState\":Exchange::ILifecycleManager::LifecycleState::UNLOADED,\"runtimeConfigObject\":null,\"launchArgs\":\"test.arguments\",\"appInstanceId\":\"\",\"errorReason\":\"\",\"success\":true}"),
         mResponse));
    
    // TC-8: Spawn an app with launchConfig as invalid
    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection,
        _T("spawnApp"),
        _T("{\"appId\":\"com.test.app\",\"appPath\":\"test.app.path\",\"appConfig\":\"test.app.config\",\"runtimeAppId\":\"test.runtime.app\",\"runtimePath\":\"test.runtime.path\",\"runtimeConfig\":\"test.runtime.config\",\"launchIntent\":\"test.launch.intent\",\"environmentVars\":\"test.env.vars\",\"enableDebugger\":true,\"targetLifecycleState\":Exchange::ILifecycleManager::LifecycleState::ACTIVE,\"runtimeConfigObject\":null,\"launchArgs\":\"test.arguments\",\"appInstanceId\":\"\",\"errorReason\":\"\",\"success\":true}"),
         mResponse));

    // TC-9: Spawn an app with launchArgs as invalid
    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection,
        _T("spawnApp"),
        _T("{\"appId\":\"com.test.app\",\"appPath\":\"test.app.path\",\"appConfig\":\"test.app.config\",\"runtimeAppId\":\"test.runtime.app\",\"runtimePath\":\"test.runtime.path\",\"runtimeConfig\":\"test.runtime.config\",\"launchIntent\":\"test.launch.intent\",\"environmentVars\":\"test.env.vars\",\"enableDebugger\":true,\"targetLifecycleState\":Exchange::ILifecycleManager::LifecycleState::ACTIVE,\"runtimeConfigObject\":null,\"launchArgs\":\"\",\"appInstanceId\":\"\",\"errorReason\":\"\",\"success\":true}"),
         mResponse));

    releaseResources();
}

TEST_F(LifecycleManagerTest, isAppLoaded_onSpawnAppSuccess) 
{
    createResources();

    // TC-10: Check if app is loaded after spawning
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("spawnApp"),
        _T("{\"appId\":\"com.test.app\",\"appPath\":\"test.app.path\",\"appConfig\":\"test.app.config\",\"runtimeAppId\":\"test.runtime.app\",\"runtimePath\":\"test.runtime.path\",\"runtimeConfig\":\"test.runtime.config\",\"launchIntent\":\"test.launch.intent\",\"environmentVars\":\"test.env.vars\",\"enableDebugger\":true,\"targetLifecycleState\":Exchange::ILifecycleManager::LifecycleState::ACTIVE,\"runtimeConfigObject\":" + runtimeConfigtoJson(runtimeConfigObject) + ",\"launchArgs\":\"test.arguments\",\"appInstanceId\":\"\",\"errorReason\":\"\",\"success\":true}"),
         mResponse));

    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("isAppLoaded"),
        _T("{\"appId\":\"com.test.app\"}"),
         mResponse));

    EXPECT_EQ(mResponse, "\"[{\\\"loaded\\\":true}]\"");

    releaseResources();
}

TEST_F(LifecycleManagerTest, isAppLoaded_onSpawnAppFailure)
{
    createResources();

    // TC-11: Check that app is not loaded after spawnApp fails
    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection,
        _T("spawnApp"),
        _T("{\"appId\":\"\",\"appPath\":\"\",\"appConfig\":\"\",\"runtimeAppId\":\"\",\"runtimePath\":\"\",\"runtimeConfig\":\"\",\"launchIntent\":\"\",\"environmentVars\":\"\",\"enableDebugger\":false,\"targetLifecycleState\":Exchange::ILifecycleManager::LifecycleState::UNLOADED,\"runtimeConfigObject\":null,\"launchArgs\":\"\",\"appInstanceId\":\"\",\"errorReason\":\"\",\"success\":false}"),
         mResponse));

    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("isAppLoaded"),
        _T("{\"appId\":\"com.test.app\"}"),
         mResponse));

    EXPECT_EQ(mResponse, "\"[{\\\"loaded\\\":false}]\"");

    releaseResources();
}

TEST_F(LifecycleManagerTest, isAppLoaded_oninvalidAppId)
{
    createResources();

    // TC-12: Verify error on passing an invalid appId
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("spawnApp"),
        _T("{\"appId\":\"com.test.app\",\"appPath\":\"test.app.path\",\"appConfig\":\"test.app.config\",\"runtimeAppId\":\"test.runtime.app\",\"runtimePath\":\"test.runtime.path\",\"runtimeConfig\":\"test.runtime.config\",\"launchIntent\":\"test.launch.intent\",\"environmentVars\":\"test.env.vars\",\"enableDebugger\":true,\"targetLifecycleState\":Exchange::ILifecycleManager::LifecycleState::ACTIVE,\"runtimeConfigObject\":" + runtimeConfigtoJson(runtimeConfigObject) + ",\"launchArgs\":\"test.arguments\",\"appInstanceId\":\"\",\"errorReason\":\"\",\"success\":true}"),
         mResponse));

    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("isAppLoaded"),
        _T("{\"appId\":\"\"}"),
         mResponse));

    EXPECT_EQ(mResponse, "\"[{\\\"loaded\\\":false}]\"");

    releaseResources();
}

TEST_F(LifecycleManagerTest, getLoadedApps_verboseEnabled)
{
    createResources();

    // TC-13: Get loaded apps with verbose enabled
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("spawnApp"),
        _T("{\"appId\":\"com.test.app\",\"appPath\":\"test.app.path\",\"appConfig\":\"test.app.config\",\"runtimeAppId\":\"test.runtime.app\",\"runtimePath\":\"test.runtime.path\",\"runtimeConfig\":\"test.runtime.config\",\"launchIntent\":\"test.launch.intent\",\"environmentVars\":\"test.env.vars\",\"enableDebugger\":true,\"targetLifecycleState\":Exchange::ILifecycleManager::LifecycleState::ACTIVE,\"runtimeConfigObject\":" + runtimeConfigtoJson(runtimeConfigObject) + ",\"launchArgs\":\"test.arguments\",\"appInstanceId\":\"\",\"errorReason\":\"\",\"success\":true}"),
         mResponse));

    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("getLoadedApps"),
        _T("{\"verbose\":true}"),
         mResponse));

    EXPECT_EQ(mResponse, "\"[{\\\"appId\\\":\\\"com.test.app\\\",\\\"type\\\":1,\\\"lifecycleState\\\":0,\\\"targetLifecycleState\\\":2,\\\"activeSessionId\\\":\\\"\\\",\\\"appInstanceId\\\":\\\"\\\"}]\"");

    releaseResources();
}

TEST_F(LifecycleManagerTest, getLoadedApps_verboseDisabled)
{
    createResources();

    // TC-14: Get loaded apps with verbose enabled
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("spawnApp"),
        _T("{\"appId\":\"com.test.app\",\"appPath\":\"test.app.path\",\"appConfig\":\"test.app.config\",\"runtimeAppId\":\"test.runtime.app\",\"runtimePath\":\"test.runtime.path\",\"runtimeConfig\":\"test.runtime.config\",\"launchIntent\":\"test.launch.intent\",\"environmentVars\":\"test.env.vars\",\"enableDebugger\":true,\"targetLifecycleState\":Exchange::ILifecycleManager::LifecycleState::ACTIVE,\"runtimeConfigObject\":" + runtimeConfigtoJson(runtimeConfigObject) + ",\"launchArgs\":\"test.arguments\",\"appInstanceId\":\"\",\"errorReason\":\"\",\"success\":true}"),
         mResponse));

    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("getLoadedApps"),
        _T("{\"verbose\":false}"),
         mResponse));

    EXPECT_EQ(mResponse, "\"[{\\\"appId\\\":\\\"com.test.app\\\",\\\"type\\\":1,\\\"lifecycleState\\\":0,\\\"targetLifecycleState\\\":2,\\\"activeSessionId\\\":\\\"\\\",\\\"appInstanceId\\\":\\\"\\\"}]\"");

    releaseResources();
}

TEST_F(LifecycleManagerTest, getLoadedApps_noAppsLoaded)
{
    createResources();

    // TC-15: Check that no apps are loaded
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("getLoadedApps"),
        _T("{\"verbose\":true}"),
         mResponse));

    EXPECT_EQ(mResponse, "\"[]\"");

    releaseResources();
}

TEST_F(LifecycleManagerTest, setTargetAppState_withValidParams)
{
    createResources();

    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("spawnApp"),
        _T("{\"appId\":\"com.test.app\",\"appPath\":\"test.app.path\",\"appConfig\":\"test.app.config\",\"runtimeAppId\":\"test.runtime.app\",\"runtimePath\":\"test.runtime.path\",\"runtimeConfig\":\"test.runtime.config\",\"launchIntent\":\"test.launch.intent\",\"environmentVars\":\"test.env.vars\",\"enableDebugger\":true,\"targetLifecycleState\":Exchange::ILifecycleManager::LifecycleState::ACTIVE,\"runtimeConfigObject\":" + runtimeConfigtoJson(runtimeConfigObject) + ",\"launchArgs\":\"test.arguments\",\"appInstanceId\":\"\",\"errorReason\":\"\",\"success\":true}"),
         mResponse));

    // TC-16: Set the target state of a loaded app with all parameters valid
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("setTargetAppState"),
        _T("{\"appInstanceId\":\"test.app.instance\",\"targetLifecycleState\":Exchange::ILifecycleManager::LifecycleState::SUSPENDED,\"launchIntent\":\"test.launch.intent\"}"),
         mResponse));

    // TC-17: Set the target state of a loaded app with only required parameters valid
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("setTargetAppState"),
        _T("{\"appInstanceId\":\"test.app.instance\",\"targetLifecycleState\":Exchange::ILifecycleManager::LifecycleState::HIBERNATED,\"launchIntent\":\"\"}"),
         mResponse));

    releaseResources();
}

TEST_F(LifecycleManagerTest, setTargetAppState_withinvalidParams)
{
    createResources();

    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("spawnApp"),
        _T("{\"appId\":\"com.test.app\",\"appPath\":\"test.app.path\",\"appConfig\":\"test.app.config\",\"runtimeAppId\":\"test.runtime.app\",\"runtimePath\":\"test.runtime.path\",\"runtimeConfig\":\"test.runtime.config\",\"launchIntent\":\"test.launch.intent\",\"environmentVars\":\"test.env.vars\",\"enableDebugger\":true,\"targetLifecycleState\":Exchange::ILifecycleManager::LifecycleState::ACTIVE,\"runtimeConfigObject\":" + runtimeConfigtoJson(runtimeConfigObject) + ",\"launchArgs\":\"test.arguments\",\"appInstanceId\":\"\",\"errorReason\":\"\",\"success\":true}"),
         mResponse));

    // TC-18: Set the target state of a loaded app with invalid appInstanceId
    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection,
        _T("setTargetAppState"),
        _T("{\"appInstanceId\":\"\",\"targetLifecycleState\":Exchange::ILifecycleManager::LifecycleState::SUSPENDED,\"launchIntent\":\"test.launch.intent\"}"),
         mResponse));

    // TC-19: Set the target state of a loaded app with invalid targetLifecycleState
    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection,
        _T("setTargetAppState"),
        _T("{\"appInstanceId\":\"test.app.instance\",\"targetLifecycleState\":Exchange::ILifecycleManager::LifecycleState::UNLOADED,\"launchIntent\":\"\"}"),
         mResponse));

    // TC-20: Set the target state of a loaded app with all parameters invalid
    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection,
        _T("setTargetAppState"),
        _T("{\"appInstanceId\":\"\",\"targetLifecycleState\":Exchange::ILifecycleManager::LifecycleState::UNLOADED,\"launchIntent\":\"\"}"),
         mResponse));

    releaseResources();
}

TEST_F(LifecycleManagerTest, unloadApp_afterSpawnApp)
{
    createResources();

    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("spawnApp"),
        _T("{\"appId\":\"com.test.app\",\"appPath\":\"test.app.path\",\"appConfig\":\"test.app.config\",\"runtimeAppId\":\"test.runtime.app\",\"runtimePath\":\"test.runtime.path\",\"runtimeConfig\":\"test.runtime.config\",\"launchIntent\":\"test.launch.intent\",\"environmentVars\":\"test.env.vars\",\"enableDebugger\":true,\"targetLifecycleState\":Exchange::ILifecycleManager::LifecycleState::ACTIVE,\"runtimeConfigObject\":" + runtimeConfigtoJson(runtimeConfigObject) + ",\"launchArgs\":\"test.arguments\",\"appInstanceId\":\"\",\"errorReason\":\"\",\"success\":true}"),
         mResponse));

    // TC-21: Unload the app after spawning
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("unloadApp"),
        _T("{\"appInstanceId\":\"test.app.instance\"}"),
         mResponse));

    //EXPECT_EQ(mResponse, "\"[{\\\"loaded\\\":false}]\"");

    releaseResources();
}

TEST_F(LifecycleManagerTest, unloadApp_onSpawnAppFailure)
{
    createResources();

    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection,
        _T("spawnApp"),
        _T("{\"appId\":\"\",\"appPath\":\"\",\"appConfig\":\"\",\"runtimeAppId\":\"\",\"runtimePath\":\"\",\"runtimeConfig\":\"\",\"launchIntent\":\"\",\"environmentVars\":\"\",\"enableDebugger\":false,\"targetLifecycleState\":Exchange::ILifecycleManager::LifecycleState::UNLOADED,\"runtimeConfigObject\":null,\"launchArgs\":\"\",\"appInstanceId\":\"\",\"errorReason\":\"\",\"success\":false}"),
         mResponse));

    // TC-22: Unload the app after spawn fails
    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection,
        _T("unloadApp"),
        _T("{\"appInstanceId\":\"test.app.instance\"}"),
         mResponse));

    releaseResources();
}

TEST_F(LifecycleManagerTest, killApp_afterSpawnApp)
{
    createResources();

    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("spawnApp"),
        _T("{\"appId\":\"com.test.app\",\"appPath\":\"test.app.path\",\"appConfig\":\"test.app.config\",\"runtimeAppId\":\"test.runtime.app\",\"runtimePath\":\"test.runtime.path\",\"runtimeConfig\":\"test.runtime.config\",\"launchIntent\":\"test.launch.intent\",\"environmentVars\":\"test.env.vars\",\"enableDebugger\":true,\"targetLifecycleState\":Exchange::ILifecycleManager::LifecycleState::ACTIVE,\"runtimeConfigObject\":" + runtimeConfigtoJson(runtimeConfigObject) + ",\"launchArgs\":\"test.arguments\",\"appInstanceId\":\"\",\"errorReason\":\"\",\"success\":true}"),
         mResponse));

    // TC-23: Kill the app after spawning
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("killApp"),
        _T("{\"appInstanceId\":\"test.app.instance\"}"),
         mResponse));

    //EXPECT_EQ(mResponse, "\"[{\\\"loaded\\\":false}]\"");

    releaseResources();
}

TEST_F(LifecycleManagerTest, killApp_onSpawnAppFailure)
{
    createResources();

    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection,
        _T("spawnApp"),
        _T("{\"appId\":\"\",\"appPath\":\"\",\"appConfig\":\"\",\"runtimeAppId\":\"\",\"runtimePath\":\"\",\"runtimeConfig\":\"\",\"launchIntent\":\"\",\"environmentVars\":\"\",\"enableDebugger\":false,\"targetLifecycleState\":Exchange::ILifecycleManager::LifecycleState::UNLOADED,\"runtimeConfigObject\":null,\"launchArgs\":\"\",\"appInstanceId\":\"\",\"errorReason\":\"\",\"success\":false}"),
         mResponse));

    // TC-24: Kill the app after spawn fails
    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection,
        _T("killApp"),
        _T("{\"appInstanceId\":\"test.app.instance\"}"),
         mResponse));

    releaseResources();
}

TEST_F(LifecycleManagerTest, sendIntenttoActiveApp_afterSpawnApp)
{
    createResources();

    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("spawnApp"),
        _T("{\"appId\":\"com.test.app\",\"appPath\":\"test.app.path\",\"appConfig\":\"test.app.config\",\"runtimeAppId\":\"test.runtime.app\",\"runtimePath\":\"test.runtime.path\",\"runtimeConfig\":\"test.runtime.config\",\"launchIntent\":\"test.launch.intent\",\"environmentVars\":\"test.env.vars\",\"enableDebugger\":true,\"targetLifecycleState\":Exchange::ILifecycleManager::LifecycleState::ACTIVE,\"runtimeConfigObject\":" + runtimeConfigtoJson(runtimeConfigObject) + ",\"launchArgs\":\"test.arguments\",\"appInstanceId\":\"\",\"errorReason\":\"\",\"success\":true}"),
         mResponse));

    // TC-25: Send intent to the app after spawning
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("sendIntenttoActiveApp"),
        _T("{\"appInstanceId\":\"test.app.instance\",\"intent\":\"test.intent\"}"),
         mResponse));

    //EXPECT_EQ(mResponse, "\"[{\\\"loaded\\\":false}]\"");

    releaseResources();
}

TEST_F(LifecycleManagerTest, sendIntenttoActiveApp_onSpawnAppFailure)
{
    createResources();

    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection,
        _T("spawnApp"),
        _T("{\"appId\":\"\",\"appPath\":\"\",\"appConfig\":\"\",\"runtimeAppId\":\"\",\"runtimePath\":\"\",\"runtimeConfig\":\"\",\"launchIntent\":\"\",\"environmentVars\":\"\",\"enableDebugger\":false,\"targetLifecycleState\":Exchange::ILifecycleManager::LifecycleState::UNLOADED,\"runtimeConfigObject\":null,\"launchArgs\":\"\",\"appInstanceId\":\"\",\"errorReason\":\"\",\"success\":false}"),
         mResponse));

    // TC-26: Send intent to the app after spawn fails
    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection,
        _T("sendIntenttoActiveApp"),
        _T("{\"appInstanceId\":\"test.app.instance\",\"intent\":\"test.intent\"}"),
         mResponse));

    releaseResources();
}

TEST_F(LifecycleManagerTest, sendIntenttoActiveApp_withinvalidParams)
{
    createResources();

    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("spawnApp"),
        _T("{\"appId\":\"com.test.app\",\"appPath\":\"test.app.path\",\"appConfig\":\"test.app.config\",\"runtimeAppId\":\"test.runtime.app\",\"runtimePath\":\"test.runtime.path\",\"runtimeConfig\":\"test.runtime.config\",\"launchIntent\":\"test.launch.intent\",\"environmentVars\":\"test.env.vars\",\"enableDebugger\":true,\"targetLifecycleState\":Exchange::ILifecycleManager::LifecycleState::ACTIVE,\"runtimeConfigObject\":" + runtimeConfigtoJson(runtimeConfigObject) + ",\"launchArgs\":\"test.arguments\",\"appInstanceId\":\"\",\"errorReason\":\"\",\"success\":true}"),
         mResponse));

    // TC-27: Send intent to the app after spawn fails with both parameters invalid
    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection,
        _T("sendIntenttoActiveApp"),
        _T("{\"appInstanceId\":\"\",\"intent\":\"\"}"),
         mResponse));

    // TC-28: Send intent to the app after spawn fails with appInstanceId invalid
    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection,
        _T("sendIntenttoActiveApp"),
        _T("{\"appInstanceId\":\"\",\"intent\":\"test.intent\"}"),
         mResponse));

    // TC-29: Send intent to the app after spawn fails with intent invalid
    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection,
        _T("sendIntenttoActiveApp"),
        _T("{\"appInstanceId\":\"test.app.instance\",\"intent\":\"\"}"),
         mResponse));

    releaseResources();
}















