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
#include <interfaces/IResourceManager.h>

using namespace WPEFramework;
using ::testing::NiceMock;
using ::testing::StrictMock;
using ::WPEFramework::Exchange::IResourceManager;

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);
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

/***********************************************
 * COM-RPC Test Class for ResourceManager
 ***********************************************/
class ResourceManagerComRpcTest : public ResourceManagerTest {
protected:
    ResourceManagerComRpcTest();
    virtual ~ResourceManagerComRpcTest() override;

public:
    uint32_t CreateResourceManagerInterfaceObjectUsingComRPCConnection();

protected:
    /** @brief ProxyType objects for proper cleanup */
    Core::ProxyType<RPC::InvokeServerType<1, 0, 4>> mResourceManagerEngine;
    Core::ProxyType<RPC::CommunicatorClient> mResourceManagerClient;

    /** @brief Pointer to the IShell interface */
    PluginHost::IShell *mControllerResourceManager;

    /** @brief Pointer to the IResourceManager interface */
    Exchange::IResourceManager *mResourceManagerPlugin;
};

/**
 * @brief Constructor for ResourceManager COM-RPC L2 test class
 */
ResourceManagerComRpcTest::ResourceManagerComRpcTest()
    : ResourceManagerTest()
    , mControllerResourceManager(nullptr)
    , mResourceManagerPlugin(nullptr)
{
    TEST_LOG("ResourceManager COM-RPC L2 test constructor");

    if (CreateResourceManagerInterfaceObjectUsingComRPCConnection() != Core::ERROR_NONE)
    {
        TEST_LOG("Failed to create ResourceManager COM-RPC interface");
    }
    else
    {
        EXPECT_TRUE(mControllerResourceManager != nullptr);
        EXPECT_TRUE(mResourceManagerPlugin != nullptr);
        TEST_LOG("ResourceManager COM-RPC interface created successfully");
    }
}

/**
 * @brief Destructor for ResourceManager COM-RPC L2 test class
 */
ResourceManagerComRpcTest::~ResourceManagerComRpcTest()
{
    TEST_LOG("ResourceManager COM-RPC L2 test destructor");

    // Clean up interface objects
    if (mResourceManagerPlugin != nullptr) {
        mResourceManagerPlugin->Release();
        mResourceManagerPlugin = nullptr;
    }

    if (mControllerResourceManager != nullptr) {
        mControllerResourceManager->Release();
        mControllerResourceManager = nullptr;
    }
}

/**
 * @brief Creates ResourceManager interface object using COM-RPC connection
 * @return Core::ERROR_NONE on success, error code otherwise
 */
