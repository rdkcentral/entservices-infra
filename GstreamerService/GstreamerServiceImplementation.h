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
#include "UtilsLogging.h"

#include <com/com.h>
#include <core/core.h>

#include <list>
#include <thread>
#include <gst/gst.h>

namespace WPEFramework {
    namespace Plugin {

        class GstreamerServiceImplementation : public Exchange::IGstreamerService {
        public:
            GstreamerServiceImplementation();
            ~GstreamerServiceImplementation() override;

            GstreamerServiceImplementation(const GstreamerServiceImplementation&)            = delete;
            GstreamerServiceImplementation& operator=(const GstreamerServiceImplementation&) = delete;

            BEGIN_INTERFACE_MAP(GstreamerServiceImplementation)
            INTERFACE_ENTRY(Exchange::IGstreamerService)
            END_INTERFACE_MAP

            // ----- IGstreamerService -----
            Core::hresult Register(Exchange::IGstreamerService::INotification* notification) override;
            Core::hresult Unregister(Exchange::IGstreamerService::INotification* notification) override;

            Core::hresult StartPipeline(const string& pipelineConfig) override;
            Core::hresult StopPipeline() override;
            Core::hresult GetPipelineStatus(string& status /* @out */) const override;
            Core::hresult PlayPause() override;
            Core::hresult Seek(const int64_t offset) override;
            Core::hresult GetPosition(int64_t& position /* @out */, int64_t& duration /* @out*/) const override;
            Core::hresult SetWindowVisible(const bool visible) override;

        private:
            // GStreamer bus watch: dispatches pipeline messages (STATE_CHANGED, ERROR, EOS)
            // from the GMainLoop thread to the appropriate notification handler.
            static gboolean BusCallback(GstBus* bus, GstMessage* msg, gpointer data);

            // Bring the pipeline to GST_STATE_NULL, unref all elements, and stop the
            // GMainLoop thread. Safe to call even if no pipeline has been created yet.
            void CleanupPipeline();

            // Helpers that iterate _notificationClients and fire the named event.
            void FirePipelineStateChanged(const string& state);
            void FireError(const string& errorMessage);

            string GetGstStateAsString(GstState state) const;

        private:
            mutable Core::CriticalSection                             _adminLock;
            std::list<Exchange::IGstreamerService::INotification*>    _notificationClients;

            GstElement*  _pipeline;
            guint        _busWatchId;
            GMainLoop*   _mainLoop;
            std::thread  _mainLoopThread;
        };

    } // namespace Plugin
} // namespace WPEFramework
