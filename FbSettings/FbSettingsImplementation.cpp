/*
 * Copyright 2023 Comcast Cable Communications Management, LLC
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
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "FbSettingsImplementation.h"
#include "UtilsLogging.h"
#include "delegate/SettingsDelegate.h"
#include <sstream>
#include <core/JSON.h>

using namespace WPEFramework::Core::JSON;

namespace WPEFramework
{
    namespace Plugin
    {

        SERVICE_REGISTRATION(FbSettingsImplementation, 1, 0);

        FbSettingsImplementation::FbSettingsImplementation() : mShell(nullptr)
        {
            mDelegate = std::make_shared<SettingsDelegate>();
        }

        FbSettingsImplementation::~FbSettingsImplementation()
        {
            // Cleanup resources if needed
            if (mShell != nullptr)
            {
                mShell->Release();
                mShell = nullptr;
            }
        }

        Core::hresult FbSettingsImplementation::HandleAppEventNotifier(const string &event /* @in */,
                                                                       const bool &listen /* @in */,
                                                                       bool &status /* @out */)
        {
            LOGINFO("HandleFireboltNotifier [event=%s listen=%s]",
                    event.c_str(), listen ? "true" : "false");
            status = true;
            Core::IWorkerPool::Instance().Submit(EventRegistrationJob::Create(this, event, listen));
            return Core::ERROR_NONE;
        }

        Core::hresult FbSettingsImplementation::SetName(const string &value /* @in */, string &result)
        {
            result = "null"; // TBA
            return Core::ERROR_NONE;
        }

        Core::hresult FbSettingsImplementation::AddAdditionalInfo(const string &value /* @in @opaque */, string &result)
        {
            result = "null"; // TBA
            return Core::ERROR_NONE;
        }
        // Delegated alias methods

        Core::hresult FbSettingsImplementation::GetDeviceMake(string &make)
        {
            LOGINFO("GetDeviceMake FbSettings");
            if (!mDelegate)
                return Core::ERROR_UNAVAILABLE;
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate)
                return Core::ERROR_UNAVAILABLE;
            return systemDelegate->GetDeviceMake(make);
        }

        Core::hresult FbSettingsImplementation::GetDeviceName(string &name)
        {
            if (!mDelegate)
                return Core::ERROR_UNAVAILABLE;
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate)
                return Core::ERROR_UNAVAILABLE;
            return systemDelegate->GetDeviceName(name);
        }

        Core::hresult FbSettingsImplementation::SetDeviceName(const string name)
        {
            if (!mDelegate)
                return Core::ERROR_UNAVAILABLE;
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate)
                return Core::ERROR_UNAVAILABLE;
            return systemDelegate->SetDeviceName(name);
        }

        Core::hresult FbSettingsImplementation::GetDeviceSku(string &sku)
        {
            if (!mDelegate)
                return Core::ERROR_UNAVAILABLE;
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate)
                return Core::ERROR_UNAVAILABLE;
            return systemDelegate->GetDeviceSku(sku);
        }

        Core::hresult FbSettingsImplementation::GetCountryCode(string &countryCode)
        {
            if (!mDelegate)
                return Core::ERROR_UNAVAILABLE;
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate)
                return Core::ERROR_UNAVAILABLE;
            return systemDelegate->GetCountryCode(countryCode);
        }

        Core::hresult FbSettingsImplementation::SetCountryCode(const string countryCode)
        {
            if (!mDelegate)
                return Core::ERROR_UNAVAILABLE;
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate)
                return Core::ERROR_UNAVAILABLE;
            return systemDelegate->SetCountryCode(countryCode);
        }

        Core::hresult FbSettingsImplementation::GetTimeZone(string &timeZone)
        {
            if (!mDelegate)
                return Core::ERROR_UNAVAILABLE;
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate)
                return Core::ERROR_UNAVAILABLE;
            return systemDelegate->GetTimeZone(timeZone);
        }

        Core::hresult FbSettingsImplementation::SetTimeZone(const string timeZone)
        {
            if (!mDelegate)
                return Core::ERROR_UNAVAILABLE;
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate)
                return Core::ERROR_UNAVAILABLE;
            return systemDelegate->SetTimeZone(timeZone);
        }

        Core::hresult FbSettingsImplementation::GetSecondScreenFriendlyName(string &name)
        {
            if (!mDelegate)
                return Core::ERROR_UNAVAILABLE;
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate)
                return Core::ERROR_UNAVAILABLE;
            return systemDelegate->GetSecondScreenFriendlyName(name);
        }

        // UserSettings APIs
        Core::hresult FbSettingsImplementation::GetVoiceGuidance(string &result)
        {
            if (!mDelegate)
            {
                result = "{\"error\":\"couldnt get voiceguidance state\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "{\"error\":\"couldnt get voiceguidance state\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->GetVoiceGuidance(result);
        }

        Core::hresult FbSettingsImplementation::GetAudioDescription(string &result)
        {
            if (!mDelegate)
            {
                result = "{\"error\":\"couldnt get audio description settings\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "{\"error\":\"couldnt get audio description settings\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->GetAudioDescription(result);
        }

        Core::hresult FbSettingsImplementation::GetAudioDescriptionsEnabled(string &result)
        {
            if (!mDelegate)
            {
                result = "{\"error\":\"couldnt get audio descriptions enabled\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "{\"error\":\"couldnt get audio descriptions enabled\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->GetAudioDescriptionsEnabled(result);
        }

        Core::hresult FbSettingsImplementation::GetHighContrast(string &result)
        {
            if (!mDelegate)
            {
                result = "{\"error\":\"couldnt get high contrast state\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "{\"error\":\"couldnt get high contrast state\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->GetHighContrast(result);
        }

        Core::hresult FbSettingsImplementation::GetCaptions(string &result)
        {
            if (!mDelegate)
            {
                result = "{\"error\":\"couldnt get captions state\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "{\"error\":\"couldnt get captions state\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->GetCaptions(result);
        }

        Core::hresult FbSettingsImplementation::GetPresentationLanguage(string &result)
        {
            if (!mDelegate)
            {
                result = "{\"error\":\"couldn't get language\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "{\"error\":\"couldn't get language\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->GetPresentationLanguage(result);
        }

        Core::hresult FbSettingsImplementation::GetLocale(string &result)
        {
            if (!mDelegate)
            {
                result = "{\"error\":\"couldn't get locale\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "{\"error\":\"couldn't get locale\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->GetLocale(result);
        }

        Core::hresult FbSettingsImplementation::SetLocale(const string &locale)
        {
            if (!mDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->SetLocale(locale);
        }

        Core::hresult FbSettingsImplementation::GetPreferredAudioLanguages(string &result)
        {
            if (!mDelegate)
            {
                result = "[]";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "[]";
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->GetPreferredAudioLanguages(result);
        }

        Core::hresult FbSettingsImplementation::GetPreferredCaptionsLanguages(string &result)
        {
            if (!mDelegate)
            {
                result = "[\"eng\"]";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "[\"eng\"]";
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->GetPreferredCaptionsLanguages(result);
        }

        Core::hresult FbSettingsImplementation::SetPreferredAudioLanguages(const string &languages)
        {
            if (!mDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->SetPreferredAudioLanguages(languages);
        }

        Core::hresult FbSettingsImplementation::SetPreferredCaptionsLanguages(const string &preferredLanguages)
        {
            if (!mDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->SetPreferredCaptionsLanguages(preferredLanguages);
        }

        Core::hresult FbSettingsImplementation::SetVoiceGuidance(const bool enabled)
        {
            if (!mDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->SetVoiceGuidance(enabled);
        }

        Core::hresult FbSettingsImplementation::SetAudioDescriptionsEnabled(const bool enabled)
        {
            if (!mDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->SetAudioDescriptionsEnabled(enabled);
        }

        Core::hresult FbSettingsImplementation::SetCaptions(const bool enabled)
        {
            if (!mDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->SetCaptions(enabled);
        }

        Core::hresult FbSettingsImplementation::SetSpeed(const double speed)
        {
            if (!mDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            // Transform the speed using vg_speed_firebolt2thunder function logic:
            // (if $speed == 2 then 10 elif $speed >= 1.67 then 1.38 elif $speed >= 1.33 then 1.19 elif $speed >= 1 then 1 else 0.1 end)
            double transformedRate;
            if (speed == 2.0)
            {
                transformedRate = 10.0;
            }
            else if (speed >= 1.67)
            {
                transformedRate = 1.38;
            }
            else if (speed >= 1.33)
            {
                transformedRate = 1.19;
            }
            else if (speed >= 1.0)
            {
                transformedRate = 1.0;
            }
            else
            {
                transformedRate = 0.1;
            }

            LOGINFO("SetSpeed: transforming speed %f to rate %f", speed, transformedRate);

            return userSettingsDelegate->SetVoiceGuidanceRate(transformedRate);
        }

        Core::hresult FbSettingsImplementation::GetSpeed(double &speed)
        {
            if (!mDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            double rate;
            Core::hresult result = userSettingsDelegate->GetVoiceGuidanceRate(rate);

            if (result != Core::ERROR_NONE)
            {
                LOGERR("Failed to get voice guidance rate");
                return result;
            }

            // Transform the rate using vg_speed_thunder2firebolt function logic:
            // (if $speed >= 1.56 then 2 elif $speed >= 1.38 then 1.67 elif $speed >= 1.19 then 1.33 elif $speed >= 1 then 1 else 0.5 end)
            if (rate >= 1.56)
            {
                speed = 2.0;
            }
            else if (rate >= 1.38)
            {
                speed = 1.67;
            }
            else if (rate >= 1.19)
            {
                speed = 1.33;
            }
            else if (rate >= 1.0)
            {
                speed = 1.0;
            }
            else
            {
                speed = 0.5;
            }

            LOGINFO("GetSpeed: transforming rate %f to speed %f", rate, speed);

            return Core::ERROR_NONE;
        }

        Core::hresult FbSettingsImplementation::GetVoiceGuidanceHints(string &result)
        {
            if (!mDelegate)
            {
                result = "{\"error\":\"couldnt get navigationHints\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "{\"error\":\"couldnt get navigationHints\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->GetVoiceGuidanceHints(result);
        }

        Core::hresult FbSettingsImplementation::SetVoiceGuidanceHints(const bool enabled)
        {
            if (!mDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->SetVoiceGuidanceHints(enabled);
        }

        Core::hresult FbSettingsImplementation::GetVoiceGuidanceSettings(string &result)
        {
            if (!mDelegate)
            {
                result = "{\"error\":\"couldn't get voice guidance settings\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "{\"error\":\"couldn't get voice guidance settings\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            // Get voice guidance enabled state
            string enabledResult;
            Core::hresult enabledStatus = userSettingsDelegate->GetVoiceGuidance(enabledResult);
            if (enabledStatus != Core::ERROR_NONE)
            {
                result = "{\"error\":\"couldn't get voiceguidance enabled state\"}";
                return enabledStatus;
            }

            // Get voice guidance rate (speed)
            double rate;
            Core::hresult rateStatus = userSettingsDelegate->GetVoiceGuidanceRate(rate);
            if (rateStatus != Core::ERROR_NONE)
            {
                result = "{\"error\":\"couldn't get voiceguidance rate\"}";
                return rateStatus;
            }

            // Get navigation hints
            string hintsResult;
            Core::hresult hintsStatus = userSettingsDelegate->GetVoiceGuidanceHints(hintsResult);
            if (hintsStatus != Core::ERROR_NONE)
            {
                result = "{\"error\":\"couldn't get voiceguidance hints\"}";
                return hintsStatus;
            }

            // Construct the combined JSON response
            // Format: {"enabled": <bool>, "speed": <rate>, "rate": <rate>, "navigationHints": <bool>}
            std::ostringstream jsonStream;
            jsonStream << "{\"enabled\": " << enabledResult
                       << ", \"speed\": " << rate
                       << ", \"rate\": " << rate
                       << ", \"navigationHints\": " << hintsResult << "}";

            result = jsonStream.str();
            LOGINFO("GetVoiceGuidanceSettings: %s", result.c_str());

            return Core::ERROR_NONE;
        }

        Core::hresult FbSettingsImplementation::GetInternetConnectionStatus(string &result)
        {
            if (!mDelegate)
            {
                result = "{\"error\":\"couldn't get internet connection status\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            auto networkDelegate = mDelegate->getNetworkDelegate();
            if (!networkDelegate)
            {
                result = "{\"error\":\"couldn't get internet connection status\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            return networkDelegate->GetInternetConnectionStatus(result);
        }

        Core::hresult FbSettingsImplementation::HandleAppGatewayRequest(
            const Exchange::Context &context,
            const string &method,
            const string &payload,
            string &result)
        {
            LOGINFO("HandleAppGatewayRequest: method=%s, payload=%s, appId=%s",
                    method.c_str(), payload.c_str(), context.appId.c_str());

            // Route System/Device methods
            if (method == "device.make")
            {
                return GetDeviceMake(result);
            }
            else if (method == "device.name")
            {
                return GetDeviceName(result);
            }
            else if (method == "device.setName")
            {
                JsonObject params;
                if (params.FromString(payload))
                {
                    string name = params.Get("value").String();
                    return SetDeviceName(name);
                }
                result = "{\"error\":\"Invalid payload\"}";
                return Core::ERROR_BAD_REQUEST;
            }
            else if (method == "device.sku")
            {
                return GetDeviceSku(result);
            }
            else if (method == "localization.countryCode")
            {
                return GetCountryCode(result);
            }
            else if (method == "localization.setCountryCode")
            {
                JsonObject params;
                if (params.FromString(payload))
                {
                    string countryCode = params.Get("value").String();
                    return SetCountryCode(countryCode);
                }
                result = "{\"error\":\"Invalid payload\"}";
                return Core::ERROR_BAD_REQUEST;
            }
            else if (method == "localization.timeZone")
            {
                return GetTimeZone(result);
            }
            else if (method == "localization.setTimeZone")
            {
                JsonObject params;
                if (params.FromString(payload))
                {
                    string timeZone = params.Get("value").String();
                    return SetTimeZone(timeZone);
                }
                result = "{\"error\":\"Invalid payload\"}";
                return Core::ERROR_BAD_REQUEST;
            }
            else if (method == "secondscreen.friendlyName")
            {
                return GetSecondScreenFriendlyName(result);
            }
            else if (method == "Localization.addAdditionalInfo")
            {
                return AddAdditionalInfo(payload, result);
            }

            // Route network-related methods
            else if (method == "device.network")
            {
                return GetInternetConnectionStatus(result);
            }

            // Route voice guidance methods
            else if (method == "voiceguidance.enabled")
            {
                return GetVoiceGuidance(result);
            }
            else if (method == "voiceguidance.setEnabled")
            {
                // Parse payload for boolean value
                JsonObject params;
                if (params.FromString(payload))
                {
                    bool enabled = params.Get("value").Boolean();
                    return SetVoiceGuidance(enabled);
                }
                result = "{\"error\":\"Invalid payload\"}";
                return Core::ERROR_BAD_REQUEST;
            }
            else if (method == "voiceguidance.speed" || method == "voiceguidance.rate")
            {
                double speed;
                Core::hresult status = GetSpeed(speed);
                if (status == Core::ERROR_NONE)
                {
                    std::ostringstream jsonStream;
                    jsonStream << speed;
                    result = jsonStream.str();
                }
                return status;
            }
            else if (method == "voiceguidance.setSpeed" || method == "voiceguidance.setRate")
            {
                JsonObject params;
                if (params.FromString(payload))
                {
                    double speed = params.Get("value").Number();
                    return SetSpeed(speed);
                }
                result = "{\"error\":\"Invalid payload\"}";
                return Core::ERROR_BAD_REQUEST;
            }
            else if (method == "voiceguidance.navigationHints")
            {
                return GetVoiceGuidanceHints(result);
            }
            else if (method == "voiceguidance.setNavigationHints")
            {
                JsonObject params;
                if (params.FromString(payload))
                {
                    bool enabled = params.Get("value").Boolean();
                    return SetVoiceGuidanceHints(enabled);
                }
                result = "{\"error\":\"Invalid payload\"}";
                return Core::ERROR_BAD_REQUEST;
            }
            else if (method == "accessibility.voiceGuidanceSettings")
            {
                return GetVoiceGuidanceSettings(result);
            }

            // Route audio description methods
            else if (method == "accessibility.audioDescriptionSettings")
            {
                return GetAudioDescription(result);
            }
            else if (method == "audiodescriptions.enabled")
            {
                return GetAudioDescriptionsEnabled(result);
            }
            else if (method == "audiodescriptions.setEnabled")
            {
                JsonObject params;
                if (params.FromString(payload))
                {
                    bool enabled = params.Get("value").Boolean();
                    return SetAudioDescriptionsEnabled(enabled);
                }
                result = "{\"error\":\"Invalid payload\"}";
                return Core::ERROR_BAD_REQUEST;
            }

            // Route accessibility methods
            else if (method == "accessibility.highContrastUI")
            {
                return GetHighContrast(result);
            }

            // Route closed captions methods
            else if (method == "closedcaptions.enabled")
            {
                return GetCaptions(result);
            }
            else if (method == "closedcaptions.setEnabled")
            {
                JsonObject params;
                if (params.FromString(payload))
                {
                    bool enabled = params.Get("value").Boolean();
                    return SetCaptions(enabled);
                }
                result = "{\"error\":\"Invalid payload\"}";
                return Core::ERROR_BAD_REQUEST;
            }
            else if (method == "closedcaptions.preferredLanguages")
            {
                return GetPreferredCaptionsLanguages(result);
            }
            else if (method == "closedcaptions.setPreferredLanguages")
            {
                JsonObject params;
                if (params.FromString(payload))
                {
                    string languages = params.Get("value").String();
                    return SetPreferredCaptionsLanguages(languages);
                }
                result = "{\"error\":\"Invalid payload\"}";
                return Core::ERROR_BAD_REQUEST;
            }

            // Route localization methods
            else if (method == "localization.language")
            {
                return GetPresentationLanguage(result);
            }
            else if (method == "localization.locale")
            {
                return GetLocale(result);
            }
            else if (method == "localization.setLocale")
            {
                JsonObject params;
                if (params.FromString(payload))
                {
                    string locale = params.Get("value").String();
                    return SetLocale(locale);
                }
                result = "{\"error\":\"Invalid payload\"}";
                return Core::ERROR_BAD_REQUEST;
            }
            else if (method == "localization.preferredAudioLanguages")
            {
                return GetPreferredAudioLanguages(result);
            }
            else if (method == "localization.setPreferredAudioLanguages")
            {
                JsonObject params;
                if (params.FromString(payload))
                {
                    string languages = params.Get("value").String();
                    return SetPreferredAudioLanguages(languages);
                }
                result = "{\"error\":\"Invalid payload\"}";
                return Core::ERROR_BAD_REQUEST;
            }

            // If method not found, return error
            result = "{\"error\":\"Method not found\"}";
            LOGERR("Unsupported method: %s", method.c_str());
            return Core::ERROR_UNKNOWN_KEY;
        }

        uint32_t FbSettingsImplementation::Configure(PluginHost::IShell *shell)
        {
            LOGINFO("Configuring FbSettings");
            uint32_t result = Core::ERROR_NONE;
            ASSERT(shell != nullptr);
            mShell = shell;
            mShell->AddRef();
            mDelegate->setShell(mShell);
            return result;
        }
    }
}

