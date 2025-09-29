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
using ::testing::_;
using ::testing::Return;
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

TEST_F(UserSettingsTest, SetAudioDescription_Success)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_AUDIO_DESCRIPTION_KEY, "true", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAudioDescription"), _T("{\"enabled\": true}"), response));
}

TEST_F(UserSettingsTest, SetAudioDescription_Failure)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_AUDIO_DESCRIPTION_KEY, "true", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setAudioDescription"), _T("{\"enabled\": true}"), response));
}

TEST_F(UserSettingsTest, GetAudioDescription_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, USERSETTINGS_AUDIO_DESCRIPTION_KEY, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("true"), ::testing::Return(Core::ERROR_NONE)));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getAudioDescription"), _T("{}"), response));
    EXPECT_TRUE(response.find("true") != string::npos);
}

TEST_F(UserSettingsTest, GetAudioDescription_Failure)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, USERSETTINGS_AUDIO_DESCRIPTION_KEY, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getAudioDescription"), _T("{}"), response));
}

TEST_F(UserSettingsTest, SetPreferredAudioLanguages_Success)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_PREFERRED_AUDIO_LANGUAGES_KEY, "eng,fra", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setPreferredAudioLanguages"), _T("{\"preferredLanguages\": \"eng,fra\"}"), response));
}

TEST_F(UserSettingsTest, SetPreferredAudioLanguages_Failure)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_PREFERRED_AUDIO_LANGUAGES_KEY, "eng,fra", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setPreferredAudioLanguages"), _T("{\"preferredLanguages\": \"eng,fra\"}"), response));
}

TEST_F(UserSettingsTest, GetPreferredAudioLanguages_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, USERSETTINGS_PREFERRED_AUDIO_LANGUAGES_KEY, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("eng,fra"), ::testing::Return(Core::ERROR_NONE)));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPreferredAudioLanguages"), _T("{}"), response));
    EXPECT_TRUE(response.find("eng,fra") != string::npos);
}

TEST_F(UserSettingsTest, GetPreferredAudioLanguages_DefaultValue)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, USERSETTINGS_PREFERRED_AUDIO_LANGUAGES_KEY, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_UNKNOWN_KEY));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPreferredAudioLanguages"), _T("{}"), response));
}

TEST_F(UserSettingsTest, SetPresentationLanguage_Success)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_PRESENTATION_LANGUAGE_KEY, "en-US", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setPresentationLanguage"), _T("{\"presentationLanguage\": \"en-US\"}"), response));
}

TEST_F(UserSettingsTest, SetPresentationLanguage_Failure)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_PRESENTATION_LANGUAGE_KEY, "en-US", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setPresentationLanguage"), _T("{\"presentationLanguage\": \"en-US\"}"), response));
}

TEST_F(UserSettingsTest, GetPresentationLanguage_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, USERSETTINGS_PRESENTATION_LANGUAGE_KEY, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("en-US"), ::testing::Return(Core::ERROR_NONE)));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPresentationLanguage"), _T("{}"), response));
    EXPECT_TRUE(response.find("en-US") != string::npos);
}

TEST_F(UserSettingsTest, SetCaptions_Success)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_CAPTIONS_KEY, "true", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setCaptions"), _T("{\"enabled\": true}"), response));
}

TEST_F(UserSettingsTest, SetCaptions_Failure)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_CAPTIONS_KEY, "true", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setCaptions"), _T("{\"enabled\": true}"), response));
}

TEST_F(UserSettingsTest, GetCaptions_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, USERSETTINGS_CAPTIONS_KEY, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("true"), ::testing::Return(Core::ERROR_NONE)));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getCaptions"), _T("{}"), response));
    EXPECT_TRUE(response.find("true") != string::npos);
}

TEST_F(UserSettingsTest, SetPreferredCaptionsLanguages_Success)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_PREFERRED_CAPTIONS_LANGUAGES_KEY, "eng,fra", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setPreferredCaptionsLanguages"), _T("{\"preferredLanguages\": \"eng,fra\"}"), response));
}

TEST_F(UserSettingsTest, SetPreferredCaptionsLanguages_Failure)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_PREFERRED_CAPTIONS_LANGUAGES_KEY, "eng,fra", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setPreferredCaptionsLanguages"), _T("{\"preferredLanguages\": \"eng,fra\"}"), response));
}

TEST_F(UserSettingsTest, GetPreferredCaptionsLanguages_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, USERSETTINGS_PREFERRED_CAPTIONS_LANGUAGES_KEY, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("eng,fra"), ::testing::Return(Core::ERROR_NONE)));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPreferredCaptionsLanguages"), _T("{}"), response));
    EXPECT_TRUE(response.find("eng,fra") != string::npos);
}

