#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mntent.h>
#include <fstream>
#include <algorithm>
#include <string>
#include <vector>
#include <cstdio>
#include "UserSettings.h"
#include "UserSettingsImplementation.h"
#include "ServiceMock.h"
#include "Store2Mock.h"
#include "COMLinkMock.h"
#include "WrapsMock.h"
#include "ThunderPortability.h"
#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);

using ::testing::NiceMock;
using namespace WPEFramework;

class UserSettingsTest : public ::testing::Test {
protected:
    Core::ProxyType<Plugin::UserSettings> plugin;
    Core::JSONRPC::Handler& handler;
    Core::JSONRPC::Context connection;
    Core::JSONRPC::Message message;
    NiceMock<ServiceMock> service;
    NiceMock<COMLinkMock> comLinkMock;
    Core::ProxyType<Plugin::UserSettingsImplementation> UserSettingsImpl;
    //Exchange::IUserSettings::INotification *notification = nullptr;
    string response;
    WrapsImplMock *p_wrapsImplMock   = nullptr;
    ServiceMock  *p_serviceMock  = nullptr;
    Store2Mock  *p_store2Mock  = nullptr;
    UserSettingsTest()
        : plugin(Core::ProxyType<Plugin::UserSettings>::Create())
        , handler(*plugin)
        , connection(1,0,"")
    {
        p_serviceMock = new NiceMock <ServiceMock>;

        p_store2Mock = new NiceMock <Store2Mock>;

        p_wrapsImplMock  = new NiceMock <WrapsImplMock>;
        Wraps::setImpl(p_wrapsImplMock);

        EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
            .WillOnce(testing::Return(p_store2Mock));

        ON_CALL(comLinkMock, Instantiate(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](const RPC::Object& object, const uint32_t waitTime, uint32_t& connectionId) {
                UserSettingsImpl = Core::ProxyType<Plugin::UserSettingsImplementation>::Create();
                return &UserSettingsImpl;
                }));

        plugin->Initialize(&service);
    }

    virtual ~UserSettingsTest() override
    {

        plugin->Deinitialize(&service);

        if (p_serviceMock != nullptr)
        {
            delete p_serviceMock;
            p_serviceMock = nullptr;
        }
        
        if (p_store2Mock != nullptr)
        {
            delete p_store2Mock;
            p_store2Mock = nullptr;
        }

        Wraps::setImpl(nullptr);
        if (p_wrapsImplMock != nullptr)
        {
            delete p_wrapsImplMock;
            p_wrapsImplMock = nullptr;
        }
    }
};

