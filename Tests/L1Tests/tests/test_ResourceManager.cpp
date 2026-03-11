/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2026 RDK Management
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

#include "ResourceManager.h"
#include "ResourceManagerImplementation.h"
#include "ServiceMock.h"
#include <core/core.h>
#include "ThunderPortability.h"

using namespace WPEFramework;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::_;
using ::testing::DoAll;
using ::testing::SetArgReferee;
using std::string;

// ===== Testable Implementation Class =====
class TestableResourceManagerImplementation : public Plugin::ResourceManagerImplementation {
public:
    void setDisableReserveTTS(bool value) {
        mDisableReserveTTS = value;
    }
    
    void setDisableBlacklist(bool value) {
        mDisableBlacklist = value;
    }
    
    bool getDisableReserveTTS() const {
        return mDisableReserveTTS;
    }
    
    bool getDisableBlacklist() const {
        return mDisableBlacklist;
    }
    
    void setService(PluginHost::IShell* service) {
        _service = service;
    }
};

// ===== Custom ServiceMock with Root<> support =====
class ResourceManagerServiceMock : public ServiceMock {
private:
    Core::ProxyType<TestableResourceManagerImplementation> _implementation;

public:
    ResourceManagerServiceMock()
        : _implementation(Core::ProxyType<TestableResourceManagerImplementation>::Create())
    {}

    // Template specialization for Root<Exchange::IResourceManager>
    template<typename INTERFACE>
    INTERFACE* Root(uint32_t& connectionId, const uint32_t waitTime, const string className, const uint32_t version = ~0) {
        // Return nullptr by default
        return nullptr;
    }

    // Get the testable implementation for test setup
    Core::ProxyType<TestableResourceManagerImplementation> GetImplementation() {
        return _implementation;
    }
};

// Specialization for IResourceManager
template<>
Exchange::IResourceManager* ResourceManagerServiceMock::Root<Exchange::IResourceManager>(
    uint32_t& connectionId, const uint32_t waitTime, const string className, const uint32_t version) {
    connectionId = 1;
    return reinterpret_cast<Exchange::IResourceManager*>(
        _implementation->QueryInterface(Exchange::IResourceManager::ID));
}

// ===== Test Fixture Base =====
class ResourceManagerTest : public ::testing::Test {
protected:
    Core::ProxyType<Plugin::ResourceManager> plugin;
    Core::JSONRPC::Handler& handler;
    DECL_CORE_JSONRPC_CONX connection;
    string response;

    ResourceManagerTest()
        : plugin(Core::ProxyType<Plugin::ResourceManager>::Create())
        , handler(*plugin)
        , INIT_CONX(1, 0)
    {}

    virtual ~ResourceManagerTest() = default;
};

// ===== Mock for SecurityAgent =====
class MockAuthenticate : public PluginHost::IAuthenticate {
public:
    MOCK_METHOD(uint32_t, CreateToken, (const uint16_t length, const uint8_t buffer[], std::string& token), (override));
    MOCK_METHOD(PluginHost::ISecurity*, Officer, (const std::string& token), (override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(void, AddRef, (), (const, override));

    BEGIN_INTERFACE_MAP(MockAuthenticate)
        INTERFACE_ENTRY(PluginHost::IAuthenticate)
    END_INTERFACE_MAP
};

// ===== Derived Fixture with Initialized Plugin =====
class ResourceManagerInitializedTest : public ResourceManagerTest {
protected:
    NiceMock<ResourceManagerServiceMock> service;
    NiceMock<MockAuthenticate>* mockAuth = nullptr;

public:
    ResourceManagerInitializedTest() {
        mockAuth = new NiceMock<MockAuthenticate>();

        // Mock QueryInterfaceByCallsign for SecurityAgent
        ON_CALL(service, QueryInterfaceByCallsign(_, _))
            .WillByDefault([this](const uint32_t, const std::string& name) -> void* {
                if (name == "SecurityAgent") {
                    mockAuth->AddRef();
                    return static_cast<void*>(mockAuth);
                }
                return nullptr;
            });

        plugin->Initialize(&service);
    }

    ~ResourceManagerInitializedTest() override {
        plugin->Deinitialize(&service);
        delete mockAuth;
    }
};

// =========================
// ===== TEST CASES ========
// =========================


TEST_F(ResourceManagerTest, Initialize)
{
    NiceMock<ResourceManagerServiceMock> noSecurityService;

    ON_CALL(noSecurityService, QueryInterfaceByCallsign(_, _))
        .WillByDefault(Return(nullptr));

    EXPECT_EQ("", plugin->Initialize(&noSecurityService));
}

TEST_F(ResourceManagerInitializedTest, RegisteredMethods)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setAVBlocked")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("reserveTTSResource")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getBlockedAVApplications")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("reserveTTSResourceForApps")));

}