TEST_F(UserSettingsTest, SetPreferredClosedCaptionService_Success)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_PREFERRED_CLOSED_CAPTIONS_SERVICE_KEY, "CC3", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setPreferredClosedCaptionService"), _T("{\"service\": \"CC3\"}"), response));
}

TEST_F(UserSettingsTest, SetPreferredClosedCaptionService_Failure)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_PREFERRED_CLOSED_CAPTIONS_SERVICE_KEY, "CC3", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setPreferredClosedCaptionService"), _T("{\"service\": \"CC3\"}"), response));
}

TEST_F(UserSettingsTest, GetPreferredClosedCaptionService_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, USERSETTINGS_PREFERRED_CLOSED_CAPTIONS_SERVICE_KEY, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("CC3"), ::testing::Return(Core::ERROR_NONE)));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPreferredClosedCaptionService"), _T("{}"), response));
    EXPECT_TRUE(response.find("CC3") != string::npos);
}

TEST_F(UserSettingsTest, SetPrivacyMode_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, "privacyMode", ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("SHARE"), ::testing::Return(Core::ERROR_NONE)));
    
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, "privacyMode", "DO_NOT_SHARE", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setPrivacyMode"), _T("{\"privacyMode\": \"DO_NOT_SHARE\"}"), response));
}

TEST_F(UserSettingsTest, SetPrivacyMode_InvalidValue)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setPrivacyMode"), _T("{\"privacyMode\": \"INVALID_VALUE\"}"), response));
}

TEST_F(UserSettingsTest, GetPrivacyMode_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, "privacyMode", ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("SHARE"), ::testing::Return(Core::ERROR_NONE)));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPrivacyMode"), _T("{}"), response));
    EXPECT_TRUE(response.find("SHARE") != string::npos);
}

TEST_F(UserSettingsTest, GetPrivacyMode_DefaultValue)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, "privacyMode", ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("INVALID"), ::testing::Return(Core::ERROR_NONE)));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPrivacyMode"), _T("{}"), response));
    EXPECT_TRUE(response.find("SHARE") != string::npos);  // Default value should be returned
}

TEST_F(UserSettingsTest, SetPinControl_Success)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_PIN_CONTROL_KEY, "true", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setPinControl"), _T("{\"pinControl\": true}"), response));
}

TEST_F(UserSettingsTest, SetPinControl_Failure)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_PIN_CONTROL_KEY, "true", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setPinControl"), _T("{\"pinControl\": true}"), response));
}

TEST_F(UserSettingsTest, GetPinControl_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, USERSETTINGS_PIN_CONTROL_KEY, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("true"), ::testing::Return(Core::ERROR_NONE)));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPinControl"), _T("{}"), response));
    EXPECT_TRUE(response.find("true") != string::npos);
}

TEST_F(UserSettingsTest, SetViewingRestrictions_Success)
{
    std::string restrictions = "{\"rating\":\"TV-14\"}";
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_VIEWING_RESTRICTIONS_KEY, restrictions, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setViewingRestrictions"), 
        _T("{\"viewingRestrictions\": \"{\\\"rating\\\":\\\"TV-14\\\"}\"}"), response));
}

TEST_F(UserSettingsTest, SetViewingRestrictions_Failure)
{
    std::string restrictions = "{\"rating\":\"TV-14\"}";
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_VIEWING_RESTRICTIONS_KEY, restrictions, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setViewingRestrictions"), 
        _T("{\"viewingRestrictions\": \"{\\\"rating\\\":\\\"TV-14\\\"}\"}"), response));
}

TEST_F(UserSettingsTest, GetViewingRestrictions_Success)
{
    std::string restrictions = "{\"rating\":\"TV-14\"}";
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, USERSETTINGS_VIEWING_RESTRICTIONS_KEY, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>(restrictions), ::testing::Return(Core::ERROR_NONE)));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getViewingRestrictions"), _T("{}"), response));
    EXPECT_TRUE(response.find(restrictions) != string::npos);
}

TEST_F(UserSettingsTest, SetViewingRestrictionsWindow_Success)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_VIEWING_RESTRICTIONS_WINDOW_KEY, "ALWAYS", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setViewingRestrictionsWindow"), 
        _T("{\"viewingRestrictionsWindow\": \"ALWAYS\"}"), response));
}

TEST_F(UserSettingsTest, SetViewingRestrictionsWindow_Failure)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_VIEWING_RESTRICTIONS_WINDOW_KEY, "ALWAYS", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setViewingRestrictionsWindow"), 
        _T("{\"viewingRestrictionsWindow\": \"ALWAYS\"}"), response));
}

