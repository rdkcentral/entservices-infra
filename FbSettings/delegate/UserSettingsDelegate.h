/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2020 RDK Management
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
#ifndef __USERSETTINGSDELEGATE_H__
#define __USERSETTINGSDELEGATE_H__
#include "StringUtils.h"
#include "BaseEventDelegate.h"
#include <interfaces/IUserSettings.h>
#include "UtilsLogging.h"
#include "ObjectUtils.h"
#include <set>
using namespace WPEFramework;
#define USERSETTINGS_CALLSIGN "org.rdk.UserSettings"

static const std::set<string> VALID_USER_SETTINGS_EVENT = {
    "localization.onlanguagechanged",
    "localization.onlocalechanged",
    "localization.onpreferredaudiolanguageschanged",
    "accessibility.onaudiodescriptionsettingschanged",
    "accessibility.onhighcontrastuichanged",
    "closedcaptions.onenabledchanged",
    "closedcaptions.onpreferredlanguageschanged",
    "accessibility.onclosedcaptionssettingschanged",
    "accessibility.onvoiceguidancesettingschanged",
};

class UserSettingsDelegate : public BaseEventDelegate{
    public:
        UserSettingsDelegate(PluginHost::IShell* shell,Exchange::IAppNotifications* appNotifications):
            BaseEventDelegate(appNotifications), mUserSettings(nullptr), mShell(shell), mNotificationHandler(*this) {}

        ~UserSettingsDelegate() {
            if (mUserSettings != nullptr) {
                mUserSettings->Release();
                mUserSettings = nullptr;
            }
        }

        bool HandleSubscription(const string &event, const bool listen) {
            if (listen) {
                if (mShell != nullptr) {
                    mUserSettings = mShell->QueryInterfaceByCallsign<Exchange::IUserSettings>(USERSETTINGS_CALLSIGN);
                    if (mUserSettings == nullptr) {
                        LOGERR("mUserSettings is null exiting");
                        return false;
                    }
                } else {
                    LOGERR("mShell is null exiting");
                    return false;
                }

                if (mUserSettings == nullptr) {
                    LOGERR("mUserSettings interface not available");
                    return false;
                }
                AddNotification(event);

                if (!mNotificationHandler.GetRegistered()) {
                    LOGINFO("Registering for UserSettings notifications");
                    mUserSettings->Register(&mNotificationHandler);
                    mNotificationHandler.SetRegistered(true);
                    return true;
                } else {
                    LOGTRACE(" Is UserSettings registered = %s", mNotificationHandler.GetRegistered() ? "true" : "false");
                }
                
            } else {
                // Not removing the notification subscription for cases where only one event is removed 
                // Registration is lazy one but we need to evaluate if there is any value in unregistering
                // given these API calls are always made
                RemoveNotification(event);
            }
            return false;
        }

        bool HandleEvent(const string &event, const bool listen, bool &registrationError) {
            LOGDBG("Checking for handle event");
            // Check if event is present in VALID_USER_SETTINGS_EVENT make check case insensitive
            if (VALID_USER_SETTINGS_EVENT.find(StringUtils::toLower(event)) != VALID_USER_SETTINGS_EVENT.end()) {
                // Handle TextToSpeech event
                registrationError = HandleSubscription(event, listen);
                return true;
            }
            return false;
        }
    private:
        class UserSettingsNotificationHandler: public Exchange::IUserSettings::INotification {
            public:
                 UserSettingsNotificationHandler(UserSettingsDelegate& parent) : mParent(parent),registered(false){}
                ~UserSettingsNotificationHandler(){}

        void OnAudioDescriptionChanged(const bool enabled) {
            mParent.Dispatch( "accessibility.onaudiodescriptionsettingschanged", ObjectUtils::CreateBooleanJsonString("enabled", enabled));
        }

        void OnPreferredAudioLanguagesChanged(const string& preferredLanguages) {
            mParent.Dispatch( "localization.onpreferredaudiolanguageschanged", preferredLanguages);
        }

