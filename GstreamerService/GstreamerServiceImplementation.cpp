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

#include "GstreamerServiceImplementation.h"

namespace WPEFramework {
    namespace Plugin {

        SERVICE_REGISTRATION(GstreamerServiceImplementation, 1, 0);

        // =====================================================================
        // Constructor / Destructor
        // =====================================================================

        GstreamerServiceImplementation::GstreamerServiceImplementation()
            : _adminLock()
            , _notificationClients()
            , _pipeline(nullptr)
            , _busWatchId(0)
            , _mainLoop(nullptr)
        {
            gst_init(nullptr, nullptr);
            SYSLOG(Logging::Startup, (_T("GstreamerServiceImplementation: GStreamer initialised")));
        }

        GstreamerServiceImplementation::~GstreamerServiceImplementation()
        {
            if (_pipeline != nullptr) {
                CleanupPipeline();
            }
            SYSLOG(Logging::Shutdown, (_T("GstreamerServiceImplementation Destructor")));
        }

        // =====================================================================
        // Register / Unregister notification clients
        // =====================================================================

        Core::hresult GstreamerServiceImplementation::Register(Exchange::IGstreamerService::INotification* notification)
        {
            ASSERT(notification != nullptr);

            _adminLock.Lock();
            auto it = std::find(_notificationClients.begin(), _notificationClients.end(), notification);
            if (it == _notificationClients.end()) {
                notification->AddRef();
                _notificationClients.push_back(notification);
            }
            _adminLock.Unlock();

            return Core::ERROR_NONE;
        }

        Core::hresult GstreamerServiceImplementation::Unregister(Exchange::IGstreamerService::INotification* notification)
        {
            ASSERT(notification != nullptr);

            _adminLock.Lock();
            auto it = std::find(_notificationClients.begin(), _notificationClients.end(), notification);
            if (it != _notificationClients.end()) {
                (*it)->Release();
                _notificationClients.erase(it);
            }
            _adminLock.Unlock();

            return Core::ERROR_NONE;
        }

        // =====================================================================
        // StartPipeline
        // =====================================================================

        Core::hresult GstreamerServiceImplementation::StartPipeline(const string& pipelineConfig)
        {
            LOGINFO("GstreamerService::StartPipeline: %s", pipelineConfig.c_str());

            if (pipelineConfig.empty()) {
                FireError("Pipeline configuration is empty");
                return Core::ERROR_GENERAL;
            }

            if (_pipeline != nullptr) {
                CleanupPipeline();
            }

            GError* error = nullptr;
            _pipeline = gst_parse_launch(pipelineConfig.c_str(), &error);

            if (error != nullptr || _pipeline == nullptr) {
                string errorMsg = error ? error->message : "Unknown error creating pipeline";
                g_clear_error(&error);
                FireError(errorMsg);
                return Core::ERROR_GENERAL;
            }

            GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(_pipeline));
            _busWatchId = gst_bus_add_watch(bus, GstreamerServiceImplementation::BusCallback, this);
            gst_object_unref(bus);

            // Start a GMainLoop in a background thread.
            // GStreamer bus watches require a running GLib main loop to dispatch
            // messages (errors, EOS, state-change notifications) asynchronously.
            _mainLoop = g_main_loop_new(nullptr, FALSE);
            _mainLoopThread = std::thread([this]() {
                g_main_loop_run(_mainLoop);
            });

            GstStateChangeReturn ret = gst_element_set_state(_pipeline, GST_STATE_PLAYING);

            if (ret == GST_STATE_CHANGE_FAILURE) {
                FireError("Failed to start pipeline");
                CleanupPipeline();
                return Core::ERROR_GENERAL;
            }

            FirePipelineStateChanged("PLAYING");
            return Core::ERROR_NONE;
        }

        // =====================================================================
        // StopPipeline
        // =====================================================================

        Core::hresult GstreamerServiceImplementation::StopPipeline()
        {
            if (_pipeline == nullptr) {
                LOGERR("GstreamerService::StopPipeline: No pipeline is running");
                return Core::ERROR_ILLEGAL_STATE;
            }

            LOGINFO("GstreamerService::StopPipeline: stopping pipeline");
            CleanupPipeline();
            FirePipelineStateChanged("NULL");
            return Core::ERROR_NONE;
        }

