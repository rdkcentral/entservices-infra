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

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);

using ::testing::NiceMock;
using namespace WPEFramework;

namespace {
const string callSign = _T("AppManager");
}

class AppManagerTest : public ::testing::Test {
protected:
    Core::ProxyType<Plugin::AppManager> mPlugin;
    Core::JSONRPC::Handler& mHandler;
    DECL_CORE_JSONRPC_CONX connection;
    string mResponse;
    Core::ProxyType<Plugin::AppManagerImplementation> AppManagerImpl;
    NiceMock<COMLinkMock> mComLinkMock;
    NiceMock<ServiceMock> mService;
    LifecycleManagerMock* mLifecycleManagerMock = nullptr;
    PackageManagerMock* mPackageManagerMock = nullptr;
    Store2Mock* mStore2Mock = nullptr;
    ServiceMock* mServiceMock = nullptr;
    // Currently, this is used for TerminateApp test cases, as it depends on the LifecycleManager state change.
    // This should be removed once the LifecycleManager notification is handled here.
    Plugin::AppManagerImplementation *mAppManagerImpl;

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
        mLifecycleManagerMock = new NiceMock<LifecycleManagerMock>;
        mPackageManagerMock = new NiceMock<PackageManagerMock>;
        mStore2Mock = new NiceMock<Store2Mock>;

        EXPECT_CALL(mService, QueryInterfaceByCallsign(::testing::_, ::testing::_))
          .Times(::testing::AnyNumber())
          .WillRepeatedly(::testing::Invoke(
              [&](const uint32_t, const std::string& name) -> void* {
                if (name == "org.rdk.LifecycleManager") {
                    return reinterpret_cast<void*>(mLifecycleManagerMock);
                } else if (name == "org.rdk.PersistentStore") {
                   return reinterpret_cast<void*>(mStore2Mock);
                } else if (name == "org.rdk.PackageManagerRDKEMS") {
                   return reinterpret_cast<void*>(mPackageManagerMock);
                }
            return nullptr;
        }));

        EXPECT_CALL(*mLifecycleManagerMock, AddRef()).WillOnce(::testing::Return());

        // Initialize the plugin
        EXPECT_EQ(string(""), mPlugin->Initialize(&mService));

        // Get the AppManagerImplementation instance
        // This should be removed once the LifecycleManager notification is handled here.
        mAppManagerImpl = Plugin::AppManagerImplementation::getInstance();
    }

    void releaseResources()
    {
        // Deinitialize the plugin
        mPlugin->Deinitialize(&mService);
        // This should be removed once the LifecycleManager notification is handled here.
        mAppManagerImpl = nullptr;

        // Clean up mocks
        if (mServiceMock != nullptr)
        {
            delete mServiceMock;
            mServiceMock = nullptr;
        }
        if (mLifecycleManagerMock != nullptr)
        {
            delete mLifecycleManagerMock;
            mLifecycleManagerMock = nullptr;
        }
        if (mPackageManagerMock != nullptr)
        {
            delete mPackageManagerMock;
            mPackageManagerMock = nullptr;
        }
        if (mStore2Mock != nullptr)
        {
            delete mStore2Mock;
            mStore2Mock = nullptr;
        }
    }


    AppManagerTest()
        : mPlugin(Core::ProxyType<Plugin::AppManager>::Create())
        , mHandler(*(mPlugin))
        , INIT_CONX(1, 0)
    {

    }

    virtual ~AppManagerTest() override
    {

    }
};



TEST_F(AppManagerTest, RegisteredMethods)
{
    createResources();
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Exists(_T("launchApp")));
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Exists(_T("getLoadedApps")));
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Exists(_T("preloadApp")));
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Exists(_T("closeApp")));
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Exists(_T("terminateApp")));
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Exists(_T("setAppProperty")));
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Exists(_T("getAppProperty")));
    releaseResources();
}



/*******************************************************************************************************************
 * Test function for :LaunchApp
 * LaunchApp :
 *                The API which launches an app with the provided appId, intent, and launch arguments.
 *
 *                @return Returns Core::ERROR_NONE if the app is successfully launched, Core::ERROR_GENERAL on failure.
 * Use case coverage:
 *                @Success : 2
 *                @Failure : 3
 ********************************************************************************************************************/

/*
 * @brief    : Validates AppManagerImplementation::LaunchApp method with required parameters.
 *             - All params are valid
 *             - All optional params are empty strings
 *             - One optional params is empty string
 *
 * @param[in]   : None.
 * @return      : Core::ERROR_NONE for all test cases
*/
TEST_F(AppManagerTest, launchAppSuccess_withValidParams)
{
    createResources();
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("launchApp"),
        _T("{\"appId\":\"com.test.app\",\"intent\":\"test.intent\",\"launchArgs\":\"test.arguments\"}"),
         mResponse));

    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("launchApp"),
        _T("{\"appId\":\"com.test.app\",\"intent\":\"\",\"launchArgs\":\"\"}"),
         mResponse));

    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("launchApp"),
        _T("{\"appId\":\"com.test.app\",\"intent\":\"test.intent\",\"launchArgs\":\"\"}"),
         mResponse));

    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("launchApp"),
        _T("{\"appId\":\"com.test.app\",\"intent\":\"\",\"launchArgs\":\"test.arguments\"}"),
         mResponse));
    releaseResources();
}

/**
 * @brief       : Validates the successful launch of an app and verifies that its information
 *                is correctly loaded and updated in the AppManagerImplementation instance.
 *                Verifies:
 *                - App is successfully launched with valid parameters.
 *                - LoadedAppInfo is populated with the correct appInstanceId and targetAppState.
 *
 * @param[in]   : None.
 * @return      : Core::ERROR_NONE
 */