TEST_F(UserSettingsTest, Exists_AllMethods)
{
    EXPECT_TRUE(handler.Exists(_T("setAudioDescription")));
    EXPECT_TRUE(handler.Exists(_T("getAudioDescription")));
    EXPECT_TRUE(handler.Exists(_T("setPreferredAudioLanguages")));
    EXPECT_TRUE(handler.Exists(_T("getPreferredAudioLanguages")));
    EXPECT_TRUE(handler.Exists(_T("setPresentationLanguage")));
    EXPECT_TRUE(handler.Exists(_T("getPresentationLanguage")));
    EXPECT_TRUE(handler.Exists(_T("setCaptions")));
    EXPECT_TRUE(handler.Exists(_T("getCaptions")));
    EXPECT_TRUE(handler.Exists(_T("setPreferredCaptionsLanguages")));
    EXPECT_TRUE(handler.Exists(_T("getPreferredCaptionsLanguages")));
    EXPECT_TRUE(handler.Exists(_T("setPreferredClosedCaptionService")));
    EXPECT_TRUE(handler.Exists(_T("getPreferredClosedCaptionService")));
    EXPECT_TRUE(handler.Exists(_T("setPrivacyMode")));
    EXPECT_TRUE(handler.Exists(_T("getPrivacyMode")));
    EXPECT_TRUE(handler.Exists(_T("setPinControl")));
    EXPECT_TRUE(handler.Exists(_T("getPinControl")));
    EXPECT_TRUE(handler.Exists(_T("setViewingRestrictions")));
    EXPECT_TRUE(handler.Exists(_T("getViewingRestrictions")));
    EXPECT_TRUE(handler.Exists(_T("setViewingRestrictionsWindow")));
    EXPECT_TRUE(handler.Exists(_T("getViewingRestrictionsWindow")));
    EXPECT_TRUE(handler.Exists(_T("setLiveWatershed")));
    EXPECT_TRUE(handler.Exists(_T("getLiveWatershed")));
    EXPECT_TRUE(handler.Exists(_T("setPlaybackWatershed")));
    EXPECT_TRUE(handler.Exists(_T("getPlaybackWatershed")));
    EXPECT_TRUE(handler.Exists(_T("setBlockNotRatedContent")));
    EXPECT_TRUE(handler.Exists(_T("getBlockNotRatedContent")));
    EXPECT_TRUE(handler.Exists(_T("setPinOnPurchase")));
    EXPECT_TRUE(handler.Exists(_T("getPinOnPurchase")));
    EXPECT_TRUE(handler.Exists(_T("setHighContrast")));
    EXPECT_TRUE(handler.Exists(_T("getHighContrast")));
    EXPECT_TRUE(handler.Exists(_T("setVoiceGuidance")));
    EXPECT_TRUE(handler.Exists(_T("getVoiceGuidance")));
    EXPECT_TRUE(handler.Exists(_T("setVoiceGuidanceRate")));
    EXPECT_TRUE(handler.Exists(_T("getVoiceGuidanceRate")));
    EXPECT_TRUE(handler.Exists(_T("setVoiceGuidanceHints")));
    EXPECT_TRUE(handler.Exists(_T("getVoiceGuidanceHints")));
    EXPECT_TRUE(handler.Exists(_T("setContentPin")));
    EXPECT_TRUE(handler.Exists(_T("getContentPin")));
    EXPECT_TRUE(handler.Exists(_T("getMigrationState")));
    EXPECT_TRUE(handler.Exists(_T("getMigrationStates")));
    EXPECT_TRUE(handler.Exists(_T("configure")));
}

TEST_F(UserSettingsTest, SetAudioDescription_Success)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "true", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAudioDescription"), _T("{\"enabled\":true}"), response));
}

TEST_F(UserSettingsTest, SetAudioDescription_Failure)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setAudioDescription"), _T("{\"enabled\":false}"), response));
}

TEST_F(UserSettingsTest, SetAudioDescription_InvalidInput)
{
    EXPECT_EQ(Core::ERROR_INVALID_INPUT, handler.Invoke(connection, _T("setAudioDescription"), _T("{\"enabled\":\"notabool\"}"), response));
}

TEST_F(UserSettingsTest, GetAudioDescription_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("true"), ::testing::Return(Core::ERROR_NONE)));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getAudioDescription"), _T("{}"), response));
}

TEST_F(UserSettingsTest, GetAudioDescription_Failure)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getAudioDescription"), _T("{}"), response));
}

TEST_F(UserSettingsTest, SetPreferredAudioLanguages_Valid)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "eng,fra", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setPreferredAudioLanguages"), _T("{\"preferredLanguages\":\"eng,fra\"}"), response));
}

TEST_F(UserSettingsTest, SetPreferredAudioLanguages_Empty)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setPreferredAudioLanguages"), _T("{\"preferredLanguages\":\"\"}"), response));
}

TEST_F(UserSettingsTest, SetPreferredAudioLanguages_Malformed)
{
    EXPECT_EQ(Core::ERROR_INVALID_INPUT, handler.Invoke(connection, _T("setPreferredAudioLanguages"), _T("{\"preferredLanguages\":123}"), response));
}

TEST_F(UserSettingsTest, GetPreferredAudioLanguages_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("eng,fra"), ::testing::Return(Core::ERROR_NONE)));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPreferredAudioLanguages"), _T("{}"), response));
}

TEST_F(UserSettingsTest, GetPreferredAudioLanguages_Failure)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getPreferredAudioLanguages"), _T("{}"), response));
}

TEST_F(UserSettingsTest, SetPresentationLanguage_Valid)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "en-US", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setPresentationLanguage"), _T("{\"presentationLanguage\":\"en-US\"}"), response));
}

TEST_F(UserSettingsTest, SetPresentationLanguage_Empty)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setPresentationLanguage"), _T("{\"presentationLanguage\":\"\"}"), response));
}