        // =====================================================================
        // GetPipelineStatus
        // =====================================================================

        Core::hresult GstreamerServiceImplementation::GetPipelineStatus(string& status /* @out */) const
        {
            _adminLock.Lock();

            if (_pipeline == nullptr) {
                status = "NULL";
                _adminLock.Unlock();
                return Core::ERROR_NONE;
            }

            GstState state;
            GstState pending;
            GstStateChangeReturn ret = gst_element_get_state(_pipeline, &state, &pending, GST_CLOCK_TIME_NONE);

            if (ret == GST_STATE_CHANGE_FAILURE) {
                status = "UNKNOWN";
                _adminLock.Unlock();
                return Core::ERROR_GENERAL;
            }

            status = GetGstStateAsString(state);

            _adminLock.Unlock();
            return Core::ERROR_NONE;
        }

        // =====================================================================
        // PlayPause
        // =====================================================================

        Core::hresult GstreamerServiceImplementation::PlayPause()
        {
            if (_pipeline == nullptr) {
                LOGERR("GstreamerService::PlayPause: No pipeline is running");
                return Core::ERROR_ILLEGAL_STATE;
            }

            GstState state, pending;
            gst_element_get_state(_pipeline, &state, &pending, 0);

            if (state == GST_STATE_PLAYING) {
                LOGINFO("GstreamerService::PlayPause: Pausing playback");
                gst_element_set_state(_pipeline, GST_STATE_PAUSED);
                FirePipelineStateChanged("PAUSED");
            } else {
                LOGINFO("GstreamerService::PlayPause: Resuming playback");
                gst_element_set_state(_pipeline, GST_STATE_PLAYING);
                FirePipelineStateChanged("PLAYING");
            }

            return Core::ERROR_NONE;
        }

        // =====================================================================
        // Seek
        // =====================================================================

        Core::hresult GstreamerServiceImplementation::Seek(const int64_t offset)
        {
            if (_pipeline == nullptr) {
                LOGERR("GstreamerService::Seek: No pipeline is running");
                return Core::ERROR_ILLEGAL_STATE;
            }

            gint64 current_pos;
            if (!gst_element_query_position(_pipeline, GST_FORMAT_TIME, &current_pos)) {
                FireError("Unable to retrieve current position for seeking");
                return Core::ERROR_GENERAL;
            }

            gint64 new_pos = current_pos + (offset * GST_SECOND);
            if (new_pos < 0) {
                new_pos = 0;
            }

            LOGINFO("GstreamerService::Seek: by %lld seconds to position %lld",
                    offset, new_pos / GST_SECOND);

            if (!gst_element_seek_simple(_pipeline, GST_FORMAT_TIME,
                    static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT), new_pos)) {
                FireError("Seek operation failed");
                return Core::ERROR_GENERAL;
            }