TEST_F(AppManagerTest, launchAppSuccess_VerifyLoadedAppInfo)
{
    const std::string appId = "com.test.app";
    const std::string intent = "test.intent";
    const std::string launchArgs = "test.arguments";
    const std::string appInstanceId = "testAppInstance";
    const std::string errorReason = "";
    bool success = true;


    createResources();
    EXPECT_CALL(*mLifecycleManagerMock, SpawnApp(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_,
            ::testing::_, intent, ::testing::_, ::testing::_, Exchange::ILifecycleManager::LifecycleState::ACTIVE,
            launchArgs, ::testing::_, ::testing::_, ::testing::_))
    .WillOnce(::testing::DoAll(
        ::testing::SetArgReferee<11>(appInstanceId),
        ::testing::SetArgReferee<12>(errorReason),
        ::testing::SetArgReferee<13>(success),
        ::testing::Return(Core::ERROR_NONE)));

    const std::string launchRequestPayload =
        "{\"appId\":\"" + appId + "\",\"intent\":\"" + intent + "\",\"launchArgs\":\"" + launchArgs + "\"}";
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection, _T("launchApp"), launchRequestPayload, mResponse));
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("getLoadedApps"),
        _T("{}"),
         mResponse));

    EXPECT_EQ(mResponse, "\"[{\\\"appId\\\":\\\"com.test.app\\\",\\\"type\\\":1,\\\"lifecycleState\\\":0,\\\"targetLifecycleState\\\":2,\\\"activeSessionId\\\":\\\"\\\",\\\"appInstanceId\\\":\\\"testAppInstance\\\"}]\"");

    releaseResources();
}

/**
 * @brief       : Validates the failure scenarios of the launchApp API with invalid parameters.
 *                Verifies:
 *                - All parameters (appId, intent, launchArgs) are empty strings.
 *                - The appId is an empty string while other parameters are valid.
 *
 * @param[in]   : None.
 * @return      : Core::ERROR_GENERAL for all test cases.
 */
TEST_F(AppManagerTest, launchAppFailed_withInvalidParams)
{
    createResources();
    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection,
        _T("launchApp"),
        _T("{\"appId\":\"\", \"intent\":\"\", \"launchArgs\":\"\"}"),
         mResponse));

    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection,
        _T("launchApp"),
        _T("{\"appId\":\"\", \"intent\":\"test.intent\", \"launchArgs\":\"test.arguments\"}"),
         mResponse));
    releaseResources();
}

/*
 * @brief       : Validates the failure of the launchApp when LifecycleManager Remote Object failed to create
 *
 * @param[in]   : None.
 * @return      : Core::ERROR_GENERAL
 */
TEST_F(AppManagerTest, launchAppFailed_whenLifecycleMngrObjectNull)
{
    const std::string appId = "com.test.app";
    const std::string intent = "test.intent";
    const std::string launchArgs = "test.arguments";

    ON_CALL(mService, COMLink())
        .WillByDefault(::testing::Invoke(
        [this]() {
             TEST_LOG("Pass created comLinkMock: %p ", &mComLinkMock);
             return &mComLinkMock;
            }));

    mServiceMock = new NiceMock<ServiceMock>;

    EXPECT_EQ(string(""), mPlugin->Initialize(&mService));
    EXPECT_CALL(mService, QueryInterfaceByCallsign(::testing::_, ::testing::_))
          .Times(::testing::AnyNumber())
          .WillRepeatedly(::testing::Invoke(
              [&](const uint32_t, const std::string& name) -> void* {
            return nullptr;
        }));

    const std::string launchRequestPayload =
        "{\"appId\":\"" + appId + "\",\"intent\":\"" + intent + "\",\"launchArgs\":\"" + launchArgs + "\"}";

    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection, _T("launchApp"), launchRequestPayload, mResponse));


    mPlugin->Deinitialize(&mService);

    if (mServiceMock != nullptr)
    {
        delete mServiceMock;
        mServiceMock = nullptr;
    }
}

/**
 * @brief       : Validates the failure of the launchApp API when SpawnApp returns an error.
 *              : Ensures LoadedAppInfo is not populated when SpawnApp fails.
 *
 * @param[in]   : None.
 * @return      : Core::ERROR_GENERAL
 */
TEST_F(AppManagerTest, launchAppFailed_onSpawnAppError)
{
    const std::string appId = "com.test.app";
    const std::string intent = "test.intent";
    const std::string launchArgs = "test.arguments";
    const std::string appInstanceId = "testAppInstance";
    const std::string errorReason = "SpawnApp failed due to system error";
    bool success = false;

    createResources();
    EXPECT_CALL(*mLifecycleManagerMock, SpawnApp(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_,
            ::testing::_, intent, ::testing::_, ::testing::_, Exchange::ILifecycleManager::LifecycleState::ACTIVE,
            launchArgs, ::testing::_, ::testing::_, ::testing::_))
    .WillOnce(::testing::DoAll(
        ::testing::SetArgReferee<11>(appInstanceId),
        ::testing::SetArgReferee<12>(errorReason),
        ::testing::SetArgReferee<13>(success),
        ::testing::Return(Core::ERROR_GENERAL)));

    const std::string launchRequestPayload =
        "{\"appId\":\"" + appId + "\",\"intent\":\"" + intent + "\",\"launchArgs\":\"" + launchArgs + "\"}";

    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection, _T("launchApp"), launchRequestPayload, mResponse));
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("getLoadedApps"),
        _T("{}"),
         mResponse));

    EXPECT_EQ(mResponse, "\"[]\"");

    releaseResources();
}

/*******************************************************************************************************************
 * Test function for :GetLoadedApps
 * GetLoadedApps :
 *                The API which retrieves a list of loaded apps.
 *
 *                @return Returns the list of loaded apps in JSON format, or Core::ERROR_GENERAL on failure.
 * Use case coverage:
 *                @Success : 2
 *                @Failure : 1
 ********************************************************************************************************************/

/**
 * @brief       : Validates the behavior of the getLoadedApps API when no apps have been launched.
 *                Ensures :it responds with an empty list of loaded apps.
 *
 * @param[in]   : None.
 * @return      : Confirms that the mResponse contains an empty array "[]" with success status.
 */
TEST_F(AppManagerTest, getLoadedAppsSuccess_withoutLaunchApp)
{
    createResources();
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("getLoadedApps"),
        _T("{}"),
         mResponse));

    EXPECT_EQ(mResponse, "\"[]\"");
    releaseResources();
}