TEST_F(UserSettingsTest, GetViewingRestrictionsWindow_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, USERSETTINGS_VIEWING_RESTRICTIONS_WINDOW_KEY, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("ALWAYS"), ::testing::Return(Core::ERROR_NONE)));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getViewingRestrictionsWindow"), _T("{}"), response));
    EXPECT_TRUE(response.find("ALWAYS") != string::npos);
}

TEST_F(UserSettingsTest, SetLiveWatershed_Success)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_LIVE_WATERSHED_KEY, "true", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setLiveWatershed"), _T("{\"liveWatershed\": true}"), response));
}

TEST_F(UserSettingsTest, SetLiveWatershed_Failure)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_LIVE_WATERSHED_KEY, "true", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setLiveWatershed"), _T("{\"liveWatershed\": true}"), response));
}

TEST_F(UserSettingsTest, GetLiveWatershed_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, USERSETTINGS_LIVE_WATERSHED_KEY, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("true"), ::testing::Return(Core::ERROR_NONE)));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getLiveWatershed"), _T("{}"), response));
    EXPECT_TRUE(response.find("true") != string::npos);
}

TEST_F(UserSettingsTest, SetPlaybackWatershed_Success)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_PLAYBACK_WATERSHED_KEY, "true", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setPlaybackWatershed"), _T("{\"playbackWatershed\": true}"), response));
}

TEST_F(UserSettingsTest, SetPlaybackWatershed_Failure)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_PLAYBACK_WATERSHED_KEY, "true", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setPlaybackWatershed"), _T("{\"playbackWatershed\": true}"), response));
}

TEST_F(UserSettingsTest, GetPlaybackWatershed_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, USERSETTINGS_PLAYBACK_WATERSHED_KEY, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("true"), ::testing::Return(Core::ERROR_NONE)));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPlaybackWatershed"), _T("{}"), response));
    EXPECT_TRUE(response.find("true") != string::npos);
}

TEST_F(UserSettingsTest, SetBlockNotRatedContent_Success)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_BLOCK_NOT_RATED_CONTENT_KEY, "true", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setBlockNotRatedContent"), _T("{\"blockNotRatedContent\": true}"), response));
}

TEST_F(UserSettingsTest, SetBlockNotRatedContent_Failure)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_BLOCK_NOT_RATED_CONTENT_KEY, "true", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setBlockNotRatedContent"), _T("{\"blockNotRatedContent\": true}"), response));
}

TEST_F(UserSettingsTest, GetBlockNotRatedContent_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, USERSETTINGS_BLOCK_NOT_RATED_CONTENT_KEY, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("true"), ::testing::Return(Core::ERROR_NONE)));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getBlockNotRatedContent"), _T("{}"), response));
    EXPECT_TRUE(response.find("true") != string::npos);
}

TEST_F(UserSettingsTest, SetPinOnPurchase_Success)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_PIN_ON_PURCHASE_KEY, "true", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setPinOnPurchase"), _T("{\"pinOnPurchase\": true}"), response));
}

TEST_F(UserSettingsTest, SetPinOnPurchase_Failure)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_PIN_ON_PURCHASE_KEY, "true", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setPinOnPurchase"), _T("{\"pinOnPurchase\": true}"), response));
}

TEST_F(UserSettingsTest, GetPinOnPurchase_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, USERSETTINGS_PIN_ON_PURCHASE_KEY, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("true"), ::testing::Return(Core::ERROR_NONE)));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPinOnPurchase"), _T("{}"), response));
    EXPECT_TRUE(response.find("true") != string::npos);
}

TEST_F(UserSettingsTest, SetHighContrast_Success)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_HIGH_CONTRAST_KEY, "true", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setHighContrast"), _T("{\"enabled\": true}"), response));
}

TEST_F(UserSettingsTest, SetHighContrast_Failure)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_HIGH_CONTRAST_KEY, "true", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setHighContrast"), _T("{\"enabled\": true}"), response));
}

TEST_F(UserSettingsTest, GetHighContrast_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, USERSETTINGS_HIGH_CONTRAST_KEY, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("true"), ::testing::Return(Core::ERROR_NONE)));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getHighContrast"), _T("{}"), response));
    EXPECT_TRUE(response.find("true") != string::npos);
}

TEST_F(UserSettingsTest, SetVoiceGuidance_Success)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_VOICE_GUIDANCE_KEY, "true", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setVoiceGuidance"), _T("{\"enabled\": true}"), response));
}

TEST_F(UserSettingsTest, SetVoiceGuidance_Failure)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_VOICE_GUIDANCE_KEY, "true", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setVoiceGuidance"), _T("{\"enabled\": true}"), response));
}

TEST_F(UserSettingsTest, GetVoiceGuidance_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, USERSETTINGS_VOICE_GUIDANCE_KEY, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("true"), ::testing::Return(Core::ERROR_NONE)));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getVoiceGuidance"), _T("{}"), response));
    EXPECT_TRUE(response.find("true") != string::npos);
}