uint32_t ResourceManagerComRpcTest::CreateResourceManagerInterfaceObjectUsingComRPCConnection()
{
    uint32_t returnValue = Core::ERROR_GENERAL;

    TEST_LOG("Creating ResourceManager COM-RPC connection");

    // Create the ResourceManager engine
    mResourceManagerEngine = Core::ProxyType<RPC::InvokeServerType<1, 0, 4>>::Create();
    mResourceManagerClient = Core::ProxyType<RPC::CommunicatorClient>::Create(
        Core::NodeId("/tmp/communicator"),
        Core::ProxyType<Core::IIPCServer>(mResourceManagerEngine)
    );

    if (!mResourceManagerClient.IsValid())
    {
        TEST_LOG("Invalid ResourceManager Client");
    }
    else
    {
        mControllerResourceManager = mResourceManagerClient->Open<PluginHost::IShell>(_T("org.rdk.ResourceManager"), ~0, 3000);
        if (mControllerResourceManager)
        {
            mResourceManagerPlugin = mControllerResourceManager->QueryInterface<Exchange::IResourceManager>();
            if (mResourceManagerPlugin) {
                returnValue = Core::ERROR_NONE;
                TEST_LOG("ResourceManager COM-RPC interface obtained successfully");
            } else {
                TEST_LOG("Failed to query IResourceManager interface");
            }
        }
        else
        {
            TEST_LOG("Failed to open IShell for ResourceManager");
        }
    }
    return returnValue;
}
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
    params["appId"] = appid;
    params["blocked"] = blocked;

    status = InvokeServiceMethod("org.rdk.ResourceManager", "setAVBlocked", params, resultJson);
    EXPECT_EQ(status, Core::ERROR_NONE);
    ASSERT_TRUE(resultJson.HasLabel("success"));
    EXPECT_TRUE(resultJson["success"].Boolean());
    std::cout << "SetAVBlockedSuccessCase test finished" << std::endl;
}
/********************************************************
************Test case Details **************************
** Negative test: setAVBlockedWrapper missing required parameters
*******************************************************/
TEST_F(ResourceManagerTest, SetAVBlockedMissingParams)
{
    std::cout << "SetAVBlockedMissingParams test started" << std::endl;
    JsonObject resultJson;
    JsonObject params; 
    uint32_t status = InvokeServiceMethod("org.rdk.ResourceManager", "setAVBlocked", params, resultJson);
    // Expect error when required parameters are missing
    EXPECT_NE(status, Core::ERROR_NONE);
    std::cout << "SetAVBlockedMissingParams test finished" << std::endl;
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
    setParams["appId"] = "org.rdk.Netflix";
    setParams["blocked"] = true;
    JsonObject setResult;
    status = InvokeServiceMethod("org.rdk.ResourceManager", "setAVBlocked", setParams, setResult);
    EXPECT_EQ(status, Core::ERROR_NONE);
    ASSERT_TRUE(setResult.HasLabel("success"));
    EXPECT_TRUE(setResult["success"].Boolean());

    // Now, get the blocked apps
    JsonObject resultJson;
    JsonObject params;
    status = InvokeServiceMethod("org.rdk.ResourceManager", "getBlockedAVApplications", params, resultJson);
    EXPECT_EQ(status, Core::ERROR_NONE);
    ASSERT_TRUE(resultJson.HasLabel("success"));
    EXPECT_TRUE(resultJson["success"].Boolean());
    ASSERT_TRUE(resultJson.HasLabel("clients"));
    const JsonArray& clients = resultJson["clients"].Array();
    bool found = false;
    for (size_t i = 0; i < clients.Length(); ++i) {
        if (clients[i].String() == "org.rdk.Netflix") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
    std::cout << "GetBlockedAVApplicationsSuccessCase test finished" << std::endl;
}
/********************************************************
************Test case Details **************************
** 1. Test reserveTTSResourceWrapper 
*******************************************************/
TEST_F(ResourceManagerTest, ReserveTTSResourceTest)
{
    std::cout << "ReserveTTSResourceTest test started" << std::endl;
    JsonObject resultJson;
    std::string appid = "xumo";
    JsonObject params;
    params["appId"] = appid;
    uint32_t status = InvokeServiceMethod("org.rdk.ResourceManager", "reserveTTSResource", params, resultJson);

    // Service returns success but with success=false since TTS service is not available
    EXPECT_EQ(status, Core::ERROR_NONE);
    ASSERT_TRUE(resultJson.HasLabel("success"));
    EXPECT_FALSE(resultJson["success"].Boolean());
    std::cout << "ReserveTTSResourceTest test finished" << std::endl;
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
    // Service returns success but with success=false since TTS service is not available
    EXPECT_EQ(status, Core::ERROR_NONE);
    ASSERT_TRUE(resultJson.HasLabel("success"));
    EXPECT_FALSE(resultJson["success"].Boolean());
    std::cout << "ReserveTTSResourceForApps test finished" << std::endl;
}

/********************************************************************
 * COM-RPC Test Cases
 ********************************************************************/

/********************************************************
************Test case Details **************************
** 1. Test SetAVBlocked using COM-RPC with success case
*******************************************************/
TEST_F(ResourceManagerComRpcTest, SetAVBlockedSuccessCaseUsingComrpc)
{
    TEST_LOG("SetAVBlockedSuccessCaseUsingComrpc test started");
    
    ASSERT_NE(mResourceManagerPlugin, nullptr) << "ResourceManager plugin interface is null";

    Core::hresult status = Core::ERROR_GENERAL;
    Exchange::IResourceManager::Success result;
    string appId = "com.example.myapp";
    bool blocked = true;

    // Call SetAVBlocked via COM-RPC
    status = mResourceManagerPlugin->SetAVBlocked(appId, blocked, result);

    // Verify the result
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result.success);

    if (status != Core::ERROR_NONE)
    {
        std::string errorMsg = "COM-RPC returned error " + std::to_string(status) + 
                               " (" + std::string(Core::ErrorToString(status)) + ")";
        TEST_LOG("Err: %s", errorMsg.c_str());
    }

    TEST_LOG("SetAVBlockedSuccessCaseUsingComrpc test finished");
}

