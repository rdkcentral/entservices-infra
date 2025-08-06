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
#include <interfaces/ITelemetry.h>

#define JSON_TIMEOUT   (1000)
#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);
#define TELEMETRY_CALLSIGN  _T("org.rdk.Telemetry.1")
#define TELEMETRYL2TEST_CALLSIGN _T("L2tests.1")

using ::testing::NiceMock;
using namespace WPEFramework;
using testing::StrictMock;
using ::WPEFramework::Exchange::ITelemetry;

typedef enum : uint32_t {
    Telemetry_OnReportUpload = 0x00000001,
    Telemetry_StateInvalid = 0x00000000
}TelemetryL2test_async_events_t;

class AsyncHandlerMock_Telemetry
{
    public:
        AsyncHandlerMock_Telemetry()
        {
        }
        MOCK_METHOD(void, OnReportUpload, (const string& telemetryUploadStatus));
};

/* Notification Handler Class for COM-RPC*/
class TelemetryNotificationHandler : public Exchange::ITelemetry::INotification {
    private:
        /** @brief Mutex */
        std::mutex m_mutex;

        /** @brief Condition variable */
        std::condition_variable m_condition_variable;

        /** @brief Event signalled flag */
        uint32_t m_event_signalled;

        BEGIN_INTERFACE_MAP(Notification)
        INTERFACE_ENTRY(Exchange::ITelemetry::INotification)
        END_INTERFACE_MAP

    public:
        TelemetryNotificationHandler(){}
        ~TelemetryNotificationHandler(){}

    void OnReportUpload(const string& telemetryUploadStatus)
    {
        TEST_LOG("onReportUpload event triggered ***\n");
        std::unique_lock<std::mutex> lock(m_mutex);

        TEST_LOG("onReportUpload received: %s\n", telemetryUploadStatus.c_str());
        /* Notify the requester thread. */
        m_event_signalled |= Telemetry_OnReportUpload;
        m_condition_variable.notify_one();
    }

    uint32_t WaitForRequestStatus(uint32_t timeout_ms,TelemetryL2test_async_events_t expected_status)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        auto now = std::chrono::system_clock::now();
        std::chrono::milliseconds timeout(timeout_ms);
        uint32_t signalled = Telemetry_StateInvalid;

        while (!(expected_status & m_event_signalled))
        {
            if (m_condition_variable.wait_until(lock, now + timeout) == std::cv_status::timeout)
            {
                TEST_LOG("Timeout waiting for request status event");
                break;
            }
        }

        signalled = m_event_signalled;
        return signalled;
    }
  };

/* Telemetry L2 test class declaration */
class Telemetry_L2test : public L2TestMocks {
protected:
    IARM_EventHandler_t powerEventHandler = nullptr;	

    Telemetry_L2test();
    virtual ~Telemetry_L2test() override;

    void TearDown() ;
    public:
        void OnReportUpload(const string &telemetryUploadStatus);

        /**
         * @brief waits for various status change on asynchronous calls
        */
        uint32_t WaitForRequestStatus(uint32_t timeout_ms,TelemetryL2test_async_events_t expected_status);
        uint32_t CreateTelemetryInterfaceObjectUsingComRPCConnection();

    private:
        /** @brief Mutex */
        std::mutex m_mutex;

        /** @brief Condition variable */
        std::condition_variable m_condition_variable;

        /** @brief Event signalled flag */
        uint32_t m_event_signalled;

    protected:
        /** @brief Pointer to the IShell interface */
        PluginHost::IShell *m_controller_telemetry;

        /** @brief Pointer to the ITelemetry interface */
        Exchange::ITelemetry *m_telemetryplugin;

        Core::Sink<TelemetryNotificationHandler> notify;
	
};

/**
 * @brief Constructor for Telemetry L2 test class
 */
