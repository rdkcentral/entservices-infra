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
#include <gtest/gtest.h>
#include<string>

#include "RuntimeManager.h"
#include "RuntimeManagerImplementation.h"
#include "HostMock.h"
#include "ServiceMock.h"
#include "ThunderPortability.h"
#include "StorageManagerMock.h"
#include "COMLinkMock.h"
#include "OCIContainerMock.h"

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);

using namespace WPEFramework;
using ::testing::NiceMock;

class RuntimeManagerTest : public ::testing::Test {
protected:
    string response;
    Core::ProxyType<Plugin::RuntimeManagerImplementation> mRuntimeManagerImpl;
    Exchange::IRuntimeManager* interface = nullptr;
    Exchange::IConfiguration* runTimeManagerConfigure = nullptr;
    ServiceMock     *mServiceMock = nullptr;
    StorageManagerMock* mStoreageManagerMock = nullptr;
    OCIContainerMock* mociContainerMock = nullptr;

    RuntimeManagerTest()
    {
        // create the RuntimeManagerImplementation instance
        mRuntimeManagerImpl = Core::ProxyType<Plugin::RuntimeManagerImplementation>::Create();

        interface = static_cast<Exchange::IRuntimeManager*>(
            mRuntimeManagerImpl->QueryInterface(Exchange::IRuntimeManager::ID));
    }
    virtual ~RuntimeManagerTest()
    {
        interface->Release();
     }

    bool createResources(void)
    {
        mStoreageManagerMock = new NiceMock<StorageManagerMock>;
        mociContainerMock = new NiceMock<OCIContainerMock>;
        mServiceMock =  new NiceMock<ServiceMock>;

        runTimeManagerConfigure = static_cast<Exchange::IConfiguration*>(
        mRuntimeManagerImpl->QueryInterface(Exchange::IConfiguration::ID));

        EXPECT_CALL(*mServiceMock, QueryInterfaceByCallsign(::testing::_, ::testing::_))
          .Times(::testing::AnyNumber())
          .WillRepeatedly(::testing::Invoke(
              [&](const uint32_t, const std::string& name) -> void* {
                if (name == "org.rdk.StorageManager") {
                    return reinterpret_cast<void*>(mStoreageManagerMock);
                }
                else if (name == "org.rdk.OCIContainer") {
                    return reinterpret_cast<void*>(mociContainerMock);
                }
            return nullptr;
        }));

        runTimeManagerConfigure->Configure(mServiceMock);
        return true;
    }

    void releaseResources()
    {
        // Clean up mocks
        if (mociContainerMock != nullptr)
        {
            EXPECT_CALL(*mociContainerMock, Release())
                .WillOnce(::testing::Invoke(
                [&]() {
                     delete mociContainerMock;
                     return 0;
                    }));
        }

        if (mServiceMock != nullptr)
        {
            EXPECT_CALL(*mServiceMock, Release())
                .WillOnce(::testing::Invoke(
                [&]() {
                     delete mServiceMock;
                     return 0;
                    }));
        }

        if (mStoreageManagerMock != nullptr)
        {
            EXPECT_CALL(*mStoreageManagerMock, Release())
                .WillOnce(::testing::Invoke(
                [&]() {
                     delete mStoreageManagerMock;
                     return 0;
                    }));
        }
        runTimeManagerConfigure->Release();
    }

    virtual void SetUp()
    {
        ASSERT_TRUE(interface != nullptr);
    }

    virtual void TearDown()
    {
        ASSERT_TRUE(interface != nullptr);
    }
};

