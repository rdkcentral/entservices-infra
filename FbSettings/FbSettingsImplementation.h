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
#pragma once

#include "Module.h"
#include <interfaces/IConfiguration.h>
#include <interfaces/IAppGateway.h>
#include <mutex>
#include <map>
#include "UtilsLogging.h"
#include "ThunderUtils.h"
#include "delegate/SettingsDelegate.h"

namespace WPEFramework
{
    namespace Plugin
    {
        class FbSettingsImplementation : public Exchange::IConfiguration, public Exchange::IAppNotificationHandlerInternal, public Exchange::IAppGatewayRequestHandler
        {
        private:
            FbSettingsImplementation(const FbSettingsImplementation &) = delete;
            FbSettingsImplementation &operator=(const FbSettingsImplementation &) = delete;

            class EXTERNAL EventRegistrationJob : public Core::IDispatch
            {
            protected:
                EventRegistrationJob(FbSettingsImplementation *parent,
                                     const string &event,
                                     const bool listen) : mParent(*parent), mEvent(event), mListen(listen)
                {
                }

            public:
                EventRegistrationJob() = delete;
                EventRegistrationJob(const EventRegistrationJob &) = delete;
                EventRegistrationJob &operator=(const EventRegistrationJob &) = delete;
                ~EventRegistrationJob()
                {
                }

                static Core::ProxyType<Core::IDispatch> Create(FbSettingsImplementation *parent,
                                                               const string &event, const bool listen)
                {
                    return (Core::ProxyType<Core::IDispatch>(Core::ProxyType<EventRegistrationJob>::Create(parent, event, listen)));
                }
                virtual void Dispatch()
                {
                    mParent.mDelegate->HandleAppEventNotifier(mEvent, mListen);
                }

            private:
                FbSettingsImplementation &mParent;
                const string mEvent;
                const bool mListen;
            };

        public:
            FbSettingsImplementation();
            ~FbSettingsImplementation();

            BEGIN_INTERFACE_MAP(FbSettingsImplementation)
            INTERFACE_ENTRY(Exchange::IConfiguration)
            INTERFACE_ENTRY(Exchange::IAppNotificationHandlerInternal)
            INTERFACE_ENTRY(Exchange::IAppGatewayRequestHandler)
            END_INTERFACE_MAP

            Core::hresult HandleAppEventNotifier(const string &event, const bool &listen, bool &status /* @out */) override;

            // IAppGatewayRequestHandler interface
            Core::hresult HandleAppGatewayRequest(const Exchange::Context &context /* @in */,
                                                  const string &method /* @in */,
                                                  const string &payload /* @in */,
                                                  string &result /* @out */) override;

            // IConfiguration interface
            uint32_t Configure(PluginHost::IShell *shell);

        private:
            // Helper methods for System/Device - called by HandleAppGatewayRequest
            Core::hresult GetDeviceMake(string &make /* @out */);
            Core::hresult GetDeviceName(string &name /* @out */);
            Core::hresult SetDeviceName(const string name /* @in */);
            Core::hresult GetDeviceSku(string &sku /* @out */);
            Core::hresult GetCountryCode(string &countryCode /* @out */);
            Core::hresult SetCountryCode(const string countryCode /* @in */);
            Core::hresult GetTimeZone(string &timeZone /* @out */);
            Core::hresult SetTimeZone(const string timeZone /* @in */);
            Core::hresult GetSecondScreenFriendlyName(string &name /* @out */);
            Core::hresult SetName(const string &value /* @in */, string &result /* @out */);
            Core::hresult AddAdditionalInfo(const string &value /* @in */, string &result /* @out */);

            // Helper methods for network status - called by HandleAppGatewayRequest
            Core::hresult GetInternetConnectionStatus(string &result /* @out */);

            // Helper methods for UserSettings - called by HandleAppGatewayRequest
            Core::hresult GetVoiceGuidance(string &result /* @out */);
            Core::hresult GetAudioDescription(string &result /* @out */);
            Core::hresult GetAudioDescriptionsEnabled(string &result /* @out */);
            Core::hresult GetHighContrast(string &result /* @out */);
            Core::hresult GetCaptions(string &result /* @out */);
            Core::hresult SetVoiceGuidance(const bool enabled /* @in */);
            Core::hresult SetAudioDescriptionsEnabled(const bool enabled /* @in */);
            Core::hresult SetCaptions(const bool enabled /* @in */);
            Core::hresult GetPresentationLanguage(string &result /* @out */);
            Core::hresult GetLocale(string &result /* @out */);
            Core::hresult SetLocale(const string &locale /* @in */);
            Core::hresult GetPreferredAudioLanguages(string &result /* @out */);
            Core::hresult GetPreferredCaptionsLanguages(string &result /* @out */);
            Core::hresult SetPreferredAudioLanguages(const string &languages /* @in */);
            Core::hresult SetPreferredCaptionsLanguages(const string &preferredLanguages /* @in */);
            Core::hresult SetSpeed(const double speed /* @in */);
            Core::hresult GetSpeed(double &speed /* @out */);
            Core::hresult GetVoiceGuidanceHints(string &result /* @out */);
            Core::hresult SetVoiceGuidanceHints(const bool enabled /* @in */);
            Core::hresult GetVoiceGuidanceSettings(string &result /* @out */);

            PluginHost::IShell *mShell;
            std::shared_ptr<SettingsDelegate> mDelegate;
        };
    }
}