Telemetry_L2test::Telemetry_L2test()
        : L2TestMocks()
{
    Core::hresult status = Core::ERROR_GENERAL;
    m_event_signalled = Telemetry_StateInvalid;

    ON_CALL(*p_iarmBusImplMock, IARM_Bus_RegisterEventHandler(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [&](const char* ownerName, IARM_EventId_t eventId, IARM_EventHandler_t handler) {
                if ((string(IARM_BUS_PWRMGR_NAME) == string(ownerName)) && (eventId == IARM_BUS_PWRMGR_EVENT_MODECHANGED)) {
                    EXPECT_TRUE(handler != nullptr);
                    powerEventHandler = handler;
                }
                return IARM_RESULT_SUCCESS;
            }));
	
    EXPECT_CALL(mfrMock::Mock(), mfrGetTemperature(::testing::_, ::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Invoke(
        [&](mfrTemperatureState_t* curState, int* curTemperature, int* wifiTemperature) {
            *curTemperature  = 90; // safe temperature
            *curState        = (mfrTemperatureState_t)0;
            *wifiTemperature = 25;
            return mfrERR_NONE;
    }));

    /* Activate plugin in constructor */
    status = ActivateService("org.rdk.PowerManager");
    EXPECT_EQ(Core::ERROR_NONE, status);

    status = ActivateService("org.rdk.Telemetry");
    EXPECT_EQ(Core::ERROR_NONE, status);

    if (CreateTelemetryInterfaceObjectUsingComRPCConnection() != Core::ERROR_NONE) 
    {
        TEST_LOG("Invalid Telemetry_Client");
    } 
    else 
    {
        EXPECT_TRUE(m_controller_telemetry != nullptr);
        if (m_controller_telemetry) 
	{
            EXPECT_TRUE(m_telemetryplugin != nullptr);
            if (m_telemetryplugin) {
		m_telemetryplugin->AddRef();    
                m_telemetryplugin->Register(&notify);
            } else {
                    TEST_LOG("m_telemetryplugin is NULL");
            }
         } else {
             TEST_LOG("m_controller_telemetry is NULL");
        }
    }
}

/**
 * @brief Destructor for Telemetry L2 test class
 */
Telemetry_L2test::~Telemetry_L2test()
{
    Core::hresult status = Core::ERROR_GENERAL;
    m_event_signalled = Telemetry_StateInvalid;

    if (m_telemetryplugin) {
        m_telemetryplugin->Unregister(&notify);
        m_telemetryplugin->Release();
    }

    status = DeactivateService("org.rdk.Telemetry");
    EXPECT_EQ(Core::ERROR_NONE, status);

}


void Telemetry_L2test::TearDown()
{
    Core::hresult status = DeactivateService("org.rdk.System");
    TEST_LOG("after deactivate system service and before equality check");
    EXPECT_EQ(Core::ERROR_NONE, status);
}

/**
 *
 * @param[in] message from Telemetry on the change
 */
void Telemetry_L2test::OnReportUpload(const string &telemetryUploadStatus)
{
    TEST_LOG("onReportUpload event triggered ***\n");
    std::unique_lock<std::mutex> lock(m_mutex);

    std::string str= telemetryUploadStatus;

    TEST_LOG("onReportUpload received: %s\n", str.c_str());

    /* Notify the requester thread. */
    m_event_signalled |= Telemetry_OnReportUpload;
    m_condition_variable.notify_one();
}

/**
 * @brief waits for various status change on asynchronous calls
 *
 * @param[in] timeout_ms timeout for waiting
 */
uint32_t Telemetry_L2test::WaitForRequestStatus(uint32_t timeout_ms,TelemetryL2test_async_events_t expected_status)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    auto now = std::chrono::system_clock::now();
    std::chrono::milliseconds timeout(timeout_ms);
    uint32_t signalled = Telemetry_StateInvalid;

    while (!(expected_status & m_event_signalled))
    {
        if (m_condition_variable.wait_until(lock, now + timeout) == std::cv_status::timeout)
        {
            TEST_LOG("Timeout waiting for request status event");
            break;
        }
    }

    signalled = m_event_signalled;

    return signalled;
}


/**
 * @brief Compare two request status objects
 *
 * @param[in] data Expected value
 * @return true if the argument and data match, false otherwise
 */
MATCHER_P(MatchRequestStatus, data, "")
{
    bool match = true;
    std::string expected;
    std::string actual;

    data.ToString(expected);
    arg.ToString(actual);
    TEST_LOG(" rec = %s, arg = %s",expected.c_str(),actual.c_str());
    EXPECT_STREQ(expected.c_str(),actual.c_str());

    return match;
}

