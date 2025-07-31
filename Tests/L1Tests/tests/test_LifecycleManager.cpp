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

namespace WPEFramework {
namespace Plugin {
class LifecycleManagerImplementationTest {
    public:
       LifecycleManagerImplementation *mLifecycleManagerImplTest;
       LifecycleManagerImplementationTest() {
            mLifecycleManagerImplTest = new LifecycleManagerImplementation();
       }
       ~LifecycleManagerImplementationTest() {
            delete mLifecycleManagerImplTest;
       }
};
} // namespace Plugin

namespace Exchange {

struct INotificationTest : public ILifecycleManager::INotification {
    MOCK_METHOD(void, OnAppStateChanged, (const std::string& appId, ILifecycleManager::LifecycleState state, const std::string& errorReason), (override));
    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(void*, QueryInterface, (const uint32_t interfaceNumber), (override));
};

struct IStateNotificationTest : public ILifecycleManagerState::INotification {
    MOCK_METHOD(void, OnAppLifecycleStateChanged, (const std::string& appId, const std::string& appInstanceId, ILifecycleManager::LifecycleState oldState, ILifecycleManager::LifecycleState newState,  const std::string& navigationIntent), (override));
    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(void*, QueryInterface, (const uint32_t interfaceNumber), (override));
};
} // namespace Exchange
} // namespace WPEFramework


using ::testing::NiceMock;
using namespace WPEFramework;
using namespace std;

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
    Exchange::ILifecycleManagerState* stateInterface = nullptr;
    Exchange::IConfiguration* mLifecycleManagerConfigure = nullptr;
    RuntimeManagerMock* mRuntimeManagerMock = nullptr;
    WindowManagerMock* mWindowManagerMock = nullptr;
    ServiceMock* mServiceMock = nullptr;
    Core::ProxyType<WorkerPoolImplementation> workerPool;

    LifecycleManagerTest()
	: workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(
            2, Core::Thread::DefaultStackSize(), 16))
    {
        mLifecycleManagerImpl = Core::ProxyType<Plugin::LifecycleManagerImplementation>::Create();
        
        interface = static_cast<Exchange::ILifecycleManager*>(mLifecycleManagerImpl->QueryInterface(Exchange::ILifecycleManager::ID));

        stateInterface = static_cast<Exchange::ILifecycleManagerState*>(mLifecycleManagerImpl->QueryInterface(Exchange::ILifecycleManagerState::ID));

		Core::IWorkerPool::Assign(&(*workerPool));
		workerPool->Run();
    }

    virtual ~LifecycleManagerTest() override
    {
		Core::IWorkerPool::Assign(nullptr);
		workerPool.Release();

		interface->Release();
		stateInterface->Release();
    }

    void SetUp() override 
    {
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
		
		// Set up mocks and expect calls
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

		EXPECT_CALL(*mServiceMock, AddRef())
          .Times(::testing::AnyNumber());
	
		EXPECT_CALL(*mRuntimeManagerMock, Register(::testing::_))
          .WillRepeatedly(::testing::Return(Core::ERROR_NONE));
    
		EXPECT_CALL(*mWindowManagerMock, Register(::testing::_))
          .WillRepeatedly(::testing::Return(Core::ERROR_NONE));

        // Configure the LifecycleManager
        mLifecycleManagerConfigure->Configure(mServiceMock);
 
        ASSERT_TRUE(interface != nullptr);
    }

    void TearDown() override
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

        if (mRuntimeManagerMock != nullptr)
        {
			EXPECT_CALL(*mRuntimeManagerMock, Unregister(::testing::_))
              .WillRepeatedly(::testing::Return(Core::ERROR_NONE));

			EXPECT_CALL(*mRuntimeManagerMock, Release())
              .WillOnce(::testing::Invoke(
              [&]() {
						delete mRuntimeManagerMock;
						mRuntimeManagerMock = nullptr;
						return 0;
					}));
        }

        if (mWindowManagerMock != nullptr)
        {
			EXPECT_CALL(*mWindowManagerMock, Unregister(::testing::_))
              .WillRepeatedly(::testing::Return(Core::ERROR_NONE));

			EXPECT_CALL(*mWindowManagerMock, Release())
              .WillOnce(::testing::Invoke(
              [&]() {
						delete mWindowManagerMock;
						mWindowManagerMock = nullptr;
						return 0;
                    }));
        }

		// Clean up the LifecycleManager
        mLifecycleManagerConfigure->Release();
	
        ASSERT_TRUE(interface != nullptr);
    }
};

