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

namespace WPEFramework {

    namespace Plugin {

        // This is a server for a JSONRPC communication channel.
        // For a plugin to be capable to handle JSONRPC, inherit from PluginHost::JSONRPC.
        // By inheriting from this class, the plugin realizes the interface PluginHost::IDispatcher.
        // This realization of this interface implements, by default, the following methods on this plugin
        // - exists
        // - register
        // - unregister
        // Any other method to be handled by this plugin can be added by using the
        // templated methods Register on the PluginHost::JSONRPC class.
        // As the registration/unregistration of notifications is realized by the class PluginHost::JSONRPC,
        // this class exposes a public method called, Notify(), using this methods, all subscribed clients
        // will receive a JSONRPC message as a notification, in case this method is called.
        class GstreamerService : public PluginHost::IPlugin, public PluginHost::JSONRPC {
        private:
            // We do not allow this plugin to be copied !!
            GstreamerService(const GstreamerService&) = delete;
            GstreamerService& operator=(const GstreamerService&) = delete;

            // Notification handler for out-of-process plugin failures and event relay
            class Notification : public RPC::IRemoteConnection::INotification,
                                 public Exchange::IGstreamerService::INotification {
            public:
                explicit Notification(GstreamerService* parent)
                    : _parent(*parent)
                {
                    ASSERT(parent != nullptr);
                }

                ~Notification() override = default;

                Notification(Notification&&) = delete;
                Notification(const Notification&) = delete;
                Notification& operator=(Notification&&) = delete;
                Notification& operator=(const Notification&) = delete;

                BEGIN_INTERFACE_MAP(Notification)
                INTERFACE_ENTRY(RPC::IRemoteConnection::INotification)
                INTERFACE_ENTRY(Exchange::IGstreamerService::INotification)
                END_INTERFACE_MAP

                // RPC::IRemoteConnection::INotification
                void Activated(RPC::IRemoteConnection* /* connection */) override
                {
                }
                void Deactivated(RPC::IRemoteConnection* connection) override
                {
                    _parent.Deactivated(connection);
                }

                // Exchange::IGstreamerService::INotification - relay as JSON-RPC
                void OnPipelineStateChanged(const string& state) override
                {
                    Exchange::JGstreamerService::Event::OnPipelineStateChanged(_parent, state);
                }
                void OnError(const string& errorMessage) override
                {
                    Exchange::JGstreamerService::Event::OnError(_parent, errorMessage);
                }

            private:
                GstreamerService& _parent;
            };

        public:
            GstreamerService();
            virtual ~GstreamerService();
            virtual const string Initialize(PluginHost::IShell* shell) override;
            virtual void Deinitialize(PluginHost::IShell* service) override;
            virtual string Information() const override { return {}; }

            BEGIN_INTERFACE_MAP(GstreamerService)
            INTERFACE_ENTRY(PluginHost::IPlugin)
            INTERFACE_ENTRY(PluginHost::IDispatcher)
            INTERFACE_AGGREGATE(Exchange::IGstreamerService, _gstreamerService)
            END_INTERFACE_MAP

        private:
            void Deactivated(RPC::IRemoteConnection* connection);

        private:
            PluginHost::IShell* _service;
            Exchange::IGstreamerService* _gstreamerService;
            uint32_t _connectionId;
            Core::Sink<Notification> _notification;
        };
    } // namespace Plugin
} // namespace WPEFramework