/**
 * @brief       : Validates the behavior of the getLoadedApps API after an app has been successfully launched.
 *                verifies The getLoadedApps API responds with  the details of the launched app.
 *
 * @param[in]   : None.
 * @return      : Core::ERROR_NONE
 */
TEST_F(AppManagerTest, getLoadedAppsSuccess_withLaunchApp)
{
    createResources();
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("launchApp"),
        _T("{\"appId\":\"com.test.app\",\"intent\":\"test.intent\",\"launchArgs\":\"test.arguments\"}"),
         mResponse));

    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("getLoadedApps"),
        _T("{}"),
         mResponse));

    EXPECT_EQ(mResponse, "\"[{\\\"appId\\\":\\\"com.test.app\\\",\\\"type\\\":1,\\\"lifecycleState\\\":0,\\\"targetLifecycleState\\\":2,\\\"activeSessionId\\\":\\\"\\\",\\\"appInstanceId\\\":\\\"\\\"}]\"");
    releaseResources();
}

/*
 * @brief       : Validates the failure of the geLoadedApp when LifecycleManager Remote Object failed to create
 *
 * @param[in]   : None.
 * @return      : Core::ERROR_GENERAL
 */
TEST_F(AppManagerTest, getLoadedAppsFailed_whenLifecycleMngrObjectNull)
{
    const std::string appId = "com.test.app";
    const std::string intent = "test.intent";
    const std::string launchArgs = "test.arguments";

    ON_CALL(mService, COMLink())
        .WillByDefault(::testing::Invoke(
        [this]() {
             TEST_LOG("Pass created comLinkMock: %p ", &mComLinkMock);
             return &mComLinkMock;
            }));

    mServiceMock = new NiceMock<ServiceMock>;

    EXPECT_EQ(string(""), mPlugin->Initialize(&mService));
    EXPECT_CALL(mService, QueryInterfaceByCallsign(::testing::_, ::testing::_))
          .Times(::testing::AnyNumber())
          .WillRepeatedly(::testing::Invoke(
              [&](const uint32_t, const std::string& name) -> void* {
            return nullptr;
        }));

    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection,
        _T("getLoadedApps"),
        _T("{}"),
         mResponse));


    mPlugin->Deinitialize(&mService);

    // Clean up mocks
    if (mServiceMock != nullptr)
    {
        delete mServiceMock;
        mServiceMock = nullptr;
    }
}

/*******************************************************************************************************************
 * Test function for :CloseApp
 * CloseApp :
 *                The API which closes an app by its appId.
 *
 *                @return Returns Core::ERROR_NONE if the app is successfully closed, Core::ERROR_GENERAL on failure.
 * Use case coverage:
 *                @Success : 1
 *                @Failure : 4
 ********************************************************************************************************************/

/**
 * @brief       : Validates closeApp when SpawnApp succeeds and sets the app state to SUSPENDED.
 *
 * @param[in]   : None.
 * @return      : Core::ERROR_NONE
 */
TEST_F(AppManagerTest, closeAppSuccess_onSpawnAppSuccess)
{
    const std::string appId = "com.test.app";
    const std::string intent = "test.intent";
    const std::string launchArgs = "test.arguments";
    const std::string appInstanceId = "testAppInstance";
    const std::string errorReason = "";
    bool success = true;


    createResources();
    EXPECT_CALL(*mLifecycleManagerMock, SpawnApp(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_,
            ::testing::_, intent, ::testing::_, ::testing::_, Exchange::ILifecycleManager::LifecycleState::ACTIVE,
            launchArgs, ::testing::_, ::testing::_, ::testing::_))
    .WillOnce(::testing::DoAll(
        ::testing::SetArgReferee<11>(appInstanceId),
        ::testing::SetArgReferee<12>(errorReason),
        ::testing::SetArgReferee<13>(success),
        ::testing::Return(Core::ERROR_NONE)));

    const std::string launchRequestPayload =
        "{\"appId\":\"" + appId + "\",\"intent\":\"" + intent + "\",\"launchArgs\":\"" + launchArgs + "\"}";
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection, _T("launchApp"), launchRequestPayload, mResponse));

    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("getLoadedApps"),
        _T("{}"),
         mResponse));

    EXPECT_EQ(mResponse, "\"[{\\\"appId\\\":\\\"com.test.app\\\",\\\"type\\\":1,\\\"lifecycleState\\\":0,\\\"targetLifecycleState\\\":2,\\\"activeSessionId\\\":\\\"\\\",\\\"appInstanceId\\\":\\\"testAppInstance\\\"}]\"");

    EXPECT_CALL(*mLifecycleManagerMock, SetTargetAppState(appInstanceId,
                Exchange::ILifecycleManager::LifecycleState::SUSPENDED, intent))
        .WillOnce(::testing::Return(Core::ERROR_NONE));

    const std::string closeRequestPayload = "{\"appId\":\"" + appId + "\"}";
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection, _T("closeApp"), closeRequestPayload, mResponse));

    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("getLoadedApps"),
        _T("{}"),
         mResponse));

    EXPECT_EQ(mResponse, "\"[{\\\"appId\\\":\\\"com.test.app\\\",\\\"type\\\":1,\\\"lifecycleState\\\":0,\\\"targetLifecycleState\\\":3,\\\"activeSessionId\\\":\\\"\\\",\\\"appInstanceId\\\":\\\"testAppInstance\\\"}]\"");

    releaseResources();
}

/**
 * @brief       : Validates closeApp when the app ID is not found in the loaded app info.
 *
 * @param[in]   : None.
 * @return      : Core::ERROR_GENERAL
 */
TEST_F(AppManagerTest, closeAppFailed_whenIdNotFound)
{
    const std::string invalidAppId = "com.invalid.app";
    const std::string closeRequestPayload = "{\"appId\":\"" + invalidAppId + "\"}";
    createResources();

    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection, _T("closeApp"), closeRequestPayload, mResponse));
    releaseResources();
}


/**
 * @brief       : Validates closeApp when the app is not in the ACTIVE state.
 *                Ensures the function returns Core::ERROR_GENERAL if the app is in a suspended state.
 *
 * @param[in]   : None.
 * @return      : Core::ERROR_GENERAL
 */
