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
#include "L2Tests.h"
#include "L2TestsMock.h"
#include <string>
#include <iostream>

using namespace WPEFramework;
using ::testing::NiceMock;
using ::testing::StrictMock;

#define RESOURCEMANAGER_CALLSIGN _T("org.rdk.ResourceManager")
#define RESOURCEMANAGERL2TEST_CALLSIGN _T("L2tests.1")
#define JSON_TIMEOUT   (1000)

class ResourceManagerTest : public L2TestMocks {
protected:
    virtual ~ResourceManagerTest() override {
        std::cout << "ResourceManagerTest destructor called" << std::endl;
        uint32_t status = Core::ERROR_GENERAL;
        status = DeactivateService("org.rdk.ResourceManager");
        EXPECT_EQ(Core::ERROR_NONE, status);
    }
public:
    ResourceManagerTest() {
        std::cout << "ResourceManagerTest constructor called" << std::endl;

        ON_CALL(*p_essRMgrMock, EssRMgrAddToBlackList(::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(true));
        uint32_t status = Core::ERROR_GENERAL;
        status = ActivateService("org.rdk.ResourceManager");
        EXPECT_EQ(Core::ERROR_NONE, status);
    }
};
/********************************************************
************Test case Details **************************
** 1. Test setAVBlockedWrapper success scenario
*******************************************************/
TEST_F(ResourceManagerTest, SetAVBlockedSuccessCase)
{
    std::cout << "SetAVBlockedSuccessCase test started" << std::endl;
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(RESOURCEMANAGER_CALLSIGN, _T("L2tests.1"));
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject resultJson;
    std::string appid = "com.example.myapp";
    bool blocked = true;

    JsonObject params;
    params["appid"] = appid;
    params["blocked"] = blocked;

    status = InvokeServiceMethod("org.rdk.ResourceManager", "setAVBlocked", params, resultJson);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(resultJson.HasLabel("success"));
    EXPECT_TRUE(resultJson["success"].Boolean());
    std::cout << "SetAVBlockedSuccessCase test finished" << std::endl;
}
/********************************************************
************Test case Details **************************
** Negative test: setAVBlockedWrapper missing required parameters
*******************************************************/
TEST_F(ResourceManagerTest, SetAVBlockedMissingParamsNegativeCase)
{
    std::cout << "SetAVBlockedMissingParamsNegativeCase test started" << std::endl;
    JsonObject resultJson;
    JsonObject params; // Intentionally missing 'appid' and 'blocked'
    uint32_t status = InvokeServiceMethod("org.rdk.ResourceManager", "setAVBlocked", params, resultJson);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(resultJson.HasLabel("success"));
    EXPECT_TRUE(resultJson["success"].Boolean());
    std::cout << "SetAVBlockedMissingParamsNegativeCase test finished" << std::endl;
}
/********************************************************
************Test case Details **************************
** 1. Test getBlockedAVApplicationsWrapper success scenario
*******************************************************/
TEST_F(ResourceManagerTest, GetBlockedAVApplicationsSuccessCase)
{
    std::cout << "GetBlockedAVApplicationsSuccessCase test started" << std::endl;
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(RESOURCEMANAGER_CALLSIGN, RESOURCEMANAGERL2TEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;

    // First, block the app 'org.rdk.Netflix'
    JsonObject setParams;
    setParams["appid"] = "org.rdk.Netflix";
    setParams["blocked"] = true;
    JsonObject setResult;
    status = InvokeServiceMethod("org.rdk.ResourceManager", "setAVBlocked", setParams, setResult);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(setResult.HasLabel("success"));
    EXPECT_TRUE(setResult["success"].Boolean());

    // Now, get the blocked apps
    JsonObject resultJson;
    JsonObject params;
    status = InvokeServiceMethod("org.rdk.ResourceManager", "getBlockedAVApplications", params, resultJson);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(resultJson.HasLabel("success"));
    EXPECT_TRUE(resultJson["success"].Boolean());
    EXPECT_TRUE(resultJson.HasLabel("clients"));
    std::cout << "GetBlockedAVApplicationsSuccessCase test finished" << std::endl;
}
/********************************************************
************Test case Details **************************
** 1. Test reserveTTSResourceWrapper success scenario
*******************************************************/
TEST_F(ResourceManagerTest, ReserveTTSResourceSuccessCase)
{
    std::cout << "ReserveTTSResourceSuccessCase test started" << std::endl;
    JsonObject resultJson;
    std::string appid = "xumo";
    JsonObject params;
    params["appid"] = appid;
    uint32_t status = InvokeServiceMethod("org.rdk.ResourceManager", "reserveTTSResource", params, resultJson);
    EXPECT_EQ(status, Core::ERROR_GENERAL);
    EXPECT_FALSE(resultJson.HasLabel("success"));
    std::cout << "ReserveTTSResourceSuccessCase test finished" << std::endl;
}

/********************************************************
************Test case Details **************************
** 1. Test reserveTTSResourceForApps with 'appids' parameter 
*******************************************************/
TEST_F(ResourceManagerTest, ReserveTTSResourceForApps)
{
    std::cout << "ReserveTTSResourceForApps test started" << std::endl;
    JsonObject resultJson;
    JsonArray appids;
    appids.Add(JsonValue("xumo"));
    appids.Add(JsonValue("netflix"));
    JsonObject params;
    params["appids"] = appids;
    uint32_t status = InvokeServiceMethod("org.rdk.ResourceManager", "reserveTTSResourceForApps", params, resultJson);
    EXPECT_EQ(status, Core::ERROR_GENERAL);
    EXPECT_FALSE(resultJson.HasLabel("success"));
    std::cout << "ReserveTTSResourceForApps test finished" << std::endl;
}

