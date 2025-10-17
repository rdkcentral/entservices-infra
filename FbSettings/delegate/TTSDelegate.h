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
#ifndef __TTSDELEGATE_H__
#define __TTSDELEGATE_H__
#include "StringUtils.h"
#include "BaseEventDelegate.h"
#include <interfaces/ITextToSpeech.h>
#include "UtilsLogging.h"
using namespace WPEFramework;
#define TTS_CALLSIGN "org.rdk.TextToSpeech"
#define APP_API_METHOD_PREFIX "TextToSpeech."
class TTSDelegate : public BaseEventDelegate{
    public:
        TTSDelegate(PluginHost::IShell* shell, Exchange::IAppNotifications* appNotifications):
            BaseEventDelegate(appNotifications), mTextToSpeech(nullptr), mShell(shell), mNotificationHandler(*this){}

        ~TTSDelegate() {
            if (mTextToSpeech != nullptr) {
                mTextToSpeech->Release();
                mTextToSpeech = nullptr;
            }
        }

        bool HandleSubscription(const string &event, const bool listen) {
            if (listen) {
                if (mShell != nullptr) {
                    mTextToSpeech = mShell->QueryInterfaceByCallsign<Exchange::ITextToSpeech>(TTS_CALLSIGN);
                    if (mTextToSpeech == nullptr) {
                        LOGERR("mTextToSpeech is null exiting");
                        return false;
                    }
                } else {
                    LOGERR("mShell is null exiting");
                    return false;
                }

                if (mTextToSpeech == nullptr) {
                    LOGERR("mTextToSpeech interface not available");
                    return false;
                }

                if (!mNotificationHandler.GetRegistered()) {
                    LOGINFO("Registering for TextToSpeech notifications");
                    mTextToSpeech->Register(&mNotificationHandler);
                    mNotificationHandler.SetRegistered(true);
                } else {
                    LOGTRACE(" Is TTS registered = %s", mNotificationHandler.GetRegistered() ? "true" : "false");
                }

                AddNotification(event);
            } else {
                // Not removing the notification subscription for cases where one event is removed 
                RemoveNotification(event);
            }
            return false;
        }

        bool HandleEvent(const string &event, const bool listen, bool &registrationError) {
            LOGDBG("Checking for handle event");
            // Check if event starts with "TextToSpeech" make check case insensitive
            if (StringUtils::rfindInsensitive(event, APP_API_METHOD_PREFIX)) {
                // Handle TextToSpeech event
                registrationError = HandleSubscription(event, listen);
                return true;
            }
            return false;
        }
    private:
        class TTSNotificationHandler: public Exchange::ITextToSpeech::INotification {
            public:
                 TTSNotificationHandler(TTSDelegate& parent) : mParent(parent),registered(false){}
                ~TTSNotificationHandler(){}

                void Enabled(const bool state) {
                    mParent.Dispatch( "TextToSpeech.onEnabled", state ? "true" : "false");
                }
                void VoiceChanged(const string voice) {
                    mParent.Dispatch( "TextToSpeech.onVoiceChanged", std::move(voice));
                }
                void WillSpeak(const uint32_t speechid) {
                    mParent.Dispatch( "TextToSpeech.onWillSpeak", to_string(speechid));
                }
                void SpeechStart(const uint32_t speechid) {
                    mParent.Dispatch( "TextToSpeech.onSpeechStart", to_string(speechid));
                }
                void SpeechPause(const uint32_t speechid) {
                    mParent.Dispatch( "TextToSpeech.onSpeechPause", to_string(speechid));
                }
                void SpeechResume(const uint32_t speechid) {
                    mParent.Dispatch( "TextToSpeech.onSpeechResume", to_string(speechid));
                }
                void SpeechInterrupted(const uint32_t speechid) {
                    mParent.Dispatch( "TextToSpeech.onSpeechInterrupted", to_string(speechid));
                }
                void NetworkError(const uint32_t speechid) {
                    mParent.Dispatch( "TextToSpeech.onNetworkError", to_string(speechid));
                }
                void PlaybackError(const uint32_t speechid) {
                    mParent.Dispatch( "TextToSpeech.onPlaybackError", to_string(speechid));
                }
                void SpeechComplete(const uint32_t speechid) {
                    mParent.Dispatch( "TextToSpeech.onSpeechComplete", to_string(speechid));
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
                INTERFACE_ENTRY(Exchange::ITextToSpeech::INotification)
                END_INTERFACE_MAP
            private:
                    TTSDelegate& mParent;
                    bool registered;
                    std::mutex registerMutex;

        };
        Exchange::ITextToSpeech *mTextToSpeech;
        PluginHost::IShell* mShell;
        Core::Sink<TTSNotificationHandler> mNotificationHandler;

        
};
#endif