TEST_F(AppManagerTest, closeAppFailed_whenNotActiveState)
{
    const std::string appId = "com.test.app";
    const std::string intent = "test.intent";
    const std::string launchArgs = "test.arguments";
    const std::string appInstanceId = "testAppInstance";
    const std::string errorReason = "";
    bool success = true;


    createResources();
    EXPECT_CALL(*mLifecycleManagerMock, SpawnApp(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_,
            ::testing::_, intent, ::testing::_, ::testing::_, Exchange::ILifecycleManager::LifecycleState::ACTIVE,
            launchArgs, ::testing::_, ::testing::_, ::testing::_))
    .WillOnce(::testing::DoAll(
        ::testing::SetArgReferee<11>(appInstanceId),
        ::testing::SetArgReferee<12>(errorReason),
        ::testing::SetArgReferee<13>(success),
        ::testing::Return(Core::ERROR_NONE)));

    const std::string launchRequestPayload =
        "{\"appId\":\"" + appId + "\",\"intent\":\"" + intent + "\",\"launchArgs\":\"" + launchArgs + "\"}";
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection, _T("launchApp"), launchRequestPayload, mResponse));

    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("getLoadedApps"),
        _T("{}"),
         mResponse));

    EXPECT_EQ(mResponse, "\"[{\\\"appId\\\":\\\"com.test.app\\\",\\\"type\\\":1,\\\"lifecycleState\\\":0,\\\"targetLifecycleState\\\":2,\\\"activeSessionId\\\":\\\"\\\",\\\"appInstanceId\\\":\\\"testAppInstance\\\"}]\"");

    EXPECT_CALL(*mLifecycleManagerMock, SetTargetAppState(appInstanceId,
                Exchange::ILifecycleManager::LifecycleState::SUSPENDED, intent))
        .WillOnce(::testing::Return(Core::ERROR_NONE));

    const std::string closeRequestPayload = "{\"appId\":\"" + appId + "\"}";
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection, _T("closeApp"), closeRequestPayload, mResponse));
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("getLoadedApps"),
        _T("{}"),
         mResponse));

    EXPECT_EQ(mResponse, "\"[{\\\"appId\\\":\\\"com.test.app\\\",\\\"type\\\":1,\\\"lifecycleState\\\":0,\\\"targetLifecycleState\\\":3,\\\"activeSessionId\\\":\\\"\\\",\\\"appInstanceId\\\":\\\"testAppInstance\\\"}]\"");
    //closeApp fails when APP is not in the ACTIVE state [currently in APP_STATE_SUSPENDED  state]
    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection, _T("closeApp"), closeRequestPayload, mResponse));
    releaseResources();
}

/*
 * @brief       : Validates the failure of the closeApp when LifecycleManager Remote Object failed to create
 *
 * @param[in]   : None.
 * @return      : Core::ERROR_GENERAL
 */
TEST_F(AppManagerTest, closeAppsFailed_whenLifecycleMngrObjectNull)
{
    const std::string appId = "com.test.app";
    const std::string intent = "test.intent";
    const std::string launchArgs = "test.arguments";
    const std::string appInstanceId = "testAppInstance";

    ON_CALL(mService, COMLink())
        .WillByDefault(::testing::Invoke(
        [this]() {
             TEST_LOG("Pass created comLinkMock: %p ", &mComLinkMock);
             return &mComLinkMock;
            }));

    mServiceMock = new NiceMock<ServiceMock>;

    EXPECT_EQ(string(""), mPlugin->Initialize(&mService));
    EXPECT_CALL(mService, QueryInterfaceByCallsign(::testing::_, ::testing::_))
          .Times(::testing::AnyNumber())
          .WillRepeatedly(::testing::Invoke(
              [&](const uint32_t, const std::string& name) -> void* {
            return nullptr;
        }));

    const std::string closeRequestPayload = "{\"appId\":\"" + appId + "\"}";
    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection, _T("closeApp"), closeRequestPayload, mResponse));

    mPlugin->Deinitialize(&mService);

    // Clean up mocks
    if (mServiceMock != nullptr)
    {
        delete mServiceMock;
        mServiceMock = nullptr;
    }
}

/**
 * @brief       : Validates closeApp when setting the target app state fails.
 *
 * @param[in]   : None.
 * @return      : Core::ERROR_GENERAL
 */
TEST_F(AppManagerTest, closeAppFailed_whenSetTargetAppStateFails)
{
    const std::string appId = "com.test.app";
    const std::string intent = "test.intent";
    const std::string launchArgs = "test.arguments";
    const std::string appInstanceId = "testAppInstance";
    const std::string errorReason = "";
    bool success = true;
    createResources();

    EXPECT_CALL(*mLifecycleManagerMock, SpawnApp(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_,
            ::testing::_, intent, ::testing::_, ::testing::_, Exchange::ILifecycleManager::LifecycleState::ACTIVE,
            launchArgs, ::testing::_, ::testing::_, ::testing::_))
    .WillOnce(::testing::DoAll(
        ::testing::SetArgReferee<11>(appInstanceId),
        ::testing::SetArgReferee<12>(errorReason),
        ::testing::SetArgReferee<13>(success),
        ::testing::Return(Core::ERROR_NONE)));

    const std::string launchRequestPayload =
        "{\"appId\":\"" + appId + "\",\"intent\":\"" + intent + "\",\"launchArgs\":\"" + launchArgs + "\"}";
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection, _T("launchApp"), launchRequestPayload, mResponse));


    EXPECT_CALL(*mLifecycleManagerMock, SetTargetAppState(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));

    const std::string closeRequestPayload = "{\"appId\":\"" + appId + "\"}";
    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection, _T("closeApp"), closeRequestPayload, mResponse));
    releaseResources();
}

/*******************************************************************************************************************
 * Test function for :TerminateApp
 * TerminateApp :
 *                The API which terminates an app by its appId.
 *
 *                @return Returns Core::ERROR_NONE if the app is successfully terminated, Core::ERROR_GENERAL on failure.
 * Use case coverage:
 *                @Success : 1
 *                @Failure : 4
 ********************************************************************************************************************/

