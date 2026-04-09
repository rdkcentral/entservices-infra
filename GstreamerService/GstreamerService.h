/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
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
 */

#pragma once

#include "Module.h"
#include <interfaces/IGstreamerService.h>
#include <interfaces/json/JsonData_GstreamerService.h>
#include <interfaces/json/JGstreamerService.h>
#include "UtilsLogging.h"
#include "tracing/Logging.h"

namespace WPEFramework {
    namespace Plugin {

        class GstreamerService : public PluginHost::IPlugin, public PluginHost::JSONRPC {
        private:
            GstreamerService(const GstreamerService&) = delete;
            GstreamerService& operator=(const GstreamerService&) = delete;

            class Notification : public RPC::IRemoteConnection::INotification,
                                 public Exchange::IGstreamerService::INotification {
            private:
                Notification()                              = delete;
                Notification(const Notification&)           = delete;
                Notification& operator=(const Notification&) = delete;

            public:
                explicit Notification(GstreamerService* parent)
                    : _parent(*parent)
                {
                    ASSERT(parent != nullptr);
                }

                ~Notification() override = default;

                BEGIN_INTERFACE_MAP(Notification)
                INTERFACE_ENTRY(Exchange::IGstreamerService::INotification)
                INTERFACE_ENTRY(RPC::IRemoteConnection::INotification)
                END_INTERFACE_MAP

                // ----- IGstreamerService::INotification -----

                void OnPipelineStateChanged(const string& state) override
                {
                    LOGINFO("[EVENT] OnPipelineStateChanged: %s", state.c_str());
                    Exchange::JGstreamerService::Event::OnPipelineStateChanged(_parent, state);
                }

                void OnError(const string& errorMessage) override
                {
                    LOGINFO("[EVENT] OnError: %s", errorMessage.c_str());
                    Exchange::JGstreamerService::Event::OnError(_parent, errorMessage);
                }

                // ----- RPC::IRemoteConnection::INotification -----

                void Activated(RPC::IRemoteConnection* /* connection */) override
                {
                }

                void Deactivated(RPC::IRemoteConnection* connection) override
                {
                    if (_parent._connectionId == connection->Id()) {
                        _parent.Deactivated(connection);
                    }
                }

            private:
                GstreamerService& _parent;
            };

        public:
            GstreamerService();
            ~GstreamerService() override;

            BEGIN_INTERFACE_MAP(GstreamerService)
            INTERFACE_ENTRY(PluginHost::IPlugin)
            INTERFACE_ENTRY(PluginHost::IDispatcher)
            INTERFACE_AGGREGATE(Exchange::IGstreamerService, _gstreamerService)
            END_INTERFACE_MAP

            // ----- IPlugin -----
            const string Initialize(PluginHost::IShell* service) override;
            void         Deinitialize(PluginHost::IShell* service) override;
            string       Information() const override;

        private:
            void Deactivated(RPC::IRemoteConnection* connection);

        private:
            PluginHost::IShell*          _service{};
            uint32_t                     _connectionId{};
            Exchange::IGstreamerService* _gstreamerService{};
            Core::Sink<Notification>     _notification;
        };

    } // namespace Plugin
} // namespace WPEFramework