TEST_F(UserSettingsTest, SetPresentationLanguage_Malformed)
{
    EXPECT_EQ(Core::ERROR_INVALID_INPUT, handler.Invoke(connection, _T("setPresentationLanguage"), _T("{\"presentationLanguage\":123}"), response));
}

TEST_F(UserSettingsTest, GetPresentationLanguage_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("en-US"), ::testing::Return(Core::ERROR_NONE)));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPresentationLanguage"), _T("{}"), response));
}

TEST_F(UserSettingsTest, GetPresentationLanguage_Failure)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getPresentationLanguage"), _T("{}"), response));
}

TEST_F(UserSettingsTest, SetCaptions_True)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "true", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setCaptions"), _T("{\"enabled\":true}"), response));
}

TEST_F(UserSettingsTest, SetCaptions_False)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "false", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setCaptions"), _T("{\"enabled\":false}"), response));
}

TEST_F(UserSettingsTest, SetCaptions_Invalid)
{
    EXPECT_EQ(Core::ERROR_INVALID_INPUT, handler.Invoke(connection, _T("setCaptions"), _T("{\"enabled\":\"notabool\"}"), response));
}

TEST_F(UserSettingsTest, GetCaptions_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("true"), ::testing::Return(Core::ERROR_NONE)));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getCaptions"), _T("{}"), response));
}

TEST_F(UserSettingsTest, GetCaptions_Failure)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getCaptions"), _T("{}"), response));
}

TEST_F(UserSettingsTest, SetPreferredCaptionsLanguages_Valid)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "eng,fra", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setPreferredCaptionsLanguages"), _T("{\"preferredLanguages\":\"eng,fra\"}"), response));
}

TEST_F(UserSettingsTest, SetPreferredCaptionsLanguages_Empty)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setPreferredCaptionsLanguages"), _T("{\"preferredLanguages\":\"\"}"), response));
}

TEST_F(UserSettingsTest, SetPreferredCaptionsLanguages_Malformed)
{
    EXPECT_EQ(Core::ERROR_INVALID_INPUT, handler.Invoke(connection, _T("setPreferredCaptionsLanguages"), _T("{\"preferredLanguages\":123}"), response));
}

TEST_F(UserSettingsTest, GetPreferredCaptionsLanguages_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("eng,fra"), ::testing::Return(Core::ERROR_NONE)));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPreferredCaptionsLanguages"), _T("{}"), response));
}

TEST_F(UserSettingsTest, GetPreferredCaptionsLanguages_Failure)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getPreferredCaptionsLanguages"), _T("{}"), response));
}

TEST_F(UserSettingsTest, SetPreferredClosedCaptionService_Valid)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "CC3", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setPreferredClosedCaptionService"), _T("{\"service\":\"CC3\"}"), response));
}

TEST_F(UserSettingsTest, SetPreferredClosedCaptionService_Invalid)
{
    EXPECT_EQ(Core::ERROR_INVALID_INPUT, handler.Invoke(connection, _T("setPreferredClosedCaptionService"), _T("{\"service\":123}"), response));
}

TEST_F(UserSettingsTest, GetPreferredClosedCaptionService_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("CC3"), ::testing::Return(Core::ERROR_NONE)));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPreferredClosedCaptionService"), _T("{}"), response));
}

TEST_F(UserSettingsTest, GetPreferredClosedCaptionService_Failure)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getPreferredClosedCaptionService"), _T("{}"), response));
}

TEST_F(UserSettingsTest, SetPrivacyMode_Valid)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "SHARE", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setPrivacyMode"), _T("{\"privacyMode\":\"SHARE\"}"), response));
}

TEST_F(UserSettingsTest, SetPrivacyMode_Invalid)
{
    EXPECT_EQ(Core::ERROR_INVALID_INPUT, handler.Invoke(connection, _T("setPrivacyMode"), _T("{\"privacyMode\":123}"), response));
}

TEST_F(UserSettingsTest, GetPrivacyMode_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("SHARE"), ::testing::Return(Core::ERROR_NONE)));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPrivacyMode"), _T("{}"), response));
}

TEST_F(UserSettingsTest, GetPrivacyMode_Failure)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getPrivacyMode"), _T("{}"), response));
}

TEST_F(UserSettingsTest, SetPinControl_True)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "true", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setPinControl"), _T("{\"pinControl\":true}"), response));
}