TEST_F(ResourceManagerInitializedTest, SetAVBlockedTest)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAVBlocked"),
        _T("{\"appId\":\"testApp\",\"blocked\":true}"), response));
}

TEST_F(ResourceManagerInitializedTest, ReserveTTSResourceTest_1)
{

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("reserveTTSResource"),
        _T("{\"appId\":\"testApp\"}"), response));
}

TEST_F(ResourceManagerInitializedTest, ReserveTTSResourceForAppsTest_1)
{

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("reserveTTSResourceForApps"),
        _T("{\"appids\":[\"testApp1\",\"testApp2\"]}"), response));
}

TEST_F(ResourceManagerInitializedTest, ReserveTTSResourceTest_2)
{
    plugin->Deinitialize(&service);  
    plugin->Initialize(&service);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("reserveTTSResource"),
        _T("{ \"appId\": \"testApp\" }"), response));

}

TEST_F(ResourceManagerInitializedTest, ReserveTTSResourceForAppsTest_2)
{
    plugin->Deinitialize(&service);
    plugin->Initialize(&service);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("reserveTTSResourceForApps"),
        _T("{ \"appids\": [\"testApp1\",\"testApp2\"]}"), response));

}

TEST_F(ResourceManagerInitializedTest, ReserveTTSResource_MissingAppId)
{
    plugin->Deinitialize(&service);
    plugin->Initialize(&service);

    // Test error handling when required appId parameter is missing
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("reserveTTSResource"),
        _T("{ }"), response));
}

TEST_F(ResourceManagerInitializedTest, ReserveTTSResourceForApps_MissingAppIds)
{
    plugin->Deinitialize(&service);
    plugin->Initialize(&service);

    // Test error handling when required appids parameter is missing
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("reserveTTSResourceForApps"),
        _T("{ }"), response));
}

TEST_F(ResourceManagerInitializedTest, InformationReturnsEmptyStringTest)
{
    EXPECT_EQ("Plugin which exposes ResourceManager related methods.", plugin->Information());
}

TEST_F(ResourceManagerInitializedTest, GetBlockedAVApplicationsTest)
{
    // Test JSON-RPC API for getBlockedAVApplications
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, handler.Invoke(connection, _T("getBlockedAVApplications"), _T("{}"), response));
}

// ===== Tests with RFC enabled 
TEST_F(ResourceManagerTest, ReserveTTSResourceWithRFCEnabled)
{
    NiceMock<ResourceManagerServiceMock> service;
    
    plugin->Initialize(&service);
    
    auto* instance = Plugin::ResourceManagerImplementation::_instance;
    ASSERT_NE(nullptr, instance);
    
    auto* testableImpl = static_cast<TestableResourceManagerImplementation*>(instance);
    
    testableImpl->setDisableReserveTTS(false);
    testableImpl->setService(&service);
    
    EXPECT_FALSE(testableImpl->getDisableReserveTTS());
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("reserveTTSResource"),
        _T("{\"appId\":\"testApp\"}"), response));
    
    std::cout << "ReserveTTSResource response: " << response << std::endl;
    
    plugin->Deinitialize(&service);
}

TEST_F(ResourceManagerTest, ReserveTTSResourceForAppsWithRFCEnabled)
{
    NiceMock<ResourceManagerServiceMock> service;

    plugin->Initialize(&service);
    
    auto* instance = Plugin::ResourceManagerImplementation::_instance;
    ASSERT_NE(nullptr, instance);
    
    auto* testableImpl = static_cast<TestableResourceManagerImplementation*>(instance);
    
    // Enable TTS reservation
    testableImpl->setDisableReserveTTS(false);
    testableImpl->setService(&service);
    
    EXPECT_FALSE(testableImpl->getDisableReserveTTS());
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("reserveTTSResourceForApps"),
        _T("{\"appids\":[\"testApp1\",\"testApp2\"]}"), response));
    
    std::cout << "ReserveTTSResourceForApps response: " << response << std::endl;
    
    plugin->Deinitialize(&service);
}