/* Test Case for Registering Notification
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Create a mock notification instance and initialize it using the INotificationTest struct
 * Register the notification with the Lifecycle Manager interface
 * Verify successful registration of notification by asserting that Register() returns Core::ERROR_NONE
 * Release the Lifecycle Manager interface object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, registerNotification)
{
    Exchange::ILifecycleManager::INotification* notification = new Exchange::INotificationTest();

    // TC-1: Check if the notification is registered
    EXPECT_EQ(Core::ERROR_NONE, interface->Register(notification));

    delete notification;
    notification = nullptr;
}

/* Test Case for Unregistering Notification after registering
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Create a mock notification instance and initialize it using the INotificationTest struct
 * Register the notification with the Lifecycle Manager interface
 * Verify successful registration of notification by asserting that Register() returns Core::ERROR_NONE
 * Unregister the notification from the Lifecycle Manager interface
 * Verify successful unregistration of notification by asserting that Unregister() returns Core::ERROR_NONE
 * Release the Lifecycle Manager interface object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, unregisterNotification_afterRegister)
{
    Exchange::ILifecycleManager::INotification* notification = new Exchange::INotificationTest();

    // TC-2: Check if the notification is unregistered after registering
    EXPECT_EQ(Core::ERROR_NONE, interface->Register(notification));

    EXPECT_EQ(Core::ERROR_NONE, interface->Unregister(notification));

    delete notification;
    notification = nullptr;
}

/* Test Case for Unregistering Notification without registering
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Create a mock notification instance and initialize it using the INotificationTest struct
 * Unregister the notification from the Lifecycle Manager interface
 * Verify unregistration of notification fails by asserting that Unregister() returns Core::ERROR_GENERAL
 * Release the Lifecycle Manager interface object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, unregisterNotification_withoutRegister)
{
    Exchange::ILifecycleManager::INotification* notification = new Exchange::INotificationTest();
	
	// TC-3: Check if the notification is unregistered without registering
    EXPECT_EQ(Core::ERROR_GENERAL, interface->Unregister(notification));

    delete notification;
    notification = nullptr;
}

/* Test Case for Registering State Notification
 * 
 * Set up Lifecycle Manager state interface, configurations, required COM-RPC resources, mocks and expectations
 * Create a mock state notification instance and initialize it using the IStateNotificationTest struct
 * Register the state notification with the Lifecycle Manager state interface
 * Verify successful registration of state notification by asserting that Register() returns Core::ERROR_NONE
 * Release the Lifecycle Manager state interface object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, registerStateNotification)
{
    Exchange::ILifecycleManagerState::INotification* notification = new Exchange::IStateNotificationTest();

    // TC-4: Check if the state notification is registered
    EXPECT_EQ(Core::ERROR_NONE, stateInterface->Register(notification));

    delete notification;
    notification = nullptr;
}

/* Test Case for Unregistering State Notification after registering
 * 
 * Set up Lifecycle Manager state interface, configurations, required COM-RPC resources, mocks and expectations
 * Create a mock state notification instance and initialize it using the IStateNotificationTest struct
 * Register the state notification with the Lifecycle Manager state interface
 * Verify successful registration of state notification by asserting that Register() returns Core::ERROR_NONE
 * Unregister the state notification from the Lifecycle Manager state interface
 * Verify successful unregistration of state notification by asserting that Unregister() returns Core::ERROR_NONE
 * Release the Lifecycle Manager state interface object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, unregisterStateNotification_afterRegister)
{
    Exchange::ILifecycleManagerState::INotification* notification = new Exchange::IStateNotificationTest();

    // TC-5: Check if the state notification is registered after unregistering
    EXPECT_EQ(Core::ERROR_NONE, stateInterface->Register(notification));

    EXPECT_EQ(Core::ERROR_NONE, stateInterface->Unregister(notification));

    delete notification;
    notification = nullptr;   
}

/* Test Case for Unregistering State Notification without registering
 * 
 * Set up Lifecycle Manager state interface, configurations, required COM-RPC resources, mocks and expectations
 * Create a mock state notification instance and initialize it using the IStateNotificationTest struct
 * Unregister the state notification from the Lifecycle Manager state interface
 * Verify unregistration of state notification fails by asserting that Unregister() returns Core::ERROR_GENERAL
 * Release the Lifecycle Manager state interface object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, unregisterStateNotification_withoutRegister)
{
    Exchange::ILifecycleManagerState::INotification* notification = new Exchange::IStateNotificationTest();
	
	// TC-6: Check if the state notification is registered without unregistering
    EXPECT_EQ(Core::ERROR_GENERAL, stateInterface->Unregister(notification));

    delete notification;
    notification = nullptr;
}

/* Test Case for Spawning an App
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Spawn an app with valid parameters from the Lifecycle Manager interface with target state as LOADING.
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Release the Lifecycle Manager interface object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, spawnApp_withValidParams)
{
    // TC-7: Spawn an app with all parameters valid
    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));
}

/* Test Case for App Ready after Spawning
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Create an object instance of LifecycleManagerImplementationTest
 * Spawn an app with valid parameters from the LifecycleManagerImplementationTest instance with target state as LOADING.
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Check if the app is ready after spawning from the LifecycleManagerImplementationTest instance with the appId
 * Verify that the app is ready by asserting that AppReady() returns Core::ERROR_NONE
 * Obtain the loaded app context using getContext() and wait for the app ready semaphore
 * Release the Lifecycle Manager interface object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, appready_onSpawnAppSuccess) 
{
    Plugin::LifecycleManagerImplementationTest obj;

    EXPECT_EQ(Core::ERROR_NONE, obj.mLifecycleManagerImplTest->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));
    
	// TC-8: Check if app is ready after spawning
    EXPECT_EQ(Core::ERROR_NONE, obj.mLifecycleManagerImplTest->AppReady(appId));

    Plugin::ApplicationContext* context = obj.mLifecycleManagerImplTest->getContext("", appId);

    sem_wait(&context->mAppReadySemaphore);    
}

/* Test Case for App Ready with invalid AppId after Spawning 
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Spawn an app with valid parameters from the LifecycleManager interface with target state as LOADING.
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Check failure of app ready due to invalid appId by asserting that AppReady() returns Core::ERROR_GENERAL
 * Release the Lifecycle Manager interface object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, appready_oninvalidAppId) 
{
	EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));
	
	// TC-9: Verify error on passing an invalid appId
    EXPECT_EQ(Core::ERROR_GENERAL, stateInterface->AppReady(""));
}

/* Test Case for querying if App is Loaded after Spawning
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Spawn an app with valid parameters from the LifecycleManager interface with target state as LOADING.
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Check if the app is loaded after spawning from the LifecycleManager interface with the appId
 * Verify that the app is loaded by asserting that IsAppLoaded() returns Core::ERROR_NONE
 * Check that the loaded flag is set to true, confirming that the app is loaded
 * Release the Lifecycle Manager interface object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, isAppLoaded_onSpawnAppSuccess) 
{
    bool loaded = false;
    
    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

	// TC-10: Check if app is loaded after spawning
    EXPECT_EQ(Core::ERROR_NONE, interface->IsAppLoaded(appId, loaded));

    EXPECT_EQ(loaded, true);    
}

/* Test Case for querying if App is Loaded with invalid AppId after Spawning
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Spawn an app with valid parameters from the LifecycleManager interface with target state as LOADING.
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Check failure of app loaded due to invalid appId by asserting that IsAppLoaded() returns Core::ERROR_GENERAL
 * Check that the loaded flag is set to false, confirming that the app is not loaded
 * Release the Lifecycle Manager interface object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, isAppLoaded_oninvalidAppId)
{
    bool loaded = true;

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));
	
	// TC-11: Verify error on passing an invalid appId
    EXPECT_EQ(Core::ERROR_NONE, interface->IsAppLoaded("", loaded));

    EXPECT_EQ(loaded, false);
}

/* Test Case for getLoadedApps with verbose enabled after Spawning
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Enable the verbose flag by setting it to true
 * Spawn an app with valid parameters from the LifecycleManager interface with target state as LOADING
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Obtain the loaded apps and assert that GetLoadedApps() returns Core::ERROR_NONE
 * Verify the app list parameters by comparing the obtained and expected app list
 * Release the Lifecycle Manager interface object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, getLoadedApps_verboseEnabled)
{
    bool verbose = true;
    string apps = "";

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));
	
	// TC-12: Get loaded apps with verbose enabled
    EXPECT_EQ(Core::ERROR_NONE, interface->GetLoadedApps(verbose, apps)); 
    
    // Add an expect eq to check the apps
}

/* Test Case for getLoadedApps with verbose disabled after Spawning
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Disable the verbose flag by setting it to false
 * Spawn an app with valid parameters from the LifecycleManager interface with target state as LOADING
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Obtain the loaded apps and assert that GetLoadedApps() returns Core::ERROR_NONE
 * Verify the app list parameters by comparing the obtained and expected app list
 * Release the Lifecycle Manager interface object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, getLoadedApps_verboseDisabled)
{
    bool verbose = false;
    string apps = "";

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));
	
	// TC-13: Get loaded apps with verbose disabled
    EXPECT_EQ(Core::ERROR_NONE, interface->GetLoadedApps(verbose, apps));    
}

/* Test Case for getLoadedApps with verbose enabled without Spawning
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Enable the verbose flag by setting it to true
 * Obtain the loaded apps and assert that GetLoadedApps() returns Core::ERROR_NONE
 * Verify the app list parameters is null indicating no apps are loaded
 * Release the Lifecycle Manager interface object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, getLoadedApps_noAppsLoaded)
{
    bool verbose = true;
    string apps = "";

    // TC-14: Check that no apps are loaded
    EXPECT_EQ(Core::ERROR_NONE, interface->GetLoadedApps(verbose, apps));

    EXPECT_EQ(apps, "[]");
}

/* Test Case for setTargetAppState with valid parameters
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Spawn an app with valid parameters from the LifecycleManager interface with target state as LOADING
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Set the target state of the app to LOADING with valid parameters
 * Verify successful state change by asserting that SetTargetAppState() returns Core::ERROR_NONE
 * Release the Lifecycle Manager interface object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, setTargetAppState_withValidParams)
{
    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    // TC-15: Set the target state of a loaded app with all parameters valid
    EXPECT_EQ(Core::ERROR_NONE, interface->SetTargetAppState(appInstanceId, targetLifecycleState, launchIntent));

    // TC-16: Set the target state of a loaded app with only required parameters valid
    EXPECT_EQ(Core::ERROR_NONE, interface->SetTargetAppState(appInstanceId, targetLifecycleState, "")); 
}

/* Test Case for setTargetAppState with invalid parameters
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Spawn an app with valid parameters from the LifecycleManager interface with target state as LOADING
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Set the target state of the app to LOADING with invalid appInstanceId
 * Verify state change fails by asserting that SetTargetAppState() returns Core::ERROR_GENERAL
 * Release the Lifecycle Manager interface object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, setTargetAppState_withinvalidParams)
{
    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    // TC-17: Set the target state of a loaded app with invalid appInstanceId
    EXPECT_EQ(Core::ERROR_GENERAL, interface->SetTargetAppState("", targetLifecycleState, launchIntent));    
}

/* Test Case for Unload App after Spawning
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Create an object instance of LifecycleManagerImplementationTest
 * Spawn an app with valid parameters from the LifecycleManagerImplementationTest instance with target state as LOADING.
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Unload the app using the appInstanceId with the LifecycleManagerImplementationTest instance.
 * Verify that app is successfully unloaded by asserting that UnloadApp() returns Core::ERROR_NONE
 * Obtain the loaded app context using getContext() and post the app running and app terminating semaphore
 * Release the Lifecycle Manager interface object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, unloadApp_onSpawnAppSuccess)
{
    EXPECT_CALL(*mRuntimeManagerMock, Run(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const string& appInstanceId, const uint32_t userId, const uint32_t groupId, Exchange::IRuntimeManager::IValueIterator* const& ports, Exchange::IRuntimeManager::IStringIterator* const& paths, Exchange::IRuntimeManager::IStringIterator* const& debugSettings, const Exchange::RuntimeConfig& runtimeConfigObject) {
                return Core::ERROR_NONE;
          }));

    EXPECT_CALL(*mRuntimeManagerMock, Terminate(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appInstanceId) {
                return Core::ERROR_NONE;
          }));

    Plugin::LifecycleManagerImplementationTest obj;

    EXPECT_EQ(Core::ERROR_NONE, obj.mLifecycleManagerImplTest->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));
    
	// TC-18: Unload the app after spawning
    EXPECT_EQ(Core::ERROR_NONE, obj.mLifecycleManagerImplTest->UnloadApp(appInstanceId, errorReason, success));

    Plugin::ApplicationContext* context = obj.mLifecycleManagerImplTest->getContext("", appId);

    sem_post(&context->mAppRunningSemaphore);

    sem_post(&context->mAppTerminatingSemaphore);    
}

/* Test Case for Unload App without Spawning
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Create an object instance of LifecycleManagerImplementationTest and set the appInstanceId to a random test value.
 * Unload the app using the appInstanceId with the LifecycleManagerImplementationTest instance.
 * Verify failure of app unload by asserting that UnloadApp() returns Core::ERROR_GENERAL
 * Release the Lifecycle Manager interface object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, unloadApp_withoutSpawning)
{
    appInstanceId = "test.app.instance";

    // TC-19: Unload the app after spawn fails
    EXPECT_EQ(Core::ERROR_GENERAL, interface->UnloadApp(appInstanceId, errorReason, success));
}

/* Test Case for Kill App after Spawning
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Create an object instance of LifecycleManagerImplementationTest
 * Spawn an app with valid parameters from the LifecycleManagerImplementationTest instance with target state as LOADING.
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Kill the app using the appInstanceId with the LifecycleManagerImplementationTest instance.
 * Verify that app is successfully killed by asserting that KillApp() returns Core::ERROR_NONE
 * Obtain the loaded app context using getContext() and post the app running and app terminating semaphore
 * Release the Lifecycle Manager interface object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, killApp_onSpawnAppSuccess)
{
    EXPECT_CALL(*mRuntimeManagerMock, Run(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const string& appInstanceId, const uint32_t userId, const uint32_t groupId, Exchange::IRuntimeManager::IValueIterator* const& ports, Exchange::IRuntimeManager::IStringIterator* const& paths, Exchange::IRuntimeManager::IStringIterator* const& debugSettings, const Exchange::RuntimeConfig& runtimeConfigObject) {
                return Core::ERROR_NONE;
          }));

    EXPECT_CALL(*mRuntimeManagerMock, Kill(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appInstanceId) {
                return Core::ERROR_NONE;
          }));

    Plugin::LifecycleManagerImplementationTest obj;

    EXPECT_EQ(Core::ERROR_NONE, obj.mLifecycleManagerImplTest->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));
    
	// TC-20: Kill the app after spawning
    EXPECT_EQ(Core::ERROR_NONE, obj.mLifecycleManagerImplTest->KillApp(appInstanceId, errorReason, success));

    Plugin::ApplicationContext* context = obj.mLifecycleManagerImplTest->getContext("", appId);

    sem_post(&context->mAppRunningSemaphore);

    sem_post(&context->mAppTerminatingSemaphore);    
}

/* Test Case for Kill App without Spawning
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Create an object instance of LifecycleManagerImplementationTest and set the appInstanceId to a random test value.
 * Kill the app using the appInstanceId with the LifecycleManagerImplementationTest instance.
 * Verify failure of app kill by asserting that KillApp() returns Core::ERROR_GENERAL
 * Release the Lifecycle Manager interface object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, killApp_withoutSpawning)
{
    appInstanceId = "test.app.instance";

    // TC-21: Kill the app after spawn fails
    EXPECT_EQ(Core::ERROR_GENERAL, interface->KillApp(appInstanceId, errorReason, success));   
}

/* Test Case for Close App on User Exit
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Create an object instance of LifecycleManagerImplementationTest
 * Spawn an app with valid parameters from the LifecycleManagerImplementationTest instance with target state as LOADING.
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Close the app using the appId and setting the reason for close as USER EXIT with the LifecycleManagerImplementationTest instance.
 * Verify that app is successfully closed by asserting that CloseApp() returns Core::ERROR_NONE
 * Obtain the loaded app context using getContext() and post the app running and app terminating semaphore
 * Release the Lifecycle Manager interface object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, closeApp_onUserExit) 
{
    EXPECT_CALL(*mRuntimeManagerMock, Run(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const string& appInstanceId, const uint32_t userId, const uint32_t groupId, Exchange::IRuntimeManager::IValueIterator* const& ports, Exchange::IRuntimeManager::IStringIterator* const& paths, Exchange::IRuntimeManager::IStringIterator* const& debugSettings, const Exchange::RuntimeConfig& runtimeConfigObject) {
                return Core::ERROR_NONE;
          }));

    EXPECT_CALL(*mRuntimeManagerMock, Kill(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appInstanceId) {
                return Core::ERROR_NONE;
          }));

    Plugin::LifecycleManagerImplementationTest obj;

    EXPECT_EQ(Core::ERROR_NONE, obj.mLifecycleManagerImplTest->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));
    
	// TC-22: User exits the app after spawning 
    EXPECT_EQ(Core::ERROR_NONE, obj.mLifecycleManagerImplTest->CloseApp(appId, Exchange::ILifecycleManagerState::AppCloseReason::USER_EXIT));

    Plugin::ApplicationContext* context = obj.mLifecycleManagerImplTest->getContext("", appId);

    sem_post(&context->mAppRunningSemaphore);

    sem_post(&context->mAppTerminatingSemaphore); 
}

/* Test Case for Close App on Error
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Create an object instance of LifecycleManagerImplementationTest
 * Spawn an app with valid parameters from the LifecycleManagerImplementationTest instance with target state as LOADING.
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Close the app using the appId and setting the reason for close as ERROR with the LifecycleManagerImplementationTest instance.
 * Verify that app is successfully closed by asserting that CloseApp() returns Core::ERROR_NONE
 * Obtain the loaded app context using getContext() and post the app running and app terminating semaphore
 * Release the Lifecycle Manager interface object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, closeApp_onError) 
{
    EXPECT_CALL(*mRuntimeManagerMock, Run(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const string& appInstanceId, const uint32_t userId, const uint32_t groupId, Exchange::IRuntimeManager::IValueIterator* const& ports, Exchange::IRuntimeManager::IStringIterator* const& paths, Exchange::IRuntimeManager::IStringIterator* const& debugSettings, const Exchange::RuntimeConfig& runtimeConfigObject) {
                return Core::ERROR_NONE;
          }));

    EXPECT_CALL(*mRuntimeManagerMock, Kill(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appInstanceId) {
                return Core::ERROR_NONE;
          }));

    Plugin::LifecycleManagerImplementationTest obj;

    EXPECT_EQ(Core::ERROR_NONE, obj.mLifecycleManagerImplTest->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));
    
	// TC-23: Error after spawning the app
    EXPECT_EQ(Core::ERROR_NONE, obj.mLifecycleManagerImplTest->CloseApp(appId, Exchange::ILifecycleManagerState::AppCloseReason::ERROR));

    Plugin::ApplicationContext* context = obj.mLifecycleManagerImplTest->getContext("", appId);

    sem_post(&context->mAppRunningSemaphore);

    sem_post(&context->mAppTerminatingSemaphore);
}

/* Test Case for Close App on Kill and Run
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Create an object instance of LifecycleManagerImplementationTest
 * Spawn an app with valid parameters from the LifecycleManagerImplementationTest instance with target state as LOADING.
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Close the app using the appId and setting the reason for close as KILL AND RUN with the LifecycleManagerImplementationTest instance.
 * Verify that app is successfully closed by asserting that CloseApp() returns Core::ERROR_NONE
 * Obtain the loaded app context using getContext() and post the app running (twice) and app terminating semaphore
 * Release the Lifecycle Manager interface object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, closeApp_onKillandRun) 
{
    EXPECT_CALL(*mRuntimeManagerMock, Run(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const string& appInstanceId, const uint32_t userId, const uint32_t groupId, Exchange::IRuntimeManager::IValueIterator* const& ports, Exchange::IRuntimeManager::IStringIterator* const& paths, Exchange::IRuntimeManager::IStringIterator* const& debugSettings, const Exchange::RuntimeConfig& runtimeConfigObject) {
                return Core::ERROR_NONE;
          }));

    EXPECT_CALL(*mRuntimeManagerMock, Kill(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appInstanceId) {
                return Core::ERROR_NONE;
          }));

    Plugin::LifecycleManagerImplementationTest obj;

    EXPECT_EQ(Core::ERROR_NONE, obj.mLifecycleManagerImplTest->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));
    
	// TC-24: Kill and run after spawning the app
    EXPECT_EQ(Core::ERROR_NONE, obj.mLifecycleManagerImplTest->CloseApp(appId, Exchange::ILifecycleManagerState::AppCloseReason::KILL_AND_RUN));

    Plugin::ApplicationContext* context = obj.mLifecycleManagerImplTest->getContext("", appId);

    sem_post(&context->mAppRunningSemaphore);

    sem_post(&context->mAppTerminatingSemaphore);

    sem_post(&context->mAppRunningSemaphore);    
}

/* Test Case for Close App on Kill and Activate
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Create an object instance of LifecycleManagerImplementationTest
 * Spawn an app with valid parameters from the LifecycleManagerImplementationTest instance with target state as LOADING.
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Close the app using the appId and setting the reason for close as KILL AND ACTIVATE with the LifecycleManagerImplementationTest instance.
 * Verify that app is successfully closed by asserting that CloseApp() returns Core::ERROR_NONE
 * Obtain the loaded app context using getContext() and post the app running (twice), app terminating and first frame semaphore
 * Release the Lifecycle Manager interface object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, closeApp_onKillandActivate) 
{
    EXPECT_CALL(*mRuntimeManagerMock, Run(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const string& appInstanceId, const uint32_t userId, const uint32_t groupId, Exchange::IRuntimeManager::IValueIterator* const& ports, Exchange::IRuntimeManager::IStringIterator* const& paths, Exchange::IRuntimeManager::IStringIterator* const& debugSettings, const Exchange::RuntimeConfig& runtimeConfigObject) {
                return Core::ERROR_NONE;
          }));

    EXPECT_CALL(*mRuntimeManagerMock, Kill(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appInstanceId) {
                return Core::ERROR_NONE;
          }));

    EXPECT_CALL(*mWindowManagerMock, RenderReady(::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& client, bool &status) {
                return Core::ERROR_NONE;
          }));

    Plugin::LifecycleManagerImplementationTest obj;

    EXPECT_EQ(Core::ERROR_NONE, obj.mLifecycleManagerImplTest->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));
    
	// TC-25: Kill and activate after spawning the app
    EXPECT_EQ(Core::ERROR_NONE, obj.mLifecycleManagerImplTest->CloseApp(appId, Exchange::ILifecycleManagerState::AppCloseReason::KILL_AND_ACTIVATE));

    Plugin::ApplicationContext* context = obj.mLifecycleManagerImplTest->getContext("", appId);

    sem_post(&context->mAppRunningSemaphore);

    sem_post(&context->mAppTerminatingSemaphore);

    sem_post(&context->mAppRunningSemaphore);

    sem_post(&context->mFirstFrameSemaphore);
}

/* Test Case for State Change Complete with valid parameters
 * 
 * Set up Lifecycle Manager state interface, configurations, required COM-RPC resources, mocks and expectations
 * Set the stateChangedId to a random test value
 * Using the state interface, signal that the state change is complete
 * Verify successful state change by asserting that StateChangeComplete() returns Core::ERROR_NONE
 * Release the Lifecycle Manager state interface object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, stateChangeComplete_withValidParams) 
{
    uint32_t stateChangedId = 1;
    
	// TC-26: Check if state change is complete
    EXPECT_EQ(Core::ERROR_NONE, stateInterface->StateChangeComplete(appId, stateChangedId, success));
}

/* Test Case for Send Intent to Active App after Spawning
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Set the intent to a random test value
 * Spawn an app with valid parameters from the LifecycleManager interface with target state as LOADING
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Send an intent to the active app using the appInstanceId with the LifecycleManager interface
 * Verify failure of sent intent (due to failure in websocket) by asserting that SendIntentToActiveApp() returns Core::ERROR_GENERAL
 * Release the Lifecycle Manager interface object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, sendIntenttoActiveApp_onSpawnAppSuccess)
{
    string intent = "test.intent";

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    // TC-27: Send intent to the app after spawning
    EXPECT_EQ(Core::ERROR_GENERAL, interface->SendIntentToActiveApp(appInstanceId, intent, errorReason, success));   
}