        void OnPresentationLanguageChanged(const string& presentationLanguage) {
            
            mParent.Dispatch( "localization.onlocalechanged", presentationLanguage);

            // check presentationLanguage is a delimitted string like "en-US"
            // add logic to get the "en" if the value is "en-US"
            if (presentationLanguage.find('-') != string::npos) {
                string language = presentationLanguage.substr(0, presentationLanguage.find('-'));
                mParent.Dispatch( "localization.onlanguagechanged", std::move(language));
            } else {
                LOGWARN("invalid value=%s set it must be a delimited string like en-US", presentationLanguage.c_str());
            }
        }

        void OnCaptionsChanged(const bool enabled) {
            mParent.Dispatch( "accessibility.onclosedcaptionssettingschanged", ObjectUtils::CreateBooleanJsonString("enabled", enabled));
        }

        void OnPreferredCaptionsLanguagesChanged(const string& preferredLanguages) {
            mParent.Dispatch( "closedcaptions.onpreferredlanguageschanged", preferredLanguages);
        }

        void OnPreferredClosedCaptionServiceChanged(const string& service) {
            mParent.Dispatch( "OnPreferredClosedCaptionServiceChanged", service);
        }

        void OnPrivacyModeChanged(const string& privacyMode) {
            mParent.Dispatch( "OnPrivacyModeChanged", privacyMode);
        }

        void OnPinControlChanged(const bool pinControl) {
            mParent.Dispatch( "OnPinControlChanged", ObjectUtils::BoolToJsonString(pinControl));
        }

        void OnViewingRestrictionsChanged(const string& viewingRestrictions) {
            mParent.Dispatch( "OnViewingRestrictionsChanged", viewingRestrictions);
        }

        void OnViewingRestrictionsWindowChanged(const string& viewingRestrictionsWindow) {
            mParent.Dispatch( "OnViewingRestrictionsWindowChanged", viewingRestrictionsWindow);
        }

        void OnLiveWatershedChanged(const bool liveWatershed) {
            mParent.Dispatch( "OnLiveWatershedChanged", ObjectUtils::BoolToJsonString(liveWatershed));
        }

        void OnPlaybackWatershedChanged(const bool playbackWatershed) {
            mParent.Dispatch( "OnPlaybackWatershedChanged", ObjectUtils::BoolToJsonString(playbackWatershed));
        }

        void OnBlockNotRatedContentChanged(const bool blockNotRatedContent) {
            mParent.Dispatch( "OnBlockNotRatedContentChanged", ObjectUtils::BoolToJsonString(blockNotRatedContent));
        }

        void OnPinOnPurchaseChanged(const bool pinOnPurchase) {
            mParent.Dispatch( "OnPinOnPurchaseChanged", ObjectUtils::BoolToJsonString(pinOnPurchase));
        }

        void OnHighContrastChanged(const bool enabled) {
            mParent.Dispatch( "accessibility.onhighcontrastuichanged", ObjectUtils::BoolToJsonString(enabled));
        }

        void OnVoiceGuidanceChanged(const bool enabled) {
            mParent.Dispatch( "accessibility.onvoiceguidancesettingschanged", ObjectUtils::CreateBooleanJsonString("enabled", enabled));
        }

        void OnVoiceGuidanceRateChanged(const double rate) {
            mParent.Dispatch( "OnVoiceGuidanceRateChanged", std::to_string(rate));
        }

        void OnVoiceGuidanceHintsChanged(const bool hints) {
            mParent.Dispatch( "OnVoiceGuidanceHintsChanged", std::to_string(hints));
        }

        void OnContentPinChanged(const string& contentPin) {
            mParent.Dispatch( "OnContentPinChanged", contentPin);
        }
                
                // New Method for Set registered
                void SetRegistered(bool state) {
                    std::lock_guard<std::mutex> lock(registerMutex);
                    registered = state;
                }

                // New Method for get registered
                bool GetRegistered() {
                    std::lock_guard<std::mutex> lock(registerMutex);
                    return registered;
                }

                BEGIN_INTERFACE_MAP(NotificationHandler)
                INTERFACE_ENTRY(Exchange::IUserSettings::INotification)
                END_INTERFACE_MAP
            private:
                    UserSettingsDelegate& mParent;
                    bool registered;
                    std::mutex registerMutex;

        };
        Exchange::IUserSettings *mUserSettings;
        PluginHost::IShell* mShell;
        Core::Sink<UserSettingsNotificationHandler> mNotificationHandler;

        
};
#endif