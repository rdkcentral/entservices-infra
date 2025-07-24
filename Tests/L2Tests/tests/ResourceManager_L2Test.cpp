
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
        // Ensure ERM mock returns true for AddToBlackList
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
** 1. Test reserveTTSResourceWrapper missing appid, RFC disabled branch
*******************************************************/
TEST_F(ResourceManagerTest, ReserveTTSResource_MissingAppId_RFCDisabled)
{
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject resultJson;
    JsonObject params; // missing 'appid'
    status = InvokeServiceMethod("org.rdk.ResourceManager", "reserveTTSResource", params, resultJson);
    EXPECT_EQ(status, Core::ERROR_GENERAL);
}
/********************************************************
************Test case Details **************************
** 1. Test reserveTTSResourceForApps failure scenario
*******************************************************/
TEST_F(ResourceManagerTest, ReserveTTSResourceForAppsFailureCase)
{
    std::cout << "ReserveTTSResourceForAppsFailureCase test started" << std::endl;
    JsonObject resultJson;
    JsonArray apps;
    apps.Add(JsonValue("xumo"));
    apps.Add(JsonValue("netflix"));
    JsonObject params;
    params["apps"] = apps;
    uint32_t status = InvokeServiceMethod("org.rdk.ResourceManager", "reserveTTSResourceForApps", params, resultJson);
    EXPECT_EQ(status, Core::ERROR_GENERAL);
    EXPECT_FALSE(resultJson.HasLabel("success"));
    std::cout << "ReserveTTSResourceForAppsFailureCase test finished" << std::endl;
}
/********************************************************
************Test case Details **************************
** 1. Test reserveTTSResourceForApps with 'appids' parameter to cover branch
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