TEST_F(UserSettingsTest, SetPinControl_False)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "false", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setPinControl"), _T("{\"pinControl\":false}"), response));
}

TEST_F(UserSettingsTest, SetPinControl_Invalid)
{
    EXPECT_EQ(Core::ERROR_INVALID_INPUT, handler.Invoke(connection, _T("setPinControl"), _T("{\"pinControl\":\"notabool\"}"), response));
}

TEST_F(UserSettingsTest, GetPinControl_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("true"), ::testing::Return(Core::ERROR_NONE)));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPinControl"), _T("{}"), response));
}

TEST_F(UserSettingsTest, GetPinControl_Failure)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getPinControl"), _T("{}"), response));
}

TEST_F(UserSettingsTest, SetViewingRestrictions_Valid)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "{\"rating\":\"PG\"}", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setViewingRestrictions"), _T("{\"viewingRestrictions\":\"{\\\"rating\\\":\\\"PG\\\"}\"}"), response));
}

TEST_F(UserSettingsTest, SetViewingRestrictions_Empty)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setViewingRestrictions"), _T("{\"viewingRestrictions\":\"\"}"), response));
}

TEST_F(UserSettingsTest, SetViewingRestrictions_Malformed)
{
    EXPECT_EQ(Core::ERROR_INVALID_INPUT, handler.Invoke(connection, _T("setViewingRestrictions"), _T("{\"viewingRestrictions\":123}"), response));
}

TEST_F(UserSettingsTest, GetViewingRestrictions_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("{\"rating\":\"PG\"}"), ::testing::Return(Core::ERROR_NONE)));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getViewingRestrictions"), _T("{}"), response));
}

TEST_F(UserSettingsTest, GetViewingRestrictions_Failure)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getViewingRestrictions"), _T("{}"), response));
}

TEST_F(UserSettingsTest, SetViewingRestrictionsWindow_Valid)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "ALWAYS", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setViewingRestrictionsWindow"), _T("{\"viewingRestrictionsWindow\":\"ALWAYS\"}"), response));
}

TEST_F(UserSettingsTest, SetViewingRestrictionsWindow_Empty)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setViewingRestrictionsWindow"), _T("{\"viewingRestrictionsWindow\":\"\"}"), response));
}

TEST_F(UserSettingsTest, SetViewingRestrictionsWindow_Malformed)
{
    EXPECT_EQ(Core::ERROR_INVALID_INPUT, handler.Invoke(connection, _T("setViewingRestrictionsWindow"), _T("{\"viewingRestrictionsWindow\":123}"), response));
}

TEST_F(UserSettingsTest, GetViewingRestrictionsWindow_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("ALWAYS"), ::testing::Return(Core::ERROR_NONE)));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getViewingRestrictionsWindow"), _T("{}"), response));
}

TEST_F(UserSettingsTest, GetViewingRestrictionsWindow_Failure)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getViewingRestrictionsWindow"), _T("{}"), response));
}

TEST_F(UserSettingsTest, SetLiveWatershed_True)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "true", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setLiveWatershed"), _T("{\"liveWatershed\":true}"), response));
}

TEST_F(UserSettingsTest, SetLiveWatershed_False)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "false", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setLiveWatershed"), _T("{\"liveWatershed\":false}"), response));
}

TEST_F(UserSettingsTest, SetLiveWatershed_Invalid)
{
    EXPECT_EQ(Core::ERROR_INVALID_INPUT, handler.Invoke(connection, _T("setLiveWatershed"), _T("{\"liveWatershed\":\"notabool\"}"), response));
}

TEST_F(UserSettingsTest, GetLiveWatershed_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("true"), ::testing::Return(Core::ERROR_NONE)));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getLiveWatershed"), _T("{}"), response));
}

TEST_F(UserSettingsTest, GetLiveWatershed_Failure)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getLiveWatershed"), _T("{}"), response));
}

TEST_F(UserSettingsTest, SetPlaybackWatershed_True)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "true", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setPlaybackWatershed"), _T("{\"playbackWatershed\":true}"), response));
}

TEST_F(UserSettingsTest, SetPlaybackWatershed_False)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "false", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setPlaybackWatershed"), _T("{\"playbackWatershed\":false}"), response));
}

