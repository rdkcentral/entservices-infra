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
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <interfaces/IAppManager.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#define SCENESET_CALLSIGN  _T("org.rdk.SceneSet")

using ::testing::NiceMock;
using namespace WPEFramework;
using testing::StrictMock;

class SceneSetTest : public L2TestMocks {
protected:
    virtual ~SceneSetTest() override;

public:
    SceneSetTest();

    uint32_t CreateSceneSetInterfaceObjectUsingComRPCConnection();
    void ReleaseSceneSetInterfaceObjectUsingComRPCConnection();

    private:

    protected:
       PluginHost::IShell *mControllerSceneSet;
       Core::ProxyType<RPC::InvokeServerType<1, 0, 4>> mEngineSceneSet;
       Core::ProxyType<RPC::CommunicatorClient> mClientSceneSet;
};

SceneSetTest:: SceneSetTest():L2TestMocks()
{
    uint32_t status = Core::ERROR_GENERAL;
        
    status = ActivateService(SCENESET_CALLSIGN);
    EXPECT_EQ(Core::ERROR_NONE, status);
}

SceneSetTest::~SceneSetTest()
{
    uint32_t status = Core::ERROR_GENERAL;
    
    status = DeactivateService(SCENESET_CALLSIGN);
    EXPECT_EQ(Core::ERROR_NONE, status);
}

uint32_t SceneSetTest::CreateSceneSetInterfaceObjectUsingComRPCConnection()
{
    uint32_t return_value =  Core::ERROR_GENERAL;

    TEST_LOG("Creating mEngineSceneSet");
    mEngineSceneSet = Core::ProxyType<RPC::InvokeServerType<1, 0, 4>>::Create();
    mClientSceneSet = Core::ProxyType<RPC::CommunicatorClient>::Create(Core::NodeId("/tmp/communicator"), Core::ProxyType<Core::IIPCServer>(mEngineSceneSet));

    TEST_LOG("Creating mEngineSceneSet Announcements");
#if ((THUNDER_VERSION == 2) || ((THUNDER_VERSION == 4) && (THUNDER_VERSION_MINOR == 2)))
    mEngineSceneSet->Announcements(mmClientSceneSet->Announcement());
#endif
    if (!mClientSceneSet.IsValid())
    {
        TEST_LOG("Invalid mClientSceneSet");
    }
    else
    {
        mControllerSceneSet = mClientSceneSet->Open<PluginHost::IShell>(_T(SCENESET_CALLSIGN), ~0, 3000);
        if (mControllerSceneSet)
        {
            return_value = Core::ERROR_NONE;
        }
    }
    return return_value;
}

void SceneSetTest::ReleaseSceneSetInterfaceObjectUsingComRPCConnection()
{
    if (mControllerSceneSet)
    {
        mControllerSceneSet->Release();
        mControllerSceneSet = nullptr;
    }

    mClientSceneSet->Close(RPC::CommunicationTimeOut);
    if (mClientSceneSet.IsValid())
    {
        mClientSceneSet.Release();
    }
    mEngineSceneSet.Release();
}

TEST_F(SceneSetTest, CreateGetAndDeleteSceneSetUsingComRpcSuccess)
{
    uint32_t status = Core::ERROR_GENERAL;
    TEST_LOG("### Test CreateGetAndDeleteSceneSetUsingComRpcSuccess Begin ###");

    status = CreateSceneSetInterfaceObjectUsingComRPCConnection();
    EXPECT_EQ(status,Core::ERROR_NONE);

    if (Core::ERROR_NONE == status)
    {
        ReleaseSceneSetInterfaceObjectUsingComRPCConnection();
    }

    TEST_LOG("### Test CreateGetAndDeleteSceneSetUsingComRpcSuccess End ###");
}