/**
 * @brief       : Validates TerminateApp when SpawnApp returns success
 *                Ensures the function correctly terminates the app when it is in a running state
 *                .
 * @param[in]   : None.
 * @return      : Core::ERROR_NONE
 */
TEST_F(AppManagerTest, terminateAppSuccess_onSpawnAppSuccess)
{
    const std::string appId = "com.test.app";
    const std::string launchArgs = "test.launch.args";
    std::string errorReason = "";
    const std::string appInstanceId = "testAppInstance";
    bool success = true;
    createResources();

    // Mocking the SpawnApp call to return success
    EXPECT_CALL(*mLifecycleManagerMock, SpawnApp(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_,
            ::testing::_, ::testing::_, ::testing::_, Exchange::ILifecycleManager::LifecycleState::RUNNING, launchArgs,
            ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<11>(appInstanceId),
            ::testing::SetArgReferee<12>(errorReason),
            ::testing::SetArgReferee<13>(success),
            ::testing::Return(Core::ERROR_NONE)));

    const std::string preLoadRequestPayload =
        "{\"appId\":\"" + appId + "\",\"launchArgs\":\"" + launchArgs + "\"}";

    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection, _T("preloadApp"), preLoadRequestPayload, mResponse));

    // Validate that the app info is populated after launch
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("getLoadedApps"),
        _T("{}"),
         mResponse));

    EXPECT_EQ(mResponse, "\"[{\\\"appId\\\":\\\"com.test.app\\\",\\\"type\\\":1,\\\"lifecycleState\\\":0,\\\"targetLifecycleState\\\":1,\\\"activeSessionId\\\":\\\"\\\",\\\"appInstanceId\\\":\\\"testAppInstance\\\"}]\"");

    // Temporarily set the app state to RUNNING, as termination depends on LifecycleManager's state transitioni EVENT, which is not yet handled.
    // This should be removed once the LifecycleManager notification is handled here.
    ASSERT_NE(mAppManagerImpl, nullptr) << "mAppManagerImplementation instance is null.";
    mAppManagerImpl->mAppInfo[appId].currentAppState = Exchange::IAppManager::AppLifecycleState::APP_STATE_RUNNING;

    EXPECT_CALL(*mLifecycleManagerMock, UnloadApp(appInstanceId, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<1>(errorReason),
            ::testing::SetArgReferee<2>(success),
            ::testing::Return(Core::ERROR_NONE)));
    const std::string terminateRequestPayload = "{\"appId\":\"" + appId + "\"}";
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection, _T("terminateApp"), terminateRequestPayload, mResponse));

    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("getLoadedApps"),
        _T("{}"),
         mResponse));

    // confirm the app is removed from mAppInfo after termination.
    EXPECT_EQ(mResponse, "\"[]\"");
    releaseResources();
}

/**
 * @brief       : Validates TerminateApp when the app ID is not found.
 *
 * @param[in]   : None.
 * @return      : Core::ERROR_GENERAL
 */
TEST_F(AppManagerTest, terminateAppFailed_whenAppIdNotFound)
{
    const std::string appId = "com.invalid.app";
    const std::string appInstanceId = "testAppInstance";
    const std::string terminateRequestPayload = "{\"appId\":\"" + appId + "\"}";
    createResources();
    EXPECT_CALL(*mLifecycleManagerMock, UnloadApp(appInstanceId, ::testing::_, ::testing::_))
        .Times(0);
    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection, _T("terminateApp"), terminateRequestPayload, mResponse));
    releaseResources();
}

/**
 * @brief       : Validates TerminateApp when the app is not in the running state.
 *
 * @param[in]   : None.
 * @return      : Core::ERROR_GENERAL
 */
TEST_F(AppManagerTest, terminateAppFailed_whenNotRunningState)
{
    const std::string appId = "com.test.app";
    const std::string intent = "test.intent";
    const std::string launchArgs = "test.arguments";
    const std::string appInstanceId = "testAppInstance";
    const std::string errorReason = "";
    bool success = true;
    createResources();

    EXPECT_CALL(*mLifecycleManagerMock, SpawnApp(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_,
            ::testing::_, intent, ::testing::_, ::testing::_, Exchange::ILifecycleManager::LifecycleState::ACTIVE,
            launchArgs, ::testing::_, ::testing::_, ::testing::_))
    .WillOnce(::testing::DoAll(
        ::testing::SetArgReferee<11>(appInstanceId),
        ::testing::SetArgReferee<12>(errorReason),
        ::testing::SetArgReferee<13>(success),
        ::testing::Return(Core::ERROR_NONE)));

    const std::string launchRequestPayload =
        "{\"appId\":\"" + appId + "\",\"intent\":\"" + intent + "\",\"launchArgs\":\"" + launchArgs + "\"}";

    // Call LaunchApp to set the state to ACTIVE, simulating that the app is not in a running state.
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection, _T("launchApp"), launchRequestPayload, mResponse));

    EXPECT_CALL(*mLifecycleManagerMock, UnloadApp(appInstanceId, ::testing::_, ::testing::_))
        .Times(0);

    const std::string terminateRequestPayload = "{\"appId\":\"" + appId + "\"}";
    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection, _T("terminateApp"), terminateRequestPayload, mResponse));
    releaseResources();
}

/*
 * @brief       : Validates the failure of the terminateApp when LifecycleManager Remote Object failed to create
 *
 * @param[in]   : None.
 * @return      : Core::ERROR_GENERAL
 */