TEST_F(UserSettingsTest, SetPlaybackWatershed_Invalid)
{
    EXPECT_EQ(Core::ERROR_INVALID_INPUT, handler.Invoke(connection, _T("setPlaybackWatershed"), _T("{\"playbackWatershed\":\"notabool\"}"), response));
}

TEST_F(UserSettingsTest, GetPlaybackWatershed_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("true"), ::testing::Return(Core::ERROR_NONE)));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPlaybackWatershed"), _T("{}"), response));
}

TEST_F(UserSettingsTest, GetPlaybackWatershed_Failure)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getPlaybackWatershed"), _T("{}"), response));
}

TEST_F(UserSettingsTest, SetBlockNotRatedContent_True)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "true", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setBlockNotRatedContent"), _T("{\"blockNotRatedContent\":true}"), response));
}

TEST_F(UserSettingsTest, SetBlockNotRatedContent_False)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "false", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setBlockNotRatedContent"), _T("{\"blockNotRatedContent\":false}"), response));
}

TEST_F(UserSettingsTest, SetBlockNotRatedContent_Invalid)
{
    EXPECT_EQ(Core::ERROR_INVALID_INPUT, handler.Invoke(connection, _T("setBlockNotRatedContent"), _T("{\"blockNotRatedContent\":\"notabool\"}"), response));
}

TEST_F(UserSettingsTest, GetBlockNotRatedContent_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("true"), ::testing::Return(Core::ERROR_NONE)));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getBlockNotRatedContent"), _T("{}"), response));
}

TEST_F(UserSettingsTest, GetBlockNotRatedContent_Failure)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getBlockNotRatedContent"), _T("{}"), response));
}

TEST_F(UserSettingsTest, SetPinOnPurchase_True)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "true", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setPinOnPurchase"), _T("{\"pinOnPurchase\":true}"), response));
}

TEST_F(UserSettingsTest, SetPinOnPurchase_False)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "false", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setPinOnPurchase"), _T("{\"pinOnPurchase\":false}"), response));
}

TEST_F(UserSettingsTest, SetPinOnPurchase_Invalid)
{
    EXPECT_EQ(Core::ERROR_INVALID_INPUT, handler.Invoke(connection, _T("setPinOnPurchase"), _T("{\"pinOnPurchase\":\"notabool\"}"), response));
}

TEST_F(UserSettingsTest, GetPinOnPurchase_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("true"), ::testing::Return(Core::ERROR_NONE)));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPinOnPurchase"), _T("{}"), response));
}

TEST_F(UserSettingsTest, GetPinOnPurchase_Failure)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getPinOnPurchase"), _T("{}"), response));
}

TEST_F(UserSettingsTest, SetHighContrast_True)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "true", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setHighContrast"), _T("{\"enabled\":true}"), response));
}

TEST_F(UserSettingsTest, SetHighContrast_False)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "false", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setHighContrast"), _T("{\"enabled\":false}"), response));
}

TEST_F(UserSettingsTest, SetHighContrast_Invalid)
{
    EXPECT_EQ(Core::ERROR_INVALID_INPUT, handler.Invoke(connection, _T("setHighContrast"), _T("{\"enabled\":\"notabool\"}"), response));
}

TEST_F(UserSettingsTest, GetHighContrast_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("true"), ::testing::Return(Core::ERROR_NONE)));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getHighContrast"), _T("{}"), response));
}

TEST_F(UserSettingsTest, GetHighContrast_Failure)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getHighContrast"), _T("{}"), response));
}

TEST_F(UserSettingsTest, SetVoiceGuidance_True)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "true", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setVoiceGuidance"), _T("{\"enabled\":true}"), response));
}

TEST_F(UserSettingsTest, SetVoiceGuidance_False)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "false", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setVoiceGuidance"), _T("{\"enabled\":false}"), response));
}

TEST_F(UserSettingsTest, SetVoiceGuidance_Invalid)
{
    EXPECT_EQ(Core::ERROR_INVALID_INPUT, handler.Invoke(connection, _T("setVoiceGuidance"), _T("{\"enabled\":\"notabool\"}"), response));
}

TEST_F(UserSettingsTest, GetVoiceGuidance_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("true"), ::testing::Return(Core::ERROR_NONE)));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getVoiceGuidance"), _T("{}"), response));
}

TEST_F(UserSettingsTest, GetVoiceGuidance_Failure)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getVoiceGuidance"), _T("{}"), response));
}

