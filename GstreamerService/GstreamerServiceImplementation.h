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
#include <interfaces/IConfiguration.h>
#include <mutex>
#include <gst/gst.h>

namespace WPEFramework {
namespace Plugin {

    class GstreamerServiceImplementation 
        : public Exchange::IGstreamerService
        , public Exchange::IConfiguration {
    private:
        GstreamerServiceImplementation(const GstreamerServiceImplementation&) = delete;
        GstreamerServiceImplementation& operator=(const GstreamerServiceImplementation&) = delete;

        // Notification dispatcher for sending events
        class NotificationDispatcher {
        public:
            NotificationDispatcher() : _adminLock(), _observers() {}
            ~NotificationDispatcher() = default;

            void Register(Exchange::IGstreamerService::INotification* notification)
            {
                _adminLock.Lock();

                ASSERT(std::find(_observers.begin(), _observers.end(), notification) == _observers.end());

                notification->AddRef();
                _observers.push_back(notification);

                _adminLock.Unlock();
            }

            void Unregister(Exchange::IGstreamerService::INotification* notification)
            {
                _adminLock.Lock();

                auto index = std::find(_observers.begin(), _observers.end(), notification);

                ASSERT(index != _observers.end());

                if (index != _observers.end()) {
                    (*index)->Release();
                    _observers.erase(index);
                }

                _adminLock.Unlock();
            }

            void NotifyPipelineStateChanged(const string& state)
            {
                _adminLock.Lock();

                for (auto* observer : _observers) {
                    observer->OnPipelineStateChanged(state);
                }

                _adminLock.Unlock();
            }

            void NotifyError(const string& errorMessage)
            {
                _adminLock.Lock();

                for (auto* observer : _observers) {
                    observer->OnError(errorMessage);
                }

                _adminLock.Unlock();
            }

        private:
            Core::CriticalSection _adminLock;
            std::list<Exchange::IGstreamerService::INotification*> _observers;
        };

    public:
        GstreamerServiceImplementation();
        ~GstreamerServiceImplementation() override;

        BEGIN_INTERFACE_MAP(GstreamerServiceImplementation)
        INTERFACE_ENTRY(Exchange::IGstreamerService)
        INTERFACE_ENTRY(Exchange::IConfiguration)
        END_INTERFACE_MAP

        // IGstreamerService interface
        Core::hresult StartPipeline(const string& pipelineConfig) override;
        Core::hresult StopPipeline() override;
        Core::hresult GetPipelineStatus(string& status /* @out */) const override;
        Core::hresult PlayPause() override;
        Core::hresult Seek(const int64_t offset) override;
        Core::hresult GetPosition(int64_t& position /* @out */, int64_t& duration /* @out*/) const override;
        Core::hresult SetWindowVisible(const bool visible) override;
        // Notification registration/unregistration
        Core::hresult Register(Exchange::IGstreamerService::INotification* notification) override;
        Core::hresult Unregister(Exchange::IGstreamerService::INotification* notification) override;

        // IConfiguration interface
        uint32_t Configure(PluginHost::IShell* shell) override;

    private:
        // GStreamer callback for bus messages
        static gboolean BusCallback(GstBus* bus, GstMessage* msg, gpointer data);

        // Helper methods
        void CleanupPipeline();
        string GetGstStateAsString(GstState state) const;

    private:
        mutable Core::CriticalSection _adminLock;
        PluginHost::IShell* _shell;
        GstElement* _pipeline;
        GstBus* _bus;
        guint _busWatchId;
        NotificationDispatcher _notificationDispatcher;
    };

} // namespace Plugin
} // namespace WPEFramework