TEST_F(UserSettingsTest, SetVoiceGuidanceRate_Success)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_VOICE_GUIDANCE_RATE_KEY, "2.5", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setVoiceGuidanceRate"), _T("{\"rate\": 2.5}"), response));
}

TEST_F(UserSettingsTest, SetVoiceGuidanceRate_Failure)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_VOICE_GUIDANCE_RATE_KEY, "2.5", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setVoiceGuidanceRate"), _T("{\"rate\": 2.5}"), response));
}

TEST_F(UserSettingsTest, SetVoiceGuidanceRate_InvalidValue_TooHigh)
{
    // Rate must be between 0.1 and 10
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("setVoiceGuidanceRate"), _T("{\"rate\": 11}"), response));
}

TEST_F(UserSettingsTest, SetVoiceGuidanceRate_InvalidValue_TooLow)
{
    // Rate must be between 0.1 and 10
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("setVoiceGuidanceRate"), _T("{\"rate\": 0.05}"), response));
}

TEST_F(UserSettingsTest, GetVoiceGuidanceRate_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, USERSETTINGS_VOICE_GUIDANCE_RATE_KEY, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("2.5"), ::testing::Return(Core::ERROR_NONE)));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getVoiceGuidanceRate"), _T("{}"), response));
    EXPECT_TRUE(response.find("2.5") != string::npos);
}

TEST_F(UserSettingsTest, SetVoiceGuidanceHints_Success)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_VOICE_GUIDANCE_HINTS_KEY, "true", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setVoiceGuidanceHints"), _T("{\"hints\": true}"), response));
}

TEST_F(UserSettingsTest, SetVoiceGuidanceHints_Failure)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_VOICE_GUIDANCE_HINTS_KEY, "true", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setVoiceGuidanceHints"), _T("{\"hints\": true}"), response));
}

TEST_F(UserSettingsTest, GetVoiceGuidanceHints_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, USERSETTINGS_VOICE_GUIDANCE_HINTS_KEY, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("true"), ::testing::Return(Core::ERROR_NONE)));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getVoiceGuidanceHints"), _T("{}"), response));
    EXPECT_TRUE(response.find("true") != string::npos);
}

TEST_F(UserSettingsTest, SetContentPin_Success)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_CONTENT_PIN_KEY, "1234", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setContentPin"), _T("{\"contentPin\": \"1234\"}"), response));
}

TEST_F(UserSettingsTest, SetContentPin_Failure)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_CONTENT_PIN_KEY, "1234", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setContentPin"), _T("{\"contentPin\": \"1234\"}"), response));
}

TEST_F(UserSettingsTest, SetContentPin_Empty_Success)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, USERSETTINGS_CONTENT_PIN_KEY, "", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setContentPin"), _T("{\"contentPin\": \"\"}"), response));
}

TEST_F(UserSettingsTest, SetContentPin_InvalidFormat)
{
    // Content PIN must be 4 digits
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("setContentPin"), _T("{\"contentPin\": \"123A\"}"), response));
}

TEST_F(UserSettingsTest, GetContentPin_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, USERSETTINGS_CONTENT_PIN_KEY, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>("1234"), ::testing::Return(Core::ERROR_NONE)));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getContentPin"), _T("{}"), response));
    EXPECT_TRUE(response.find("1234") != string::npos);
}

TEST_F(UserSettingsTest, GetMigrationState_Success)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, USERSETTINGS_VOICE_GUIDANCE_KEY, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getMigrationState"), 
        _T("{\"key\": 15}"), response)); // 15 = VOICE_GUIDANCE
    EXPECT_TRUE(response.find("false") != string::npos); // requiresMigration should be false
}

TEST_F(UserSettingsTest, GetMigrationState_RequiresMigration)
{
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, USERSETTINGS_VOICE_GUIDANCE_KEY, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_UNKNOWN_KEY));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getMigrationState"), 
        _T("{\"key\": 15}"), response)); // 15 = VOICE_GUIDANCE
    EXPECT_TRUE(response.find("true") != string::npos); // requiresMigration should be true
}

TEST_F(UserSettingsTest, GetMigrationState_InvalidKey)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getMigrationState"), 
        _T("{\"key\": 999}"), response)); // Invalid key
}

TEST_F(UserSettingsTest, GetMigrationStates_Success)
{
    // For simplicity, we'll mock just a few calls, as the real implementation would make 18 calls
    EXPECT_CALL(*p_store2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(Core::ERROR_NONE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getMigrationStates"), _T("{}"), response));
    EXPECT_TRUE(response.find("states") != string::npos);
}