TEST_F(RuntimeManagerTest, TerminateMethods)
{
    string appInstanceId("youTube");

    EXPECT_EQ(true, createResources());

    EXPECT_CALL(*mociContainerMock, StopContainer("com.sky.as.appsyouTube", false,::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](const string&, bool force, bool& success, string& errorReason) {
                success = true;
                errorReason = "No Error";
                return WPEFramework::Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_NONE, interface->Terminate(appInstanceId));
    releaseResources();
}

//Negative test cases: Terminate - Container Not Found (Terminate a non-existent container)
TEST_F(RuntimeManagerTest, TerminateNonExistentContainer) {

    string appInstanceId("invalid_id");

    EXPECT_EQ(true, createResources());

    EXPECT_CALL(*mociContainerMock, StopContainer(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [&](const string&, bool, bool& success, string& errorReason) {
                success = false;
                errorReason = "Container not found";
                return Core::ERROR_GENERAL;
            }));
    EXPECT_EQ(Core::ERROR_GENERAL, interface->Terminate(appInstanceId));
    releaseResources();
}

//Negative test cases: Terminate - invalid `appInstanceId`
TEST_F(RuntimeManagerTest, TerminateWithInvalidContainerId) {
    string appInstanceId("_567gshg&*rt-"); // Empty ID

    EXPECT_EQ(true, createResources());
    EXPECT_CALL(*mociContainerMock, StopContainer(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [&](const string&, bool, bool& success, string& errorReason) {
                success = false;
                errorReason = "Container not found";
                return Core::ERROR_GENERAL;
            }));
    EXPECT_EQ(Core::ERROR_GENERAL, interface->Terminate(appInstanceId));
    releaseResources();
    //EXPECT_EQ(Core::ERROR_GENERAL, interface->Terminate("invalid_container")); // Non-existent
}

//Negative test cases: Terminate - Already Terminated Container
TEST_F(RuntimeManagerTest, TerminateAlreadyStoppedContainer) {
    string appInstanceId("youTube");
    EXPECT_EQ(true, createResources());

    // Mock container as already stopped
    EXPECT_CALL(*mociContainerMock, StopContainer("com.sky.as.appsyouTube", false, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [&](const string&, bool, bool& success, string& errorReason) {
                success = false;
                errorReason = "Container already stopped";
                return Core::ERROR_GENERAL;
            }));

    EXPECT_EQ(Core::ERROR_GENERAL, interface->Terminate(appInstanceId));
    releaseResources();
}

//Negative test cases: Terminate with Force Flag Failing
//Forceful termination (`Kill`) fails due to system error
TEST_F(RuntimeManagerTest, TerminateWithForceFails) {
    string appInstanceId("youTube");
    EXPECT_EQ(true, createResources());

    // Mock force-stop failure
    EXPECT_CALL(*mociContainerMock, StopContainer("com.sky.as.appsyouTube", true, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [&](const string&, bool, bool& success, string& errorReason) {
                success = false;
                errorReason = "System error: Cannot kill process";
                return Core::ERROR_GENERAL;
            }));

    EXPECT_EQ(Core::ERROR_GENERAL, interface->Kill(appInstanceId));
    releaseResources();
}

//Negative test cases: Kill Non-Existent Container
//Forcefully terminate a container that doesn’t exist.
TEST_F(RuntimeManagerTest, KillNonExistentContainer) {
    EXPECT_EQ(true, createResources());
    EXPECT_CALL(*mociContainerMock, StopContainer("com.sky.as.appsInvalid", true, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [&](const string&, bool, bool& success, string& errorReason) {
                success = false;
                errorReason = "Container not found";
                return Core::ERROR_GENERAL;
            }));
    EXPECT_EQ(Core::ERROR_GENERAL, interface->Kill("Invalid"));
    releaseResources();
}


//Negative test cases: Kill with Insufficient Permissions
//Force-kill fails due to permission denial
TEST_F(RuntimeManagerTest, KillWithPermissionDenied) {
    EXPECT_EQ(true, createResources());
    EXPECT_CALL(*mociContainerMock, StopContainer(::testing::_, true, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [&](const string&, bool, bool& success, string& errorReason) {
                success = false;
                errorReason = "Permission denied";
                return Core::ERROR_GENERAL;
            }));
    EXPECT_EQ(Core::ERROR_GENERAL, interface->Kill("youTube"));
    releaseResources();
}

TEST_F(RuntimeManagerTest, HibernateMethods)
{
    string appInstanceId("youTube");

    EXPECT_EQ(true, createResources());

    EXPECT_CALL(*mociContainerMock, HibernateContainer("com.sky.as.appsyouTube", "",::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](const string&, const string&, bool& success, string& errorReason) {
                success = true;
                errorReason = "No Error";
                return WPEFramework::Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_NONE, interface->Hibernate(appInstanceId));
    releaseResources();
}

TEST_F(RuntimeManagerTest, KillMethods)
{
    string appInstanceId("youTube");

    EXPECT_EQ(true, createResources());

    EXPECT_CALL(*mociContainerMock, StopContainer("com.sky.as.appsyouTube", true,::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](const string&, bool force, bool& success, string& errorReason) {
                success = true;
                errorReason = "No Error";
                return WPEFramework::Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_NONE, interface->Kill(appInstanceId));
    releaseResources();
}

TEST_F(RuntimeManagerTest, AnnonateMethods)
{
    string appInstanceId("youTube");
    string appKey("youTube_Key");
    string appValue("youTube_Key");

    EXPECT_EQ(true, createResources());

    EXPECT_CALL(*mociContainerMock, Annotate("com.sky.as.appsyouTube", appKey,appValue,::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](const string& , const string& , const string& , bool& success , string& errorReason ){
                success = true;
                errorReason = "No Error";
                return WPEFramework::Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_NONE, interface->Annotate(appInstanceId, appKey, appValue));
    releaseResources();
}

//Negative test cases: Annotate with Empty Key/Value
TEST_F(RuntimeManagerTest, AnnotateWithEmptyKeyOrValue) {
    EXPECT_EQ(true, createResources());
    EXPECT_EQ(Core::ERROR_GENERAL, interface->Annotate("youTube", "", "value")); // Empty key
    releaseResources();
}

//Negative test cases: Annotate Non-Existent Container
//Add metadata to a non-existent container.
TEST_F(RuntimeManagerTest, AnnotateNonExistentContainer) {
    EXPECT_EQ(true, createResources());
    EXPECT_CALL(*mociContainerMock, Annotate("com.sky.as.appsInvalid", ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [&](const string&, const string&, const string&, bool& success, string& errorReason) {
                success = false;
                errorReason = "Container not found";
                return Core::ERROR_GENERAL;
            }));
    EXPECT_EQ(Core::ERROR_GENERAL, interface->Annotate("Invalid", "key", "value"));
    releaseResources();
}

TEST_F(RuntimeManagerTest, GetInfoMethods)
{
    string appInstanceId("youTube");
    string appInfo("");
    string expectedAppInfo("This Test YouTube");

    EXPECT_EQ(true, createResources());

    EXPECT_CALL(*mociContainerMock, GetContainerInfo("com.sky.as.appsyouTube", ::testing::_,::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](const string& , string& info ,  bool& success , string& errorReason ){
                info = expectedAppInfo;
                success = true;
                errorReason = "No Error";
                return WPEFramework::Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_NONE, interface->GetInfo(appInstanceId, appInfo));
    releaseResources();
}

TEST_F(RuntimeManagerTest, GetInfoForNonExistentContainer) {
    string appInfo;
    EXPECT_EQ(true, createResources());
    EXPECT_CALL(*mociContainerMock, GetContainerInfo("com.sky.as.appsInvalid", ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [&](const string&, string& info, bool& success, string& errorReason) {
                info = "";
                success = false;
                errorReason = "Container not found";
                return Core::ERROR_GENERAL;
            }));
    EXPECT_EQ(Core::ERROR_GENERAL, interface->GetInfo("Invalid", appInfo));
    EXPECT_TRUE(appInfo.empty());
    releaseResources();
}

TEST_F(RuntimeManagerTest, GetInfoForValidContainerReturnError) {
    string appInfo;
    EXPECT_EQ(true, createResources());
    EXPECT_CALL(*mociContainerMock, GetContainerInfo("com.sky.as.appsInvalid", ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [&](const string&, string& info, bool& success, string& errorReason) {
                info = "";
                success = true;
                errorReason = "No Error";
                return Core::ERROR_GENERAL;
            }));
    EXPECT_EQ(Core::ERROR_GENERAL, interface->GetInfo("Invalid", appInfo));
    EXPECT_TRUE(appInfo.empty());
    releaseResources();
}

TEST_F(RuntimeManagerTest, RunMethods)
{
    string appInstanceId("youTube");
    string appPath("/var/runTimeManager");
    string runtimePath("/tmp/runTimeManager");
    string expectedAppInfo("This Test YouTube");
    std::vector<std::string> envVarsList = {"XDG_RUNTIME_DIR=/tmp", "WAYLAND_DISPLAY=main"};
    std::vector<uint32_t> portList = {10, 20};
    std::vector<std::string> pathsList = {"/tmp", "/opt"};
    std::vector<std::string> debugSettingsList = {"MIL", "INFO"};

    auto envVarsIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(envVarsList);
    auto portsIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IValueIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IValueIterator>(portList);
    auto pathsListIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(pathsList);
    auto debugSettingsIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(debugSettingsList);

    EXPECT_EQ(true, createResources());

    EXPECT_CALL(*mociContainerMock, StartContainerFromDobbySpec("com.sky.as.appsyouTube", ::testing::_, "", "/tmp/main", ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](const string& , const string&, const string&, const string&,int32_t& descriptor, bool& success, string& errorReason ){
                descriptor = 100;
                success = true;
                errorReason = "No Error";
                return WPEFramework::Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_NONE, interface->Run(appInstanceId, appInstanceId, appPath, runtimePath, envVarsIterator, 10, 10, portsIterator, pathsListIterator, debugSettingsIterator));
    releaseResources();
}


//Failed Container Operations
TEST_F(RuntimeManagerTest, StartContainerFailure) {

    string appInstanceId("youTube");
    string appPath("/var/runTimeManager");
    string runtimePath("/tmp/runTimeManager");
    string expectedAppInfo("This Test YouTube");
    std::vector<std::string> envVarsList = {"XDG_RUNTIME_DIR=/tmp", "WAYLAND_DISPLAY=main"};
    std::vector<uint32_t> portList = {10, 20};
    std::vector<std::string> pathsList = {"/tmp", "/opt"};
    std::vector<std::string> debugSettingsList = {"MIL", "INFO"};

    auto envVarsIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(envVarsList);
    auto portsIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IValueIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IValueIterator>(portList);
    auto pathsListIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(pathsList);
    auto debugSettingsIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(debugSettingsList);

    EXPECT_EQ(true, createResources());
    EXPECT_CALL(*mociContainerMock, StartContainerFromDobbySpec(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [&](const string& , const string&, const string&, const string&,int32_t& descriptor, bool& success, string& errorReason ) {
                descriptor = -1;
                success = false;
                errorReason = "Dobby spec invalid";
                return Core::ERROR_GENERAL;
            }));
    EXPECT_EQ(Core::ERROR_GENERAL, interface->Run(appInstanceId, appInstanceId, appPath, runtimePath, envVarsIterator, 10, 10, portsIterator, pathsListIterator, debugSettingsIterator)); // Pass valid args
    releaseResources();
}

//Run with Empty Container ID
//Pass an empty `appInstanceId'
TEST_F(RuntimeManagerTest, RunWithEmptyContainerId) {

    std::vector<std::string> envVarsList = {"XDG_RUNTIME_DIR=/tmp", "WAYLAND_DISPLAY=main"};
    auto envVars = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(envVarsList);
    EXPECT_EQ(Core::ERROR_GENERAL, interface->Run(
        "", "", "/valid/path", "/valid/runtime",
        envVars, 10, 10, nullptr, nullptr, nullptr
    ));
}

// Run with Invalid Paths
// Pass non-existent paths for `appPath` or `runtimePath`
TEST_F(RuntimeManagerTest, RunWithInvalidPaths) {

    std::vector<std::string> envVarsList = {"XDG_RUNTIME_DIR=/tmp", "WAYLAND_DISPLAY=main"};
    EXPECT_EQ(true, createResources());
    auto envVars = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(envVarsList);
    EXPECT_EQ(Core::ERROR_GENERAL, interface->Run(
        "youTube", "youTube",
        "/non/existent/path", // Invalid appPath
        "/invalid/runtime",   // Invalid runtimePath
        envVars, 10, 10, nullptr, nullptr, nullptr
    ));
    releaseResources();
}

//Run with Null Iterators
//Pass `nullptr` for required iterators (`envVars`, `ports`, etc.).
TEST_F(RuntimeManagerTest, RunWithNullIterators) {

    EXPECT_EQ(true, createResources());
    EXPECT_EQ(Core::ERROR_GENERAL, interface->Run(
        "youTube", "youTube", "/valid/path", "/valid/runtime",
        nullptr,  // Null envVarsIterator
        10, 10,
        nullptr,  // Null portsIterator
        nullptr,  // Null pathsIterator
        nullptr   // Null debugSettingsIterator
    ));
    releaseResources();
}

//Run with Duplicate Container ID
//Start a container with an ID that’s already running.
TEST_F(RuntimeManagerTest, RunWithDuplicateContainerId) {
    EXPECT_EQ(true, createResources());
    EXPECT_CALL(*mociContainerMock, StartContainerFromDobbySpec(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [&](const string& , const string&, const string&, const string&,int32_t& descriptor, bool& success, string& errorReason ) {
                descriptor = -1;
                success = false;
                errorReason = "Container ID already exists";
                return Core::ERROR_GENERAL;
            }));

    std::vector<std::string> envVarsList = {"XDG_RUNTIME_DIR=/tmp", "WAYLAND_DISPLAY=main"};
    auto envVars = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(envVarsList);
    EXPECT_EQ(Core::ERROR_GENERAL, interface->Run(
        "youTube", "youTube", "/valid/path", "/valid/runtime",
        envVars, 10, 10, nullptr, nullptr, nullptr
    ));
    releaseResources();
}

// Run with Invalid Port Mappings
// Pass out-of-range ports (e.g., `0` or `65536`)
TEST_F(RuntimeManagerTest, RunWithInvalidPorts) {
    std::vector<uint32_t> portList = {0, 65536}; // Invalid ports
    auto invalidPorts = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IValueIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IValueIterator>(portList);

    std::vector<std::string> envVarsList = {"XDG_RUNTIME_DIR=/tmp", "WAYLAND_DISPLAY=main"};

    EXPECT_EQ(true, createResources());
    auto envVars = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(envVarsList);
    EXPECT_EQ(Core::ERROR_GENERAL, interface->Run(
        "youTube", "youTube", "/valid/path", "/valid/runtime",
        envVars, 10, 10, invalidPorts, nullptr, nullptr
    ));
    releaseResources();
}


// Run with Invalid Environment Variables
// Pass malformed env vars (e.g., missing `=`)
TEST_F(RuntimeManagerTest, RunWithInvalidEnvVars) {
    std::vector<std::string> envVarsList = {"MISSING_EQUALS_SIGN"}; // Invalid format
    auto invalidEnvVars = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(envVarsList);
    EXPECT_EQ(true, createResources());
    EXPECT_EQ(Core::ERROR_GENERAL, interface->Run(
        "youTube", "youTube", "/valid/path", "/valid/runtime",
        invalidEnvVars, 10, 10, nullptr, nullptr, nullptr
    ));
    releaseResources();
}


// Run with Timeout
// Simulate a timeout during container startup
TEST_F(RuntimeManagerTest, RunWithTimeout) {
    EXPECT_EQ(true, createResources());
    EXPECT_CALL(*mociContainerMock, StartContainerFromDobbySpec(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [&](const string& , const string&, const string&, const string&,int32_t& descriptor,  bool& success , string& errorReason ) {
                descriptor = -1;
                success = false;
                errorReason = "Operation timed out";
                return Core::ERROR_GENERAL;
            }));

    std::vector<std::string> envVarsList = {"XDG_RUNTIME_DIR=/tmp", "WAYLAND_DISPLAY=main"};
    auto envVars = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(envVarsList);
    EXPECT_EQ(Core::ERROR_GENERAL, interface->Run(
        "youTube", "youTube", "/valid/path", "/valid/runtime",
        envVars, 10, 10, nullptr, nullptr, nullptr
    ));
    releaseResources();
}

TEST_F(RuntimeManagerTest, WakeMethods)
{
    string appInstanceId("youTube");
    string appPath("/var/runTimeManager");
    string runtimePath("/tmp/runTimeManager");
    string expectedAppInfo("This Test YouTube");
    std::vector<std::string> envVarsList = {"XDG_RUNTIME_DIR=/tmp", "WAYLAND_DISPLAY=main"};
    std::vector<uint32_t> portList = {10, 20};
    std::vector<std::string> pathsList = {"/tmp", "/opt"};
    std::vector<std::string> debugSettingsList = {"MIL", "INFO"};

    auto envVarsIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(envVarsList);
    auto portsIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IValueIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IValueIterator>(portList);
    auto pathsListIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(pathsList);
    auto debugSettingsIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(debugSettingsList);

    EXPECT_EQ(true, createResources());

    EXPECT_CALL(*mociContainerMock, StartContainerFromDobbySpec("com.sky.as.appsyouTube", ::testing::_, "", "/tmp/main", ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](const string& , const string&, const string&, const string&,int32_t& descriptor, bool& success, string& errorReason ){
                descriptor = 100;
                success = true;
                errorReason = "No Error";
                return WPEFramework::Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_NONE, interface->Run(appInstanceId, appInstanceId, appPath, runtimePath, envVarsIterator, 10, 10, portsIterator, pathsListIterator, debugSettingsIterator));

    EXPECT_CALL(*mociContainerMock, HibernateContainer("com.sky.as.appsyouTube", "",::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](const string&, const string&, bool& success, string& errorReason) {
                success = true;
                errorReason = "No Error";
                return WPEFramework::Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_NONE, interface->Hibernate(appInstanceId));

    EXPECT_CALL(*mociContainerMock, WakeupContainer("com.sky.as.appsyouTube", ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](const string&, bool& success, string& errorReason) {
                success = true;
                errorReason = "No Error";
                return WPEFramework::Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_NONE, interface->Wake(appInstanceId, WPEFramework::Exchange::IRuntimeManager::RUNTIME_STATE_RUNNING));

    releaseResources();
}

TEST_F(RuntimeManagerTest, WakeOnRunningNonHibernateContainer)
{
    string appInstanceId("youTube");
    string appPath("/var/runTimeManager");
    string runtimePath("/tmp/runTimeManager");
    string expectedAppInfo("This Test YouTube");
    std::vector<std::string> envVarsList = {"XDG_RUNTIME_DIR=/tmp", "WAYLAND_DISPLAY=main"};
    std::vector<uint32_t> portList = {10, 20};
    std::vector<std::string> pathsList = {"/tmp", "/opt"};
    std::vector<std::string> debugSettingsList = {"MIL", "INFO"};

    auto envVarsIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(envVarsList);
    auto portsIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IValueIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IValueIterator>(portList);
    auto pathsListIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(pathsList);
    auto debugSettingsIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(debugSettingsList);

    EXPECT_EQ(true, createResources());

    EXPECT_CALL(*mociContainerMock, StartContainerFromDobbySpec("com.sky.as.appsyouTube", ::testing::_, "", "/tmp/main", ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](const string& , const string&, const string&, const string&,int32_t& descriptor, bool& success, string& errorReason ){
                descriptor = 100;
                success = true;
                errorReason = "No Error";
                return WPEFramework::Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_NONE, interface->Run(appInstanceId, appInstanceId, appPath, runtimePath, envVarsIterator, 10, 10, portsIterator, pathsListIterator, debugSettingsIterator));


    EXPECT_EQ(Core::ERROR_GENERAL, interface->Wake(appInstanceId, WPEFramework::Exchange::IRuntimeManager::RUNTIME_STATE_RUNNING));

    releaseResources();
}

TEST_F(RuntimeManagerTest, WakeWithGeneralError)
{
    string appInstanceId("youTube");
    string appPath("/var/runTimeManager");
    string runtimePath("/tmp/runTimeManager");
    string expectedAppInfo("This Test YouTube");
    std::vector<std::string> envVarsList = {"XDG_RUNTIME_DIR=/tmp", "WAYLAND_DISPLAY=main"};
    std::vector<uint32_t> portList = {10, 20};
    std::vector<std::string> pathsList = {"/tmp", "/opt"};
    std::vector<std::string> debugSettingsList = {"MIL", "INFO"};

    auto envVarsIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(envVarsList);
    auto portsIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IValueIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IValueIterator>(portList);
    auto pathsListIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(pathsList);
    auto debugSettingsIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(debugSettingsList);

    EXPECT_EQ(true, createResources());

    EXPECT_CALL(*mociContainerMock, StartContainerFromDobbySpec("com.sky.as.appsyouTube", ::testing::_, "", "/tmp/main", ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](const string& , const string&, const string&, const string&,int32_t& descriptor, bool& success, string& errorReason ){
                descriptor = 100;
                success = true;
                errorReason = "No Error";
                return WPEFramework::Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_NONE, interface->Run(appInstanceId, appInstanceId, appPath, runtimePath, envVarsIterator, 10, 10, portsIterator, pathsListIterator, debugSettingsIterator));

    EXPECT_CALL(*mociContainerMock, HibernateContainer("com.sky.as.appsyouTube", "",::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](const string&, const string&, bool& success, string& errorReason) {
                success = true;
                errorReason = "No Error";
                return WPEFramework::Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_NONE, interface->Hibernate(appInstanceId));

    EXPECT_CALL(*mociContainerMock, WakeupContainer("com.sky.as.appsyouTube", ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](const string&, bool& success, string& errorReason) {
                success = false;
                errorReason = "Genral Error";
                return WPEFramework::Core::ERROR_GENERAL;
          }));

    EXPECT_EQ(Core::ERROR_GENERAL, interface->Wake(appInstanceId, WPEFramework::Exchange::IRuntimeManager::RUNTIME_STATE_RUNNING));

    releaseResources();
}

TEST_F(RuntimeManagerTest, WakeOnNonRunningContainer)
{
    string appInstanceId("youTube");
    EXPECT_EQ(true, createResources());

    EXPECT_EQ(Core::ERROR_GENERAL, interface->Wake(appInstanceId, WPEFramework::Exchange::IRuntimeManager::RUNTIME_STATE_RUNNING));

    releaseResources();
}

TEST_F(RuntimeManagerTest, ResumeMethods)
{
    string appInstanceId("youTube");

    EXPECT_EQ(Core::ERROR_NONE, interface->Resume(appInstanceId));
}