            return Core::ERROR_NONE;
        }

        // =====================================================================
        // GetPosition
        // =====================================================================

        Core::hresult GstreamerServiceImplementation::GetPosition(int64_t& position /* @out */, int64_t& duration /* @out*/) const
        {
            _adminLock.Lock();

            if (_pipeline == nullptr) {
                _adminLock.Unlock();
                return Core::ERROR_ILLEGAL_STATE;
            }

            gint64 pos, dur;
            if (!gst_element_query_position(_pipeline, GST_FORMAT_TIME, &pos)) {
                pos = 0;
            }
            if (!gst_element_query_duration(_pipeline, GST_FORMAT_TIME, &dur)) {
                dur = 0;
            }

            position = pos / GST_SECOND;
            duration = dur / GST_SECOND;

            _adminLock.Unlock();
            return Core::ERROR_NONE;
        }

        // =====================================================================
        // SetWindowVisible
        // =====================================================================

        Core::hresult GstreamerServiceImplementation::SetWindowVisible(const bool visible)
        {
            if (_pipeline == nullptr) {
                LOGERR("GstreamerService::SetWindowVisible: No pipeline is running");
                return Core::ERROR_ILLEGAL_STATE;
            }

            LOGINFO("GstreamerService::SetWindowVisible: %s", visible ? "Visible" : "Hidden");

            GstElement* videoSink = nullptr;
            g_object_get(_pipeline, "video-sink", &videoSink, NULL);

            if (videoSink != nullptr) {
                if (g_object_class_find_property(G_OBJECT_GET_CLASS(videoSink), "alpha")) {
                    g_object_set(videoSink, "alpha", visible ? 1.0 : 0.0, NULL);
                }
                gst_object_unref(videoSink);
            }

            return Core::ERROR_NONE;
        }

        // =====================================================================
        // Private helpers
        // =====================================================================

        /* static */
        gboolean GstreamerServiceImplementation::BusCallback(
            GstBus* /* bus */, GstMessage* msg, gpointer data)
        {
            GstreamerServiceImplementation* self =
                static_cast<GstreamerServiceImplementation*>(data);

            switch (GST_MESSAGE_TYPE(msg)) {
                case GST_MESSAGE_ERROR: {
                    GError* err   = nullptr;
                    gchar*  debug = nullptr;
                    gst_message_parse_error(msg, &err, &debug);
                    LOGERR("GstreamerService::BusCallback: ERROR – %s (%s)",
                           err ? err->message : "unknown",
                           debug ? debug : "no debug info");
                    g_clear_error(&err);
                    g_free(debug);
                    self->CleanupPipeline();
                    break;
                }

                case GST_MESSAGE_WARNING: {
                    GError* err   = nullptr;
                    gchar*  debug = nullptr;
                    gst_message_parse_warning(msg, &err, &debug);
                    LOGINFO("GstreamerService::BusCallback: WARNING – %s (%s)",
                            err ? err->message : "unknown",
                            debug ? debug : "no debug info");
                    g_clear_error(&err);
                    g_free(debug);
                    break;
                }

                case GST_MESSAGE_STATE_CHANGED: {
                    if (GST_MESSAGE_SRC(msg) == GST_OBJECT(self->_pipeline)) {
                        GstState old_state, new_state, pending_state;
                        gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
                        string newStateStr = self->GetGstStateAsString(new_state);
                        self->FirePipelineStateChanged(newStateStr);
                    }
                    break;
                }

                case GST_MESSAGE_EOS:
                    LOGINFO("GstreamerService::BusCallback: EOS");
                    self->FirePipelineStateChanged("EOS");
                    break;

                default:
                    break;
            }

            return TRUE;
        }

        void GstreamerServiceImplementation::CleanupPipeline()
        {
            if (_pipeline != nullptr) {
                gst_element_set_state(_pipeline, GST_STATE_NULL);

                if (_busWatchId != 0) {
                    g_source_remove(_busWatchId);
                    _busWatchId = 0;
                }

                gst_object_unref(_pipeline);
                _pipeline = nullptr;
            }

            // Stop the GMainLoop and wait for the thread to exit.
            if (_mainLoop != nullptr) {
                g_main_loop_quit(_mainLoop);
                if (_mainLoopThread.joinable()) {
                    _mainLoopThread.join();
                }
                g_main_loop_unref(_mainLoop);
                _mainLoop = nullptr;
            }
        }

        void GstreamerServiceImplementation::FirePipelineStateChanged(const string& state)
        {
            _adminLock.Lock();
            for (auto* client : _notificationClients) {
                client->OnPipelineStateChanged(state);
            }
            _adminLock.Unlock();
        }

        void GstreamerServiceImplementation::FireError(const string& errorMessage)
        {
            _adminLock.Lock();
            for (auto* client : _notificationClients) {
                client->OnError(errorMessage);
            }
            _adminLock.Unlock();
        }

        string GstreamerServiceImplementation::GetGstStateAsString(GstState state) const
        {
            switch (state) {
                case GST_STATE_VOID_PENDING:
                    return "VOID_PENDING";
                case GST_STATE_NULL:
                    return "NULL";
                case GST_STATE_READY:
                    return "READY";
                case GST_STATE_PAUSED:
                    return "PAUSED";
                case GST_STATE_PLAYING:
                    return "PLAYING";
                default:
                    return "UNKNOWN";
            }
        }

    } // namespace Plugin
} // namespace WPEFramework