//COM-RPC Changes
uint32_t Telemetry_L2test::CreateTelemetryInterfaceObjectUsingComRPCConnection()
{
    Core::hresult return_value =  Core::ERROR_GENERAL;
    Core::ProxyType<RPC::InvokeServerType<1, 0, 4>> Engine_Telemetry;
    Core::ProxyType<RPC::CommunicatorClient> Client_Telemetry;

    TEST_LOG("Creating Engine_Telemetry");
    Engine_Telemetry = Core::ProxyType<RPC::InvokeServerType<1, 0, 4>>::Create();
    Client_Telemetry = Core::ProxyType<RPC::CommunicatorClient>::Create(Core::NodeId("/tmp/communicator"), Core::ProxyType<Core::IIPCServer>(Engine_Telemetry));

    TEST_LOG("Creating Engine_Telemetry Announcements");
#if ((THUNDER_VERSION == 2) || ((THUNDER_VERSION == 4) && (THUNDER_VERSION_MINOR == 2)))
    Engine_Telemetry->Announcements(mClient_Telemetry->Announcement());
#endif
    if (!Client_Telemetry.IsValid())
    {
        TEST_LOG("Invalid Client_Telemetry");
    }
    else
    {
        m_controller_telemetry = Client_Telemetry->Open<PluginHost::IShell>(_T("org.rdk.Telemetry"), ~0, 3000);
        if (m_controller_telemetry)
        {
        m_telemetryplugin = m_controller_telemetry->QueryInterface<Exchange::ITelemetry>();
        return_value = Core::ERROR_NONE;
        }
    }
    return return_value;
}

/************Test case Details **************************
** 1.LogApplicationEvent with eventName and eventValue for success case.
*******************************************************/

TEST_F(Telemetry_L2test, LogApplicationEventUsingComrpc)
{
    Core::hresult status = Core::ERROR_GENERAL;

    string eventName=".....",eventValue=".....";
    status = m_telemetryplugin->LogApplicationEvent(eventName,eventValue);
    EXPECT_EQ(status,Core::ERROR_NONE);
    if (status != Core::ERROR_NONE)
    {
        std::string errorMsg = "COM-RPC returned error " + std::to_string(status) + " (" + std::string(Core::ErrorToString(status)) + ")";
        TEST_LOG("Err: %s", errorMsg.c_str());
    }  
}

/************Test case Details **************************
** 1.LogApplicationEvent with empty eventName and eventValue for failure case.
*******************************************************/

TEST_F(Telemetry_L2test, LogApplicationEventFailureUsingComrpc)
{
    Core::hresult status = Core::ERROR_GENERAL;

    string eventName="",eventValue="";
    status = m_telemetryplugin->LogApplicationEvent(eventName,eventValue);
    EXPECT_EQ(status,Core::ERROR_GENERAL);
    if (status != Core::ERROR_GENERAL)
    {
        std::string errorMsg = "COM-RPC returned error " + std::to_string(status) + " (" + std::string(Core::ErrorToString(status)) + ")";
        TEST_LOG("Err: %s", errorMsg.c_str());
    }
}

/************Test case Details **************************
** 1.LogApplicationEvent with eventName and eventValue for success using Jsonrpc.
*******************************************************/

TEST_F(Telemetry_L2test, TelemetrylogApplicationEventUsingJsonrpc)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(TELEMETRY_CALLSIGN, TELEMETRYL2TEST_CALLSIGN);
    StrictMock<AsyncHandlerMock_Telemetry> async_handler;
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;
    JsonObject expected_status;

    /*With both Params expecting Success*/
    params["eventName"] = ".....";
    params["eventValue"] = ".....";
    status = InvokeServiceMethod("org.rdk.Telemetry.1", "logApplicationEvent", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_STREQ("null", result["value"].String().c_str());
}

/************Test case Details **************************
** 1.LogApplicationEvent with empty eventName and eventValue for failure case using Jsonrpc.
*******************************************************/

TEST_F(Telemetry_L2test, TelemetrylogApplicationFailureEventUsingJsonrpc)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(TELEMETRY_CALLSIGN, TELEMETRYL2TEST_CALLSIGN);
    StrictMock<AsyncHandlerMock_Telemetry> async_handler;
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;
    JsonObject expected_status;

    /*With one Param  expecting Fail case */
    params["eventName"] = "";
    params["eventValue"] = "";
    status = InvokeServiceMethod("org.rdk.Telemetry.1", "logApplicationEvent", params, result);
    EXPECT_EQ(Core::ERROR_GENERAL, status);
    EXPECT_STREQ("null", result["value"].String().c_str());
}