TEST_F(AppManagerTest, terminateAppFailed_whenLifecycleMngrObjectNull)
{
    const std::string appId = "com.test.app";
    const std::string intent = "test.intent";
    const std::string launchArgs = "test.arguments";
    const std::string appInstanceId = "testAppInstance";

    ON_CALL(mService, COMLink())
        .WillByDefault(::testing::Invoke(
        [this]() {
             TEST_LOG("Pass created comLinkMock: %p ", &mComLinkMock);
             return &mComLinkMock;
            }));

    mServiceMock = new NiceMock<ServiceMock>;

    EXPECT_EQ(string(""), mPlugin->Initialize(&mService));
    EXPECT_CALL(mService, QueryInterfaceByCallsign(::testing::_, ::testing::_))
      .Times(::testing::AnyNumber())
      .WillRepeatedly(::testing::Invoke(
        [&](const uint32_t, const std::string& name) -> void* {
            return nullptr;
        }));

    const std::string terminateRequestPayload = "{\"appId\":\"" + appId + "\"}";
    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection, _T("terminateApp"), terminateRequestPayload, mResponse));

    mPlugin->Deinitialize(&mService);

    // Clean up mocks
    if (mServiceMock != nullptr)
    {
        delete mServiceMock;
        mServiceMock = nullptr;
    }
}

/**
 * @brief       : Validates TerminateApp when unloading the app fails.
 *
 * @param[in]   : None.
 * @return      : Core::ERROR_GENERAL
 */
TEST_F(AppManagerTest, terminateAppFailed_whenUnloadAppFails)
{

    const std::string appId = "com.test.app";
    const std::string launchArgs = "test.launch.args";
    std::string errorReason = "";
    const std::string appInstanceId = "testAppInstance";
    bool success = true;
    createResources();

    // Mocking the SpawnApp call to return success
    EXPECT_CALL(*mLifecycleManagerMock, SpawnApp(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_,
            ::testing::_, ::testing::_, ::testing::_, Exchange::ILifecycleManager::LifecycleState::RUNNING, launchArgs,
            ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<11>(appInstanceId),
            ::testing::SetArgReferee<12>(errorReason),
            ::testing::SetArgReferee<13>(success),
            ::testing::Return(Core::ERROR_NONE)));

    const std::string preLoadRequestPayload =
        "{\"appId\":\"" + appId + "\",\"launchArgs\":\"" + launchArgs + "\"}";

    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection, _T("preloadApp"), preLoadRequestPayload, mResponse));

    // Temporarily set the app state to RUNNING, as termination depends on LifecycleManager's state transitioni EVENT, which is not yet handled.
    // This should be removed once the LifecycleManager notification is handled here.
    ASSERT_NE(mAppManagerImpl, nullptr) << "mAppManagerImplementation instance is null.";
    mAppManagerImpl->mAppInfo[appId].currentAppState = Exchange::IAppManager::AppLifecycleState::APP_STATE_RUNNING;

    EXPECT_CALL(*mLifecycleManagerMock, UnloadApp(appInstanceId, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<1>(errorReason),
            ::testing::SetArgReferee<2>(success),
            ::testing::Return(Core::ERROR_GENERAL)));
    const std::string terminateRequestPayload = "{\"appId\":\"" + appId + "\"}";
    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection, _T("terminateApp"), terminateRequestPayload, mResponse));
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("getLoadedApps"),
        _T("{}"),
         mResponse));

    // Verify that the app information for the given appId is still found in the loaded apps list.
    EXPECT_EQ(mResponse, "\"[{\\\"appId\\\":\\\"com.test.app\\\",\\\"type\\\":1,\\\"lifecycleState\\\":1,\\\"targetLifecycleState\\\":1,\\\"activeSessionId\\\":\\\"\\\",\\\"appInstanceId\\\":\\\"testAppInstance\\\"}]\"");
    releaseResources();
}

/*******************************************************************************************************************
 * Test function for :PreloadApp
 * PreloadApp :
 *                The API  preloads an app.
 *
 *                @return Returns Core::ERROR_NONE if the app is successfully preloaded, Core::ERROR_GENERAL on failure.
 * Use case coverage:
 *                @Success : 1
 *                @Failure : 2
 ********************************************************************************************************************/

/**
 * @brief       : Validates PreLoadApp when the spawnApp returns success
 *                Ensures the function correctly preloads the app when the spawn operation is successful.
 *
 * @param[in]   : None.
 * @return      : Core::ERROR_NONE
 */
TEST_F(AppManagerTest, preLoadAppSuccess_onSpawnAppSuccess)
{
    const std::string appId = "com.test.app";
    const std::string launchArgs = "test.launch.args";
    std::string errorReason = "";
    const std::string appInstanceId = "testAppInstance";
    bool success = true;
    createResources();

    // Mocking the SpawnApp call to return success
    EXPECT_CALL(*mLifecycleManagerMock, SpawnApp(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_,
            ::testing::_, ::testing::_, ::testing::_, Exchange::ILifecycleManager::LifecycleState::RUNNING, launchArgs,
            ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<11>(appInstanceId),
            ::testing::SetArgReferee<12>(errorReason),
            ::testing::SetArgReferee<13>(success),
            ::testing::Return(Core::ERROR_NONE)));

    const std::string preLoadRequestPayload =
        "{\"appId\":\"" + appId + "\",\"launchArgs\":\"" + launchArgs + "\"}";

    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection, _T("preloadApp"), preLoadRequestPayload, mResponse));

    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("getLoadedApps"),
        _T("{}"),
         mResponse));

    // Check that the app info is correctly updated
    EXPECT_EQ(mResponse, "\"[{\\\"appId\\\":\\\"com.test.app\\\",\\\"type\\\":1,\\\"lifecycleState\\\":0,\\\"targetLifecycleState\\\":1,\\\"activeSessionId\\\":\\\"\\\",\\\"appInstanceId\\\":\\\"testAppInstance\\\"}]\"");

    releaseResources();
}

/*
 * @brief       : Validates the failure of the preloadApp when LifecycleManager Remote Object failed to create
 *
 * @param[in]   : None.
 * @return      : Core::ERROR_GENERAL
 */
