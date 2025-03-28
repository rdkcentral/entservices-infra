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
#include <interfaces/ITelemetry.h>
#include <interfaces/json/JTelemetry.h>
#include <interfaces/json/JsonData_Telemetry.h>
#include <interfaces/IConfiguration.h>
#include "UtilsLogging.h"
#include "tracing/Logging.h"

namespace WPEFramework 
{
    namespace Plugin
    {
        class Telemetry : public PluginHost::IPlugin, public PluginHost::JSONRPC 
        {
            private:
                class Notification : public RPC::IRemoteConnection::INotification, public Exchange::ITelemetry::INotification
                {
                    private:
                        Notification() = delete;
                        Notification(const Notification&) = delete;
                        Notification& operator=(const Notification&) = delete;

                    public:
                    explicit Notification(Telemetry* parent) 
                        : _parent(*parent)
                        {
                            ASSERT(parent != nullptr);
                        }

                        virtual ~Notification()
                        {
                        }

                        BEGIN_INTERFACE_MAP(Notification)
                        INTERFACE_ENTRY(Exchange::ITelemetry::INotification)
                        INTERFACE_ENTRY(RPC::IRemoteConnection::INotification)
                        END_INTERFACE_MAP

                        void Activated(RPC::IRemoteConnection*) override
                        {
                            LOGINFO("Telemetry Notification Activated");
                        }

                        void Deactivated(RPC::IRemoteConnection *connection) override
                        {
                            LOGINFO("Telemetry Notification Deactivated");
                            _parent.Deactivated(connection);
                        }

                        void OnReportUpload(const string& telemetryUploadStatus ) override
                        {
                            LOGINFO("OnReportUpload: telemetryUploadStatus %s\n", telemetryUploadStatus.c_str());
                            Exchange::JTelemetry::Event::OnReportUpload(_parent, telemetryUploadStatus);
                        }

                    private:
                        Telemetry& _parent;
                };

                public:
                    Telemetry(const Telemetry&) = delete;
                    Telemetry& operator=(const Telemetry&) = delete;

                    Telemetry();
                    virtual ~Telemetry();

                    BEGIN_INTERFACE_MAP(Telemetry)
                    INTERFACE_ENTRY(PluginHost::IPlugin)
                    INTERFACE_ENTRY(PluginHost::IDispatcher)
                    INTERFACE_AGGREGATE(Exchange::ITelemetry, _telemetry)
                    END_INTERFACE_MAP

                    //  IPlugin methods
                    // -------------------------------------------------------------------------------------------------------
                    const string Initialize(PluginHost::IShell* service) override;
                    void Deinitialize(PluginHost::IShell* service) override;
                    string Information() const override;

                private:
                    void Deactivated(RPC::IRemoteConnection* connection);

                private:
                    PluginHost::IShell* _service{};
                    uint32_t _connectionId{};
                    Exchange::ITelemetry* _telemetry{};
                    Core::Sink<Notification> _telemetryNotification;
		    Exchange::IConfiguration* configure;
       };
    } // namespace Plugin
} // namespace WPEFramework
