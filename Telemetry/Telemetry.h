/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2019 RDK Management
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

#pragma once

#include "Module.h"
#include <interfaces/IPowerManager.h>
#include "PowerManagerInterface.h"

using namespace WPEFramework;
using PowerState = WPEFramework::Exchange::IPowerManager::PowerState;
using ThermalTemperature = WPEFramework::Exchange::IPowerManager::ThermalTemperature;

namespace WPEFramework {

    namespace Plugin {

        // This is a server for a JSONRPC communication channel.
        // For a plugin to be capable to handle JSONRPC, inherit from PluginHost::JSONRPC.
        // By inheriting from this class, the plugin realizes the interface PluginHost::IDispatcher.
        // This realization of this interface implements, by default, the following methods on this plugin
        // - exists
        // - register
        // - unregister
        // Any other methood to be handled by this plugin  can be added can be added by using the
        // templated methods Register on the PluginHost::JSONRPC class.
        // As the registration/unregistration of notifications is realized by the class PluginHost::JSONRPC,
        // this class exposes a public method called, Notify(), using this methods, all subscribed clients
        // will receive a JSONRPC message as a notification, in case this method is called.
        class Telemetry : public PluginHost::IPlugin, public PluginHost::JSONRPC {
        private:
            class PowerManagerNotification : public Exchange::IPowerManager::INotification {
            private:
                PowerManagerNotification(const PowerManagerNotification&) = delete;
                PowerManagerNotification& operator=(const PowerManagerNotification&) = delete;
            
            public:
                explicit PowerManagerNotification(Telemetry& parent)
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
                Telemetry& _parent;
            };

            // We do not allow this plugin to be copied !!
            Telemetry(const Telemetry&) = delete;
            Telemetry& operator=(const Telemetry&) = delete;

            //Begin methods
            uint32_t setReportProfileStatus(const JsonObject& parameters, JsonObject& response);
            uint32_t logApplicationEvent(const JsonObject& parameters, JsonObject& response);
            uint32_t uploadReport(const JsonObject& parameters, JsonObject& response);
            uint32_t abortReport(const JsonObject& parameters, JsonObject& response);
            void InitializePowerManager();
            //End methods

        public:
            Telemetry();
            virtual ~Telemetry();
            virtual const string Initialize(PluginHost::IShell* service) override;
            virtual void Deinitialize(PluginHost::IShell* service) override;
            virtual string Information() const override { return {}; }
            void onPowerModeChanged(const PowerState &currentState, const PowerState &newState);
            void registerEventHandlers();

            uint32_t UploadReport();
            uint32_t AbortReport();

            BEGIN_INTERFACE_MAP(Telemetry)
            INTERFACE_ENTRY(PluginHost::IPlugin)
            INTERFACE_ENTRY(PluginHost::IDispatcher)
            END_INTERFACE_MAP

#ifdef HAS_RBUS
            void onReportUploadStatus(const char* status);
            void notifyT2PrivacyMode(std::string privacyMode);
#endif
        public:
            static Telemetry* _instance;
            enum Event
            {
                TELEMETRY_EVENT_UPLOADREPORT,
                TELEMETRY_EVENT_ABORTREPORT
            };

        class EXTERNAL Job : public Core::IDispatch {
        protected:
            Job(Telemetry* telemetry, Event event)
                : _telemetry(telemetry)
                , _event(event) {
                if (_telemetry != nullptr) {
                    _telemetry->AddRef();
                }
            }

       public:
            Job() = delete;
            Job(const Job&) = delete;
            Job& operator=(const Job&) = delete;
            ~Job() {
                if (_telemetry != nullptr) {
                    _telemetry->Release();
                }
            }

       public:
            static Core::ProxyType<Core::IDispatch> Create(Telemetry* Instance, Event event ) {
#ifndef USE_THUNDER_R4
                return (Core::proxy_cast<Core::IDispatch>(Core::ProxyType<Job>::Create(Instance, event)));
#else
                return (Core::ProxyType<Core::IDispatch>(Core::ProxyType<Job>::Create(Instance, event)));
#endif
            }

            virtual void Dispatch() {
                _telemetry->Dispatch(_event);
            }
        private:
            Telemetry *_telemetry;
            const Event _event;
        };
        private:

            void Dispatch(Event event);
            friend class Job;

            std::shared_ptr<WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>> m_systemServiceConnection;
            PowerManagerInterfaceRef _powerManagerPlugin;
            Core::Sink<PowerManagerNotification> _pwrMgrNotification;
            bool _registeredEventHandlers;
            PluginHost::IShell* m_service;
        };
    } // namespace Plugin
} // namespace WPEFramework