TEST_F(UserSettingsTest, SetVoiceGuidanceRate_Valid)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "1.0", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setVoiceGuidanceRate"), _T("{\"rate\":1.0}"), response));
}

TEST_F(UserSettingsTest, SetVoiceGuidanceRate_Boundary_Min)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "0.1", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setVoiceGuidanceRate"), _T("{\"rate\":0.1}"), response));
}

TEST_F(UserSettingsTest, SetVoiceGuidanceRate_Boundary_Max)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "10.0", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setVoiceGuidanceRate"), _T("{\"rate\":10.0}"), response));
}

TEST_F(UserSettingsTest, SetVoiceGuidanceRate_OutOfBounds)
{
    EXPECT_EQ(Core::ERROR_INVALID_INPUT, handler.Invoke(connection, _T("setVoiceGuidanceRate"), _T("{\"rate\":0.0}"), response));
    EXPECT_EQ(Core::ERROR_INVALID_INPUT, handler.Invoke(connection, _T("setVoiceGuidanceRate"), _T("{\"rate\":11.0}"), response));
}

TEST_F(UserSettingsTest, SetVoiceGuidanceRate_Invalid)
{
    EXPECT_EQ(Core::ERROR_INVALID_INPUT, handler.Invoke(connection, _T("setVoiceGuidanceRate"), _T("{\"rate\":\"notadouble\"}"), response));
}

TEST_F(UserSettingsTest, GetVoiceGuidanceRate_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("1.0"), ::testing::Return(Core::ERROR_NONE)));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getVoiceGuidanceRate"), _T("{}"), response));
}

TEST_F(UserSettingsTest, GetVoiceGuidanceRate_Failure)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getVoiceGuidanceRate"), _T("{}"), response));
}

TEST_F(UserSettingsTest, SetVoiceGuidanceHints_True)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "true", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setVoiceGuidanceHints"), _T("{\"hints\":true}"), response));
}

TEST_F(UserSettingsTest, SetVoiceGuidanceHints_False)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "false", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setVoiceGuidanceHints"), _T("{\"hints\":false}"), response));
}

TEST_F(UserSettingsTest, SetVoiceGuidanceHints_Invalid)
{
    EXPECT_EQ(Core::ERROR_INVALID_INPUT, handler.Invoke(connection, _T("setVoiceGuidanceHints"), _T("{\"hints\":\"notabool\"}"), response));
}

TEST_F(UserSettingsTest, GetVoiceGuidanceHints_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("true"), ::testing::Return(Core::ERROR_NONE)));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getVoiceGuidanceHints"), _T("{}"), response));
}

TEST_F(UserSettingsTest, GetVoiceGuidanceHints_Failure)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getVoiceGuidanceHints"), _T("{}"), response));
}

TEST_F(UserSettingsTest, SetContentPin_Valid)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "1234", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setContentPin"), _T("{\"contentPin\":\"1234\"}"), response));
}

TEST_F(UserSettingsTest, SetContentPin_Empty)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, "", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setContentPin"), _T("{\"contentPin\":\"\"}"), response));
}

TEST_F(UserSettingsTest, SetContentPin_Malformed)
{
    EXPECT_EQ(Core::ERROR_INVALID_INPUT, handler.Invoke(connection, _T("setContentPin"), _T("{\"contentPin\":1234}"), response));
}

TEST_F(UserSettingsTest, GetContentPin_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("1234"), ::testing::Return(Core::ERROR_NONE)));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getContentPin"), _T("{}"), response));
}

TEST_F(UserSettingsTest, GetContentPin_Failure)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getContentPin"), _T("{}"), response));
}

TEST_F(UserSettingsTest, GetMigrationState_Valid)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getMigrationState"), _T("{\"key\":1}"), response));
}

TEST_F(UserSettingsTest, GetMigrationState_Invalid)
{
    EXPECT_EQ(Core::ERROR_INVALID_INPUT, handler.Invoke(connection, _T("getMigrationState"), _T("{\"key\":\"notakey\"}"), response));
}

TEST_F(UserSettingsTest, GetMigrationStates_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getMigrationStates"), _T("{}"), response));
}

TEST_F(UserSettingsTest, Configure_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("configure"), _T("{}"), response));
}
