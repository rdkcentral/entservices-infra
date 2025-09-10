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
#include <gmock/gmock.h>

#include "SceneSet.h"
#include "ServiceMock.h"
#include "ThunderPortability.h"
#include "COMLinkMock.h"
#include "RequestHandler.h"
#include "Module.h"

class AppManagerMock : public WPEFramework::Exchange::IAppManager {
public:
    // IUnknown
    MOCK_METHOD(void*, QueryInterface, (const uint32_t), (override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(void, AddRef, (), (const, override));

    // IAppManager
    MOCK_METHOD(WPEFramework::Core::hresult, GetInstalledApps, (std::string&), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, IsInstalled, (const std::string&, bool&), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetLoadedApps, (std::string&), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, LaunchApp, (const std::string&, const std::string&, const std::string&), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, PreloadApp, (const std::string&, const std::string&, std::string&), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, CloseApp, (const std::string&), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, TerminateApp, (const std::string&), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, StartSystemApp, (const std::string&), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, StopSystemApp, (const std::string&), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, KillApp, (const std::string&), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, SendIntent, (const std::string&, const std::string&), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, ClearAppData, (const std::string&), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, ClearAllAppData, (), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetAppMetadata, (const std::string&, const std::string&, std::string&), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetAppProperty, (const std::string&, const std::string&, std::string&), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, SetAppProperty, (const std::string&, const std::string&, const std::string&), (override));

    MOCK_METHOD(WPEFramework::Core::hresult, GetMaxRunningApps, (int32_t&), (const, override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetMaxHibernatedApps, (int32_t&), (const, override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetMaxHibernatedFlashUsage, (int32_t&), (const, override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetMaxInactiveRamUsage, (int32_t&), (const, override));
    
    MOCK_METHOD(WPEFramework::Core::hresult, Register, (WPEFramework::Exchange::IAppManager::INotification*), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, Unregister, (WPEFramework::Exchange::IAppManager::INotification*), (override));
};

using namespace WPEFramework::Plugin;
using ::testing::Return;


class TestableSceneSet : public SceneSet {
public:
    void AddRef() const override {}
    uint32_t Release() const override { return 0; }
};

class SceneSetTest : public ::testing::Test {
protected:
    TestableSceneSet* sceneSet;

    void SetUp() override {
        sceneSet = new TestableSceneSet();
    }

    void TearDown() override {
        delete sceneSet;
    }
};

TEST_F(SceneSetTest, Construction) {
    ASSERT_NE(sceneSet, nullptr);
}

TEST_F(SceneSetTest, Initialize_WithAppManager_LaunchesApp) {
    ServiceMock serviceMock;
    AppManagerMock appManagerMock;
    EXPECT_CALL(serviceMock, ConfigLine()).WillRepeatedly(Return("{\"refAppName\":\"rdk-reference-app\"}"));
    EXPECT_CALL(serviceMock, AddRef()).Times(1);
    ON_CALL(serviceMock, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .WillByDefault([&](auto, auto) { return static_cast<void*>(&appManagerMock); });
    EXPECT_CALL(appManagerMock, LaunchApp(::testing::_, ::testing::_, ::testing::_)).WillOnce(Return(WPEFramework::Core::ERROR_NONE));
    EXPECT_CALL(appManagerMock, AddRef()).Times(1);
    EXPECT_CALL(appManagerMock, Register(::testing::_)).Times(1);

    std::string result = sceneSet->Initialize(&serviceMock);
    EXPECT_EQ(result, "");
}

TEST_F(SceneSetTest, Initialize_WithAppManager_FailsToLaunchApp) {
    ServiceMock serviceMock;
    AppManagerMock appManagerMock;
    EXPECT_CALL(serviceMock, ConfigLine()).WillRepeatedly(Return("{\"refAppName\":\"rdk-reference-app\"}"));
    EXPECT_CALL(serviceMock, AddRef()).Times(1);
    ON_CALL(serviceMock, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .WillByDefault([&](auto, auto) { return static_cast<void*>(&appManagerMock); });
    EXPECT_CALL(appManagerMock, LaunchApp(::testing::_, ::testing::_, ::testing::_)).WillOnce(Return(WPEFramework::Core::ERROR_GENERAL));
    EXPECT_CALL(appManagerMock, AddRef()).Times(1);
    EXPECT_CALL(appManagerMock, Register(::testing::_)).Times(1);

    std::string result = sceneSet->Initialize(&serviceMock);
    EXPECT_EQ(result, "");
}

TEST_F(SceneSetTest, Deinitialize_CallsRelease) {
    ServiceMock serviceMock;
    EXPECT_CALL(serviceMock, ConfigLine()).WillRepeatedly(Return("{\"refAppName\":\"rdk-reference-app\"}"));
    EXPECT_CALL(serviceMock, AddRef()).Times(1);
    sceneSet->Initialize(&serviceMock);
    EXPECT_CALL(serviceMock, Release()).Times(1);
    sceneSet->Deinitialize(&serviceMock);
}