/********************************************************
************Test case Details **************************
** 2. Test GetBlockedAVApplications using COM-RPC
*******************************************************/
TEST_F(ResourceManagerComRpcTest, GetBlockedAVApplicationsSuccessCaseUsingComrpc)
{
    TEST_LOG("GetBlockedAVApplicationsSuccessCaseUsingComrpc test started");
    
    ASSERT_NE(mResourceManagerPlugin, nullptr) << "ResourceManager plugin interface is null";

    Core::hresult status = Core::ERROR_GENERAL;
    Exchange::IResourceManager::Success setResult;
    
    // First, block some applications
    string appId1 = "org.rdk.Netflix";
    string appId2 = "com.example.testapp";
    
    status = mResourceManagerPlugin->SetAVBlocked(appId1, true, setResult);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(setResult.success);
    
    status = mResourceManagerPlugin->SetAVBlocked(appId2, true, setResult);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(setResult.success);

    // Now get the list of blocked applications
    Exchange::IResourceManager::IStringIterator* clients = nullptr;
    bool success = false;
    
    status = mResourceManagerPlugin->GetBlockedAVApplications(clients, success);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(success);
    ASSERT_NE(clients, nullptr) << "IStringIterator is null";

    // Iterate through the blocked applications list
    bool foundNetflix = false;
    bool foundTestApp = false;
    
    if (clients != nullptr)
    {
        string appId;
        while (clients->Next(appId) == true)
        {
            TEST_LOG("Blocked app found: %s", appId.c_str());
            if (appId == "org.rdk.Netflix") {
                foundNetflix = true;
            }
            if (appId == "com.example.testapp") {
                foundTestApp = true;
            }
        }
        
        EXPECT_TRUE(foundNetflix) << "Netflix should be in blocked list";
        EXPECT_TRUE(foundTestApp) << "testapp should be in blocked list";
        
        clients->Release();
    }

    TEST_LOG("GetBlockedAVApplicationsSuccessCaseUsingComrpc test finished");
}

/********************************************************
************Test case Details **************************
** 3. Test ReserveTTSResource using COM-RPC
*******************************************************/
TEST_F(ResourceManagerComRpcTest, ReserveTTSResourceTestUsingComrpc)
{
    TEST_LOG("ReserveTTSResourceTestUsingComrpc test started");
    
    ASSERT_NE(mResourceManagerPlugin, nullptr) << "ResourceManager plugin interface is null";

    Core::hresult status = Core::ERROR_GENERAL;
    Exchange::IResourceManager::Success result;
    string appId = "xumo";

    // Call ReserveTTSResource via COM-RPC
    status = mResourceManagerPlugin->ReserveTTSResource(appId, result);

    // Verify the result
    EXPECT_EQ(status, Core::ERROR_NONE);
    // Service returns success but with success=false since TTS service is not available
    EXPECT_FALSE(result.success) << "TTS service should not be available in test environment";

    TEST_LOG("ReserveTTSResourceTestUsingComrpc test finished");
}

/********************************************************
************Test case Details **************************
** 4. Test ReserveTTSResourceForApps using COM-RPC
*******************************************************/
TEST_F(ResourceManagerComRpcTest, ReserveTTSResourceForAppsUsingComrpc)
{
    TEST_LOG("ReserveTTSResourceForAppsUsingComrpc test started");
    
    ASSERT_NE(mResourceManagerPlugin, nullptr) << "ResourceManager plugin interface is null";

    Core::hresult status = Core::ERROR_GENERAL;
    Exchange::IResourceManager::Success result;
    
    // Create a vector of app IDs
    std::vector<string> appIdList;
    appIdList.push_back("xumo");
    appIdList.push_back("netflix");

    // Create an IStringIterator to pass the app IDs
    // Using RPC::IteratorType implementation
    using StringIterator = RPC::IteratorType<Exchange::IResourceManager::IStringIterator>;
    Exchange::IResourceManager::IStringIterator* appidsIterator = 
        Core::Service<StringIterator>::Create<Exchange::IResourceManager::IStringIterator>(appIdList);

    ASSERT_NE(appidsIterator, nullptr) << "Failed to create IStringIterator";

    // Call ReserveTTSResourceForApps via COM-RPC
    status = mResourceManagerPlugin->ReserveTTSResourceForApps(appidsIterator, result);

    // Verify the result
    EXPECT_EQ(status, Core::ERROR_NONE);
    // Service returns success but with success=false since TTS service is not available
    EXPECT_FALSE(result.success) << "TTS service should not be available in test environment";

    // Clean up
    if (appidsIterator != nullptr) {
        appidsIterator->Release();
    }

    TEST_LOG("ReserveTTSResourceForAppsUsingComrpc test finished");
}