TEST_F(AppManagerTest, preloadAppFailed_whenLifecycleMngrObjectNull)
{
    const std::string appId = "com.test.app";
    const std::string intent = "test.intent";
    const std::string launchArgs = "test.arguments";
    const std::string appInstanceId = "testAppInstance";

    ON_CALL(mService, COMLink())
        .WillByDefault(::testing::Invoke(
        [this]() {
             TEST_LOG("Pass created comLinkMock: %p ", &mComLinkMock);
             return &mComLinkMock;
            }));

    mServiceMock = new NiceMock<ServiceMock>;

    EXPECT_EQ(string(""), mPlugin->Initialize(&mService));
    EXPECT_CALL(mService, QueryInterfaceByCallsign(::testing::_, ::testing::_))
      .Times(::testing::AnyNumber())
      .WillRepeatedly(::testing::Invoke(
        [&](const uint32_t, const std::string& name) -> void* {
            return nullptr;
        }));

    const std::string preLoadRequestPayload =
        "{\"appId\":\"" + appId + "\",\"launchArgs\":\"" + launchArgs + "\"}";

    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection, _T("preloadApp"), preLoadRequestPayload, mResponse));

    mPlugin->Deinitialize(&mService);

    // Clean up mocks
    if (mServiceMock != nullptr)
    {
        delete mServiceMock;
        mServiceMock = nullptr;
    }
}

/**
 * @brief       : Validates PreLoadApp when the spawnApp fails.
 *
 * @param[in]   : None.
 * @return      : Core::ERROR_GENERAL
 */
TEST_F(AppManagerTest, preLoadAppFailed_onSpawnAppFails)
{
    const std::string appId = "com.test.app";
    const std::string launchArgs = "test.launch.args";
    std::string errorReason = "Some error occurred";
    const std::string appInstanceId = "";
    bool success = false;
    createResources();

    // Mocking the SpawnApp call to return failure
    EXPECT_CALL(*mLifecycleManagerMock, SpawnApp(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_,
            ::testing::_, ::testing::_, ::testing::_, Exchange::ILifecycleManager::LifecycleState::RUNNING, launchArgs,
            ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<11>(appInstanceId),
            ::testing::SetArgReferee<12>(errorReason),
            ::testing::SetArgReferee<13>(success),
            ::testing::Return(Core::ERROR_GENERAL)));

    const std::string preLoadRequestPayload =
        "{\"appId\":\"" + appId + "\",\"launchArgs\":\"" + launchArgs + "\"}";
    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection, _T("preloadApp"), preLoadRequestPayload, mResponse));

    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection,
        _T("getLoadedApps"),
        _T("{}"),
         mResponse));

    EXPECT_EQ(mResponse, "\"[]\"");

    releaseResources();
}

/*******************************************************************************************************************
 * Test function for :SetAppProperty
 * SetAppProperty :
 *                The API which sets a property for an app by its appId and key.
 *
 *                @return Returns Core::ERROR_NONE if the property is successfully set, Core::ERROR_GENERAL on failure.
 * Use case coverage:
 *                @Success : 1
 *                @Failure : 4
 ********************************************************************************************************************/

/**
 * @brief       : Validates setAppProperty when setting the value in PersistentStore succeeds.
 *
 * @param[in]   : None.
 * @return      : Core::ERROR_NONE
 */
TEST_F(AppManagerTest, setAppPropertySuccess_onSetValueSuccess)
{
    const std::string appId = "com.test.app";
    const std::string key = "app.property.key";
    const std::string value = "property.value";
    createResources();

    EXPECT_CALL(*mStore2Mock, SetValue(Exchange::IStore2::ScopeType::DEVICE, appId, key, value, 0))
        .WillOnce(::testing::Return(Core::ERROR_NONE));

    const std::string requestPayload =
        "{\"appId\":\"" + appId + "\",\"key\":\"" + key + "\",\"value\":\"" + value + "\"}";
    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection, _T("setAppProperty"), requestPayload, mResponse));

    releaseResources();
}

/**
 * @brief       : Validates setAppProperty when failed to create persistent store object.
 *
 * @param[in]   : None.
 * @return      : Core::ERROR_NONE
 */
TEST_F(AppManagerTest, setAppPropertyFailed_whenPersistentStoreObjectNull)
{
    const std::string appId = "com.test.app";
    const std::string key = "app.property.key";
    const std::string value = "property.value";
    ON_CALL(mService, COMLink())
        .WillByDefault(::testing::Invoke(
        [this]() {
             TEST_LOG("Pass created comLinkMock: %p ", &mComLinkMock);
             return &mComLinkMock;
            }));

    mServiceMock = new NiceMock<ServiceMock>;


    EXPECT_EQ(string(""), mPlugin->Initialize(&mService));
    EXPECT_CALL(mService, QueryInterfaceByCallsign(::testing::_, ::testing::_))
    .Times(::testing::AnyNumber())
    .WillRepeatedly(::testing::Invoke(
        [&](const uint32_t, const std::string& name) -> void* {
            return nullptr;
        }));

    const std::string requestPayload =
          "{\"appId\":\"" + appId + "\",\"key\":\"" + key + "\",\"value\":\"" + value + "\"}";
    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection, _T("setAppProperty"), requestPayload, mResponse));


    mPlugin->Deinitialize(&mService);

    // Clean up mocks
    if (mServiceMock != nullptr)
    {
        delete mServiceMock;
        mServiceMock = nullptr;
    }

}
/**
 * @brief       : Validates setAppProperty when appId is empty.
 *
 * @param[in]   : None.
 * @return      : Core::ERROR_GENERAL
 */
TEST_F(AppManagerTest, setAppPropertyFailed_onEmptyAppId)
{
    const std::string appId = "";  // Empty appId
    const std::string key = "app.property.key";
    const std::string value = "property.value";
    createResources();

    const std::string requestPayload =
        "{\"appId\":\"" + appId + "\",\"key\":\"" + key + "\",\"value\":\"" + value + "\"}";
    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection, _T("setAppProperty"), requestPayload, mResponse));
    releaseResources();
}

/**
 * @brief       : Validates setAppProperty when key is empty.
 *
 * @param[in]   : None.
 * @return      : Core::ERROR_GENERAL
 */
TEST_F(AppManagerTest, setAppPropertyFailed_onEmptyKey)
{
    const std::string appId = "com.test.app";
    const std::string key = "";  // Empty key
    const std::string value = "property.value";
    createResources();

    const std::string requestPayload =
        "{\"appId\":\"" + appId + "\",\"key\":\"" + key + "\",\"value\":\"" + value + "\"}";
    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection, _T("setAppProperty"), requestPayload, mResponse));
    releaseResources();
}

