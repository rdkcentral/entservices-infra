/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2025 RDK Management
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
*/

#pragma once

#include "Module.h"
#include <interfaces/Ids.h>
#include <interfaces/ITelemetry.h>
#include <interfaces/IConfiguration.h>
#include <interfaces/IPowerManager.h>
#include "PowerManagerInterface.h"

#include <com/com.h>
#include <core/core.h>

using namespace WPEFramework;
using PowerState = WPEFramework::Exchange::IPowerManager::PowerState;
using ThermalTemperature = WPEFramework::Exchange::IPowerManager::ThermalTemperature;

namespace WPEFramework {
namespace Plugin {
    class TelemetryImplementation : public Exchange::ITelemetry, public Exchange::IConfiguration
    {
        private:
            class PowerManagerNotification : public Exchange::IPowerManager::INotification {
            private:
                PowerManagerNotification(const PowerManagerNotification&) = delete;
                PowerManagerNotification& operator=(const PowerManagerNotification&) = delete;

            public:
                explicit PowerManagerNotification(TelemetryImplementation& parent)
                    : _parent(parent)
                {
                }
                ~PowerManagerNotification() override = default;

            public:
                void OnPowerModeChanged(const PowerState &currentState, const PowerState &newState) override
                {
                    _parent.onPowerModeChanged(currentState, newState);
                }
                void OnPowerModePreChange(const PowerState &currentState, const PowerState &newState) override {}
                void OnDeepSleepTimeout(const int &wakeupTimeout) override {}
                void OnNetworkStandbyModeChanged(const bool &enabled) override {}
                void OnThermalModeChanged(const ThermalTemperature &currentThermalLevel, const ThermalTemperature &newThermalLevel, const float &currentTemperature) override {}
                void OnRebootBegin(const string &rebootReasonCustom, const string &rebootReasonOther, const string &rebootRequestor) override {}

                BEGIN_INTERFACE_MAP(PowerManagerNotification)
                INTERFACE_ENTRY(Exchange::IPowerManager::INotification)
                END_INTERFACE_MAP

            private:
                TelemetryImplementation& _parent;
            };
    public:
        // We do not allow this plugin to be copied !!
        TelemetryImplementation();
        ~TelemetryImplementation() override;

        static TelemetryImplementation* instance(TelemetryImplementation *TelemetryImpl = nullptr);

        // We do not allow this plugin to be copied !!
        TelemetryImplementation(const TelemetryImplementation&) = delete;
        TelemetryImplementation& operator=(const TelemetryImplementation&) = delete;

        BEGIN_INTERFACE_MAP(TelemetryImplementation)
        INTERFACE_ENTRY(Exchange::ITelemetry)
	INTERFACE_ENTRY(Exchange::IConfiguration)
        END_INTERFACE_MAP

    public:
        enum Event
        {
            ON_REPORT_UPLOAD
        };
        class EXTERNAL Job : public Core::IDispatch {
        protected:
            Job(TelemetryImplementation* TelemetryImplementation, Event event, JsonValue &params)
                : _telemetryImplementation(TelemetryImplementation)
                , _event(event)
                , _params(params) {
                if (_telemetryImplementation != nullptr) {
                    _telemetryImplementation->AddRef();
                }
            }

       public:
            Job() = delete;
            Job(const Job&) = delete;
            Job& operator=(const Job&) = delete;
            ~Job() {
                if (_telemetryImplementation != nullptr) {
                    _telemetryImplementation->Release();
                }
            }

       public:
            static Core::ProxyType<Core::IDispatch> Create(TelemetryImplementation* telemetryImplementation, Event event, JsonValue  params ) {
#ifndef USE_THUNDER_R4
                return (Core::proxy_cast<Core::IDispatch>(Core::ProxyType<Job>::Create(telemetryImplementation, event, params)));
#else
                return (Core::ProxyType<Core::IDispatch>(Core::ProxyType<Job>::Create(telemetryImplementation, event, params)));
#endif
            }

            virtual void Dispatch() {
                _telemetryImplementation->Dispatch(_event, _params);
            }
            
        private:
            TelemetryImplementation *_telemetryImplementation;
            const Event _event;
            JsonValue _params;
        };
    public:
        virtual Core::hresult Register(Exchange::ITelemetry::INotification *notification ) override ;
        virtual Core::hresult Unregister(Exchange::ITelemetry::INotification *notification ) override;

        virtual Core::hresult SetReportProfileStatus(const string& status) override;
        virtual Core::hresult LogApplicationEvent(const string& eventName, const string& eventValue) override;
        virtual Core::hresult UploadReport() override;
        virtual Core::hresult AbortReport() override;

        void InitializePowerManager();
        void onPowerModeChanged(const PowerState &currentState, const PowerState &newState);
        void registerEventHandlers();
        
        // IConfiguration interface
        uint32_t Configure(PluginHost::IShell* service) override;
        
#ifdef HAS_RBUS
        void notifyT2PrivacyMode(std::string privacyMode);
        void onReportUploadStatus(const char* status);
        void activateSystemPluginandGetPrivacyMode();
        void setRFCReportProfiles();
#endif

    private:
        std::shared_ptr<WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>> m_systemServiceConnection;
        mutable Core::CriticalSection _adminLock;
        Core::ProxyType<RPC::InvokeServerType<1, 0, 4>> _engine;
        Core::ProxyType<RPC::CommunicatorClient> _communicatorClient;
        PluginHost::IShell* _service;
        std::list<Exchange::ITelemetry::INotification*> _telemetryNotification;
        PowerManagerInterfaceRef _powerManagerPlugin;
        Core::Sink<PowerManagerNotification> _pwrMgrNotification;
        bool _registeredEventHandlers;
        
        void dispatchEvent(Event, const JsonValue &params);
        void Dispatch(Event event, const JsonValue params);
    public:
        static TelemetryImplementation* _instance;

        friend class Job;
    };
} // namespace Plugin
} // namespace WPEFramework