/************Test case Details **************************
** 1.UploadReport with Upload status as SUCCESS.
*******************************************************/

TEST_F(Telemetry_L2test, UploadReportUsingComrpc)
{
    Core::hresult status = Core::ERROR_GENERAL;
    uint32_t signalled = Telemetry_StateInvalid;
    struct _rbusObject rbObject;
    struct _rbusValue rbValue;

    ON_CALL(*p_rBusApiImplMock, rbus_open(::testing::_, ::testing::_))
        .WillByDefault(
            ::testing::Return(RBUS_ERROR_SUCCESS));

    ON_CALL(*p_rBusApiImplMock, rbusValue_GetString(::testing::_, ::testing::_))
        .WillByDefault(
            ::testing::Return( "SUCCESS"));

    ON_CALL(*p_rBusApiImplMock, rbusObject_GetValue(::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [&](rbusObject_t object, char const* name) {
                EXPECT_EQ(object, &rbObject);
                EXPECT_EQ(string(name), _T("UPLOAD_STATUS"));
                return &rbValue;
            }));
                
    ON_CALL(*p_rBusApiImplMock, rbusMethod_InvokeAsync(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
         .WillByDefault(::testing::Invoke(
            [&](rbusHandle_t handle, char const* methodName, rbusObject_t inParams, rbusMethodAsyncRespHandler_t callback,  int timeout) {
                callback(handle, methodName, RBUS_ERROR_SUCCESS, &rbObject);
                return RBUS_ERROR_SUCCESS;
	    }));
                
    status = m_telemetryplugin->UploadReport();
    EXPECT_EQ(status,Core::ERROR_NONE);
    if (status != Core::ERROR_NONE)
    {
        std::string errorMsg = "COM-RPC returned error " + std::to_string(status) + " (" + std::string(Core::ErrorToString(status)) + ")";
        TEST_LOG("Err: %s", errorMsg.c_str());
    }
    signalled = notify.WaitForRequestStatus(JSON_TIMEOUT,Telemetry_OnReportUpload);
    EXPECT_TRUE(signalled & Telemetry_OnReportUpload);
}

/************Test case Details **************************
** 1.UploadReport with RBUS Error for failure case. 
*******************************************************/

TEST_F(Telemetry_L2test, UploadReportFailureCaseUsingComrpc)
{
    Core::hresult status = Core::ERROR_GENERAL;
    uint32_t signalled = Telemetry_StateInvalid;
    struct _rbusObject rbObject;
    struct _rbusValue rbValue;

    //On failure case for covering else case in code//
    ON_CALL(*p_rBusApiImplMock, rbusMethod_InvokeAsync(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [&](rbusHandle_t handle, char const* methodName, rbusObject_t inParams, rbusMethodAsyncRespHandler_t callback,  int timeout) {
                callback(handle, methodName, RBUS_ERROR_BUS_ERROR, &rbObject);
                return RBUS_ERROR_BUS_ERROR;
	    }));

    status = m_telemetryplugin->UploadReport();
    EXPECT_EQ(status,Core::ERROR_RPC_CALL_FAILED);
    if (status != Core::ERROR_RPC_CALL_FAILED)
    {
        std::string errorMsg = "COM-RPC returned error " + std::to_string(status) + " (" + std::string(Core::ErrorToString(status)) + ")";
        TEST_LOG("Err: %s", errorMsg.c_str());
    }

    /* UploadReport is called by powerEventHandler internally in the below case */
    IARM_Bus_PWRMgr_EventData_t param;
    param.data.state.curState = IARM_BUS_PWRMGR_POWERSTATE_ON;
    param.data.state.newState = IARM_BUS_PWRMGR_POWERSTATE_STANDBY_LIGHT_SLEEP;
    powerEventHandler(IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_EVENT_MODECHANGED, &param, 0);	
}

/************Test case Details **************************
** 1.AbortReport with RBus Success case using Jsonrpc.
*******************************************************/

TEST_F(Telemetry_L2test, TelemetryAbortReportUsingJsonrpc)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(TELEMETRY_CALLSIGN, TELEMETRYL2TEST_CALLSIGN);
    StrictMock<AsyncHandlerMock_Telemetry> async_handler;
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;
    std::string message;
    JsonObject expected_status;
    struct _rbusObject rbObject;

    status = InvokeServiceMethod("org.rdk.Telemetry.1", "abortReport", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_STREQ("null", result["value"].String().c_str());

    ON_CALL(*p_rBusApiImplMock, rbus_open(::testing::_, ::testing::_))
        .WillByDefault(
            ::testing::Return(RBUS_ERROR_SUCCESS));

    ON_CALL(*p_rBusApiImplMock, rbusMethod_InvokeAsync(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [&](rbusHandle_t handle, char const* methodName, rbusObject_t inParams, rbusMethodAsyncRespHandler_t callback, int timeout) {
                callback(handle, methodName, RBUS_ERROR_SUCCESS, &rbObject);
                return RBUS_ERROR_SUCCESS;
            }));

    status = InvokeServiceMethod("org.rdk.Telemetry.1", "abortReport", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_STREQ("null", result["value"].String().c_str());

}

/************Test case Details **************************
** 1.AbortReport with RBus Error case using Jsonrpc.
*******************************************************/

TEST_F(Telemetry_L2test, TelemetryAbortReportFailureCaseUsingJsonrpc)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(TELEMETRY_CALLSIGN, TELEMETRYL2TEST_CALLSIGN);
    StrictMock<AsyncHandlerMock_Telemetry> async_handler;
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;

    /**Called rbusMethod_InvokeAsync Twice, where once it returns "RBUS_ERROR_SUCCESS" for IARM ModeChanged
       & again returns "RBUS_ERROR_BUS_ERROR" for "ERROR_RPC_CALL_FAILED" Errorcheck**/

    EXPECT_CALL(*p_rBusApiImplMock, rbusMethod_InvokeAsync(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
	.Times(2)
        .WillOnce(::testing::Invoke(
            [&](rbusHandle_t handle, char const* methodName, rbusObject_t inParams, rbusMethodAsyncRespHandler_t callback, int timeout) {
                return RBUS_ERROR_BUS_ERROR;
            }))     
        .WillOnce(::testing::Invoke(
            [&](rbusHandle_t handle, char const* methodName, rbusObject_t inParams, rbusMethodAsyncRespHandler_t callback, int timeout) {
                return RBUS_ERROR_BUS_ERROR;
            }));


    /* "ERROR_RPC_CALL_FAILED" -- ErrorCheck */
    status = InvokeServiceMethod("org.rdk.Telemetry.1", "abortReport", params, result);
    EXPECT_EQ(Core::ERROR_RPC_CALL_FAILED, status);

    /* abortReport is called by powerEventHandler internally in the below case */
    IARM_Bus_PWRMgr_EventData_t param;
    param.data.state.curState = IARM_BUS_PWRMGR_POWERSTATE_ON;
    param.data.state.newState = IARM_BUS_PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP;
    powerEventHandler(IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_EVENT_MODECHANGED, &param, 0);	
}

/************Test case Details **************************
** 1.AbortReport with RBus Success case using Comrpc.
*******************************************************/

TEST_F(Telemetry_L2test, AbortReportUsingComrpc)
{
    Core::hresult status = Core::ERROR_GENERAL;
    struct _rbusObject rbObject;
    uint32_t signalled = Telemetry_StateInvalid;    
            
    ON_CALL(*p_rBusApiImplMock, rbus_open(::testing::_, ::testing::_))
        .WillByDefault(
            ::testing::Return(RBUS_ERROR_SUCCESS));

    ON_CALL(*p_rBusApiImplMock, rbusMethod_InvokeAsync(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [&](rbusHandle_t handle, char const* methodName, rbusObject_t inParams, rbusMethodAsyncRespHandler_t callback,  int timeout) {
                callback(handle, methodName, RBUS_ERROR_SUCCESS, &rbObject);
                return RBUS_ERROR_SUCCESS;
            }));
    status = m_telemetryplugin->AbortReport();
    EXPECT_EQ(status,Core::ERROR_NONE);
    if (status != Core::ERROR_NONE)
    {
        std::string errorMsg = "COM-RPC returned error " + std::to_string(status) + " (" + std::string(Core::ErrorToString(status)) + ")";
        TEST_LOG("Err: %s", errorMsg.c_str());
    }
}

/************Test case Details **************************
** 1.AbortReport with RBus Error case using Comrpc.
*******************************************************/

TEST_F(Telemetry_L2test, AbortReportFailurecaseUsingComrpc)
{
    Core::hresult status = Core::ERROR_GENERAL;
    struct _rbusObject rbObject;
    uint32_t signalled = Telemetry_StateInvalid;

    //On failure case for covering else case in code//
    ON_CALL(*p_rBusApiImplMock, rbusMethod_InvokeAsync(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [&](rbusHandle_t handle, char const* methodName, rbusObject_t inParams, rbusMethodAsyncRespHandler_t callback,  int timeout) {
                callback(handle, methodName, RBUS_ERROR_BUS_ERROR, &rbObject);
                return RBUS_ERROR_BUS_ERROR;
            }));

    status = m_telemetryplugin->AbortReport();
    EXPECT_EQ(status,Core::ERROR_RPC_CALL_FAILED);
    if (status != Core::ERROR_RPC_CALL_FAILED)
    {
        std::string errorMsg = "COM-RPC returned error " + std::to_string(status) + " (" + std::string(Core::ErrorToString(status)) + ")";
        TEST_LOG("Err: %s", errorMsg.c_str());
    }

    /* AbortReport is called by powerEventHandler internally in the below case */
    IARM_Bus_PWRMgr_EventData_t param;
    param.data.state.curState = IARM_BUS_PWRMGR_POWERSTATE_ON;
    param.data.state.newState = IARM_BUS_PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP;
    powerEventHandler(IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_EVENT_MODECHANGED, &param, sizeof(param));	

}

/************Test case Details **************************
** 1.Mocking default.json file creation.
*******************************************************/

TEST_F(Telemetry_L2test, ValidateDefaultProfilesFileUsingComrpc)
{
    struct stat buffer;
    if (stat("/etc/t2profiles/default.json", &buffer) != 0) {
        // File doesn't exist, create it
        if (mkdir("/opt/.t2reportprofiles", 0755) == -1 && errno != EEXIST) {
            TEST_LOG("Failed to create directory /opt/.t2reportprofiles: %s", strerror(errno));
            return;
        }
        if (mkdir("/etc/t2profiles", 0755) == -1 && errno != EEXIST) {
            TEST_LOG("Failed to create directory /etc/t2profiles: %s", strerror(errno));
            return;
	}
        std::ofstream jsonFile("/etc/t2profiles/default.json");
        ASSERT_TRUE(jsonFile.is_open());
        jsonFile << "{\n";
        jsonFile << "    \"profile\": \"default\",\n";
        jsonFile << "    \"version\": \"1.0\",\n";
        jsonFile << "    \"markers\": [\n";
        jsonFile << "        {\n";
        jsonFile << "            \"name\": \"TEST_MARKER\",\n";
        jsonFile << "            \"interval\": 3600,\n";
        jsonFile << "            \"threshold\": 100\n";
        jsonFile << "        }\n";
        jsonFile << "    ]\n";
        jsonFile << "}\n";
        jsonFile.close();
        std::ifstream verifyFile("/etc/t2profiles/default.json");
        EXPECT_TRUE(verifyFile.good());
        verifyFile.close();
     }
}

/************Test case Details **************************
** 1.Validate uploadReport with no 'UPLOAD_STATUS' using Jsonrpc.
*******************************************************/

TEST_F(Telemetry_L2test, TelemetryReportUploadErrorCheckUsingJsonrpc)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(TELEMETRY_CALLSIGN, TELEMETRYL2TEST_CALLSIGN);
    StrictMock<AsyncHandlerMock_Telemetry> async_handler;
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;
    std::string message;
    JsonObject expected_status;
    struct _rbusObject rbObject;

    ON_CALL(*p_rBusApiImplMock, rbus_open(::testing::_, ::testing::_))
        .WillByDefault(
            ::testing::Return(RBUS_ERROR_SUCCESS));

    ON_CALL(*p_rBusApiImplMock, rbusValue_GetString(::testing::_, ::testing::_))
        .WillByDefault(
            ::testing::Return("SUCCESS"));

    /**Called rbusObject_GetValue where it returns nullptr to verify "No 'UPLOAD_STATUS' value" for onReportUploadStatus **/
    EXPECT_CALL(*p_rBusApiImplMock, rbusObject_GetValue(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](rbusObject_t object, char const* name) {
                EXPECT_EQ(object, &rbObject);
                EXPECT_EQ(string(name), _T("UPLOAD_STATUS"));
                return nullptr;
            }));

    ON_CALL(*p_rBusApiImplMock, rbusMethod_InvokeAsync(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [&](rbusHandle_t handle, char const* methodName, rbusObject_t inParams, rbusMethodAsyncRespHandler_t callback, int timeout) {
                callback(handle, methodName, RBUS_ERROR_SUCCESS, &rbObject);
                return RBUS_ERROR_SUCCESS;
            }));

    status = InvokeServiceMethod("org.rdk.Telemetry.1", "uploadReport", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
}

/************Test case Details **************************
** 1.Validate uploadReport with no 'UPLOAD_STATUS' using Comrpc.
*******************************************************/

TEST_F(Telemetry_L2test, OnReportUploadStatusWithNoUploadStatusUsingComrpc)
{
    Core::hresult status = Core::ERROR_GENERAL;
    uint32_t signalled = Telemetry_StateInvalid;
    struct _rbusObject rbObject;
 
    //onReportUploadStatus for "No 'UPLOAD_STATUS' value"
    ON_CALL(*p_rBusApiImplMock, rbus_open(::testing::_, ::testing::_))
        .WillByDefault(
            ::testing::Return(RBUS_ERROR_SUCCESS));
    
    ON_CALL(*p_rBusApiImplMock, rbusValue_GetString(::testing::_, ::testing::_))
        .WillByDefault(
            ::testing::Return( "SUCCESS"));
    
    /**Called rbusObject_GetValue where it returns nullptr to verify "No 'UPLOAD_STATUS' value" for onReportUploadStatus **/
    EXPECT_CALL(*p_rBusApiImplMock, rbusObject_GetValue(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](rbusObject_t object, char const* name) {
                EXPECT_EQ(object, &rbObject);
                EXPECT_EQ(string(name), _T("UPLOAD_STATUS"));
                return nullptr;
            }));
            
    ON_CALL(*p_rBusApiImplMock, rbusMethod_InvokeAsync(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [&](rbusHandle_t handle, char const* methodName, rbusObject_t inParams, rbusMethodAsyncRespHandler_t callback,  int timeout) {
                callback(handle, methodName, RBUS_ERROR_SUCCESS, &rbObject);
                return RBUS_ERROR_SUCCESS;
            }));
                
    status = m_telemetryplugin->UploadReport();
    EXPECT_EQ(status,Core::ERROR_NONE);
    if (status != Core::ERROR_NONE)
    {
        std::string errorMsg = "COM-RPC returned error " + std::to_string(status) + " (" + std::string(Core::ErrorToString(status)) + ")";
        TEST_LOG("Err: %s", errorMsg.c_str());
    }
    signalled = notify.WaitForRequestStatus(JSON_TIMEOUT,Telemetry_OnReportUpload);
    EXPECT_TRUE(signalled & Telemetry_OnReportUpload);
 
}

/************Test case Details **************************
** 1.setReportProfileStatus Without params and with Params as "No status".
** 2.setReportProfileStatus With Params as "STARTED".
** 3.setReportProfileStatus With Params as "COMPLETE".
** 4.setReportProfileStatus mocking RFC parameter and "NO Status" for failure case.
*******************************************************/

TEST_F(Telemetry_L2test, TelemetrysetReportProfileStatusUsingJsonrpc)
{

    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(TELEMETRY_CALLSIGN, TELEMETRYL2TEST_CALLSIGN);
    StrictMock<AsyncHandlerMock_Telemetry> async_handler;
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;
    std::string message;
    JsonObject expected_status;

    /* Without params and with Params as "No status" expecting Fail case*/
    status = InvokeServiceMethod("org.rdk.Telemetry.1", "setReportProfileStatus", params, result);
    EXPECT_EQ(Core::ERROR_GENERAL, status);
    EXPECT_STREQ("null", result["value"].String().c_str());

    params["status"] = "No status";
    status = InvokeServiceMethod("org.rdk.Telemetry.1", "setReportProfileStatus", params, result);
    EXPECT_EQ(Core::ERROR_GENERAL, status);
    EXPECT_STREQ("null", result["value"].String().c_str());

    /* With Params as "STARTED" expecting Success*/
    params["status"] = "STARTED";
    status = InvokeServiceMethod("org.rdk.Telemetry.1", "setReportProfileStatus", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_STREQ("null", result["value"].String().c_str());

    /* With Params as "COMPLETE" expecting Success*/
    params["status"] = "COMPLETE";
    status = InvokeServiceMethod("org.rdk.Telemetry.1", "setReportProfileStatus", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_STREQ("null", result["value"].String().c_str());

    /*mocking RFC parameter and ErrorCheck for "NO Status" */
    EXPECT_CALL(*p_rfcApiImplMock, setRFCParameter(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](char* pcCallerID, const char* pcParameterName, const char* pcParameterValue, DATA_TYPE eDataType) {
                return WDMP_FAILURE;
        }));

    params["status"] = "STARTED";
    status = InvokeServiceMethod("org.rdk.Telemetry.1", "setReportProfileStatus", params, result);
    EXPECT_EQ(Core::ERROR_GENERAL, status);
    EXPECT_STREQ("null", result["value"].String().c_str());
}

/************Test case Details **************************
** 1.setReportProfileStatus Without params and with Params as "No status" using comrpc.
** 2.setReportProfileStatus With Params as "STARTED" using comrpc.
** 3.setReportProfileStatus With Params as "COMPLETE" using comrpc.
** 4.setReportProfileStatus mocking RFC parameter and "NO Status" for failure case using comrpc.
*******************************************************/

TEST_F(Telemetry_L2test, SetReportProfileStatusUsingComrpc)
{
    Core::hresult status = Core::ERROR_GENERAL;
    string ReportStatus = "";

    /* Without ReportStatus and with ReportStatus as "No status" expecting Fail case*/
    status=m_telemetryplugin->SetReportProfileStatus(ReportStatus);
    EXPECT_EQ(Core::ERROR_GENERAL, status);
    if (status != Core::ERROR_GENERAL)
    {
        std::string errorMsg = "COM-RPC returned error " + std::to_string(status) + " (" + std::string(Core::ErrorToString(status)) + ")";
        TEST_LOG("Err: %s", errorMsg.c_str());
    }
                
    ReportStatus = "No status";
    status=m_telemetryplugin->SetReportProfileStatus(ReportStatus);
    EXPECT_EQ(Core::ERROR_GENERAL, status);
    if (status != Core::ERROR_GENERAL)
    {
        std::string errorMsg = "COM-RPC returned error " + std::to_string(status) + " (" + std::string(Core::ErrorToString(status)) + ")";
        TEST_LOG("Err: %s", errorMsg.c_str());
    }

    /*with ReportStatus as "STARTED" */
    ReportStatus = "STARTED";
    status=m_telemetryplugin->SetReportProfileStatus(ReportStatus);
    EXPECT_EQ(Core::ERROR_NONE, status);
    if (status != Core::ERROR_NONE)
    {
        std::string errorMsg = "COM-RPC returned error " + std::to_string(status) + " (" + std::string(Core::ErrorToString(status)) + ")";
        TEST_LOG("Err: %s", errorMsg.c_str());
    }

    /*with ReportStatus as "COMPLETE" */
    ReportStatus = "COMPLETE";
    status=m_telemetryplugin->SetReportProfileStatus(ReportStatus);
    EXPECT_EQ(Core::ERROR_NONE, status);
    if (status != Core::ERROR_NONE)
    {
        std::string errorMsg = "COM-RPC returned error " + std::to_string(status) + " (" + std::string(Core::ErrorToString(status)) + ")";
        TEST_LOG("Err: %s", errorMsg.c_str());
    }
	
    /*mocking RFC parameter and ErrorCheck for "NO Status" */
    EXPECT_CALL(*p_rfcApiImplMock, setRFCParameter(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](char* pcCallerID, const char* pcParameterName, const char* pcParameterValue, DATA_TYPE eDataType) {
                return WDMP_FAILURE;
            }));

    /*with ReportStatus as "STARTED" */
    ReportStatus = "STARTED";
    status=m_telemetryplugin->SetReportProfileStatus(ReportStatus);
    EXPECT_EQ(Core::ERROR_GENERAL, status);
    if (status != Core::ERROR_GENERAL)
    {
        std::string errorMsg = "COM-RPC returned error " + std::to_string(status) + " (" + std::string(Core::ErrorToString(status)) + ")";
        TEST_LOG("Err: %s", errorMsg.c_str());
    }
}