/**
 * @brief       : Validates setAppProperty when setting the value in PersistentStore fails.
 *
 * @param[in]   : None.
 * @return      : Core::ERROR_GENERAL
 */
TEST_F(AppManagerTest, setAppPropertyFailed_onSetValueFails)
{
    const std::string appId = "com.test.app";
    const std::string key = "app.property.key";
    const std::string value = "property.value";
    createResources();

    EXPECT_CALL(*mStore2Mock, SetValue(Exchange::IStore2::ScopeType::DEVICE, appId, key, value, 0))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));

    const std::string requestPayload =
        "{\"appId\":\"" + appId + "\",\"key\":\"" + key + "\",\"value\":\"" + value + "\"}";
    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection, _T("setAppProperty"), requestPayload, mResponse));
    releaseResources();
}


/*******************************************************************************************************************
 * Test function for :GetAppProperty
 * GetAppProperty :
 *                The API which retrieves a property value for an app by its appId and key.
 *
 *                @return Returns the property value if successful, Core::ERROR_GENERAL on failure.
 * Use case coverage:
 *                @Success : 1
 *                @Failure : 4
 ********************************************************************************************************************/

/**
 * @brief       : Validates getAppProperty when getting the value from PersistentStore succeeds.
 *                Ensures the correct value is returned and parsed in the response.
 *
 * @param[in]   : None.
 * @return      : Core::ERROR_NONE
 */
TEST_F(AppManagerTest, getAppPropertySuccess_onGetValueSuccess)
{
    const std::string appId = "com.test.app";
    const std::string key = "app.property.key";
    const std::string expectedValue = "property.value";
    createResources();

    EXPECT_CALL(*mStore2Mock, GetValue(Exchange::IStore2::ScopeType::DEVICE, appId, key, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<3>(expectedValue), // Set the output value
            ::testing::Return(Core::ERROR_NONE)));

    const std::string requestPayload =
        "{\"appId\":\"" + appId + "\",\"key\":\"" + key + "\"}";

    EXPECT_EQ(Core::ERROR_NONE, mHandler.Invoke(connection, _T("getAppProperty"), requestPayload, mResponse));

    Core::JSON::String responseJson;
    EXPECT_TRUE(responseJson.FromString(mResponse)) << "Failed to parse the response JSON";

    std::string actualValue = responseJson.Value();
    EXPECT_EQ(expectedValue, actualValue) << "The returned value does not match the expected value.";
    releaseResources();
}

/**
 * @brief       : Validates the getAppProperty API behavior when an empty appId is provided.
 *
 * @param[in]   : None.
 * @return      : Core::ERROR_GENERAL
 */
TEST_F(AppManagerTest, getAppPropertyFailed_withEmptyAppId)
{
    const std::string appId = "";  // Empty appId
    const std::string key = "app.property.key";
    std::string value;
    createResources();

    const std::string requestPayload =
        "{\"appId\":\"" + appId + "\",\"key\":\"" + key + "\",\"value\":\"" + value + "\"}";
    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection, _T("setAppProperty"), requestPayload, mResponse));
    releaseResources();
}

/**
 * @brief       : Validates the getAppProperty API behavior when an empty key is provided.
 *
 * @param[in]   : None.
 * @return      : Core::ERROR_GENERAL.
 */
TEST_F(AppManagerTest, getAppPropertyFailed_withEmptyKey)
{
    const std::string appId = "com.test.app";
    const std::string key = "";  // Empty key
    std::string value;
    createResources();

    const std::string requestPayload =
        "{\"appId\":\"" + appId + "\",\"key\":\"" + key + "\",\"value\":\"" + value + "\"}";
    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection, _T("getAppProperty"), requestPayload, mResponse));
    releaseResources();
}

/**
 * @brief       : Validates the getAppProperty API behavior when an persistent Object creation fails.
 *
 * @param[in]   : None.
 * @return      : Core::ERROR_GENERAL.
 */
TEST_F(AppManagerTest, getAppPropertyFailed_whenPersistentStoreObjectNull)
{
    const std::string appId = "com.test.app";
    const std::string key = "app.property.key";
    const std::string expectedValue = "property.valpue";

    ON_CALL(mService, COMLink())
        .WillByDefault(::testing::Invoke(
        [this]() {
             TEST_LOG("Pass created comLinkMock: %p ", &mComLinkMock);
             return &mComLinkMock;
            }));

    mServiceMock = new NiceMock<ServiceMock>;

    EXPECT_EQ(string(""), mPlugin->Initialize(&mService));
    EXPECT_CALL(mService, QueryInterfaceByCallsign(::testing::_, ::testing::_))
          .Times(::testing::AnyNumber())
          .WillRepeatedly(::testing::Invoke(
              [&](const uint32_t, const std::string& name) -> void* {
            return nullptr;
        }));

    const std::string requestPayload =
        "{\"appId\":\"" + appId + "\",\"key\":\"" + key + "\"}";

    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection, _T("getAppProperty"), requestPayload, mResponse));

    mPlugin->Deinitialize(&mService);

    // Clean up mocks
    if (mServiceMock != nullptr)
    {
        delete mServiceMock;
        mServiceMock = nullptr;
    }
}


/**
 * @brief       : Validates the getAppProperty API behavior when the GetValue operation from PersistentStore fails.
 *
 * @param[in]   : None.
 * @return      : Core::ERROR_GENERAL.
 */
TEST_F(AppManagerTest, getAppPropertyFailed_onGetValueFails)
{
    const std::string appId = "com.test.app";
    const std::string key = "app.property.key";
    std::string value;
    createResources();

    EXPECT_CALL(*mStore2Mock, GetValue(Exchange::IStore2::ScopeType::DEVICE, appId, key, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));

    const std::string requestPayload =
        "{\"appId\":\"" + appId + "\",\"key\":\"" + key + "\",\"value\":\"" + value + "\"}";
    EXPECT_EQ(Core::ERROR_GENERAL, mHandler.Invoke(connection, _T("getAppProperty"), requestPayload, mResponse));
    releaseResources();
}
