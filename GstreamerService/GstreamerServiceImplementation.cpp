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

    GstreamerServiceImplementation::GstreamerServiceImplementation()
        : _adminLock()
        , _shell(nullptr)
        , _pipeline(nullptr)
        , _bus(nullptr)
        , _busWatchId(0)
        , _notificationDispatcher()
    {
        SYSLOG(Logging::Startup, (_T("GstreamerServiceImplementation Constructor")));
        
        // Initialize GStreamer
    }

    GstreamerServiceImplementation::~GstreamerServiceImplementation()
    {
        SYSLOG(Logging::Shutdown, (_T("GstreamerServiceImplementation Destructor")));
        
        CleanupPipeline();

        if (_shell != nullptr) {
            _shell->Release();
            _shell = nullptr;
        }

    }

    uint32_t GstreamerServiceImplementation::Configure(PluginHost::IShell* shell)
    {
        ASSERT(shell != nullptr);
        
        _adminLock.Lock();
        gst_init(nullptr, nullptr);

        _shell = shell;
        _shell->AddRef();
        
        _adminLock.Unlock();

        SYSLOG(Logging::Startup, (_T("GstreamerServiceImplementation::Configure: Configured successfully")));
        
        return Core::ERROR_NONE;
    }

    Core::hresult GstreamerServiceImplementation::StartPipeline(const string& pipelineConfig)
    {
        _adminLock.Lock();

        SYSLOG(Logging::Startup, (_T("GstreamerServiceImplementation::StartPipeline: %s"), pipelineConfig.c_str()));

        if (pipelineConfig.empty()) {
            _adminLock.Unlock();
            _notificationDispatcher.NotifyError("Pipeline configuration is empty");
            return Core::ERROR_GENERAL;
        }

        // Stop any existing pipeline
        if (_pipeline != nullptr) {
            CleanupPipeline();
        }

        // Create pipeline from description
        GError* error = nullptr;
        _pipeline = gst_parse_launch(pipelineConfig.c_str(), &error);

        if (error != nullptr || _pipeline == nullptr) {
            string errorMsg = error ? error->message : "Unknown error creating pipeline";
            g_clear_error(&error);
            _adminLock.Unlock();
            _notificationDispatcher.NotifyError(errorMsg);
            return Core::ERROR_GENERAL;
        }

        // Get the pipeline bus
        _bus = gst_element_get_bus(_pipeline);
        if (_bus != nullptr) {
            // Add a bus watch to monitor pipeline messages
            _busWatchId = gst_bus_add_watch(_bus, (GstBusFunc)BusCallback, this);
        }

        // Start playing the pipeline
        GstStateChangeReturn ret = gst_element_set_state(_pipeline, GST_STATE_PLAYING);
        
        _adminLock.Unlock();

        if (ret == GST_STATE_CHANGE_FAILURE) {
            _notificationDispatcher.NotifyError("Failed to start pipeline");
            CleanupPipeline();
            return Core::ERROR_GENERAL;
        }

        _notificationDispatcher.NotifyPipelineStateChanged("PLAYING");

        return Core::ERROR_NONE;
    }

    Core::hresult GstreamerServiceImplementation::StopPipeline()
    {
        _adminLock.Lock();

        SYSLOG(Logging::Shutdown, (_T("GstreamerServiceImplementation::StopPipeline")));

        if (_pipeline == nullptr) {
            _adminLock.Unlock();
            return Core::ERROR_ILLEGAL_STATE;
        }

        // Stop the pipeline
        GstStateChangeReturn ret = gst_element_set_state(_pipeline, GST_STATE_NULL);

        CleanupPipeline();

        _adminLock.Unlock();

        if (ret == GST_STATE_CHANGE_FAILURE) {
            _notificationDispatcher.NotifyError("Failed to stop pipeline");
            return Core::ERROR_GENERAL;
        }

        _notificationDispatcher.NotifyPipelineStateChanged("NULL");

        return Core::ERROR_NONE;
    }

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

    Core::hresult GstreamerServiceImplementation::PlayPause()
    {
        _adminLock.Lock();

        if(_pipeline == nullptr){
            _adminLock.Unlock();
            return Core::ERROR_ILLEGAL_STATE;
        }
        GstState state,pending;
        gst_element_get_state(_pipeline, &state, &pending, 0);

        if(state == GST_STATE_PLAYING){
            SYSLOG(Logging::Notification, (_T("Pausing playback")));
            gst_element_set_state(_pipeline, GST_STATE_PAUSED);
            _adminLock.Unlock();
            _notificationDispatcher.NotifyPipelineStateChanged("PAUSED");
        } else{
            SYSLOG(Logging::Notification, (_T("Resuming playback")));
            gst_element_set_state(_pipeline, GST_STATE_PLAYING);
            _adminLock.Unlock();
            _notificationDispatcher.NotifyPipelineStateChanged("PLAYING");
        }
        return Core::ERROR_NONE;   
    }

    Core::hresult GstreamerServiceImplementation::Seek(const int64_t offset)
    {
        _adminLock.Lock();

        if(_pipeline == nullptr){
            _adminLock.Unlock();
            return Core::ERROR_ILLEGAL_STATE;
        }

        gint64 current_pos;
        if(!gst_element_query_position(_pipeline, GST_FORMAT_TIME, &current_pos)){
            _adminLock.Unlock();
            _notificationDispatcher.NotifyError("Unable to retrieve current position for seeking");
            return Core::ERROR_GENERAL;
        }
        gint64 new_pos = current_pos + (offset* GST_SECOND);
        if (new_pos <0){
            new_pos = 0;
        }
        SYSLOG(Logging::Notification, (_T("Seeking by %lld seconds to position %lld"), offset, new_pos / GST_SECOND));
        if(!gst_element_seek_simple(_pipeline, GST_FORMAT_TIME, (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT), new_pos)){
            _adminLock.Unlock();
            _notificationDispatcher.NotifyError("Seek operation failed");
            return Core::ERROR_GENERAL;
        }

         _adminLock.Unlock();
         return Core::ERROR_NONE;
    }

    Core::hresult GstreamerServiceImplementation::GetPosition(int64_t& position /* @out */, int64_t& duration /* @out*/) const
    {
        _adminLock.Lock();

        if(_pipeline == nullptr){
            //position = 0;
            //duration = 0;
            _adminLock.Unlock();
            return Core::ERROR_ILLEGAL_STATE;
        }

        gint64 pos, dur;
        if(!gst_element_query_position(_pipeline, GST_FORMAT_TIME, &pos)){
            pos = 0;
        }
        if(!gst_element_query_duration(_pipeline, GST_FORMAT_TIME, &dur)){
            dur = 0;
        }
        // Convert from nanoseconds to seconds for output
        position = pos / GST_SECOND; // Convert to seconds
        duration = dur / GST_SECOND; // Convert to seconds

        _adminLock.Unlock();
        return Core::ERROR_NONE;
    }

    Core::hresult GstreamerServiceImplementation::SetWindowVisible(const bool visible)
    {
        _adminLock.Lock();

        if (_pipeline == nullptr) {
            _adminLock.Unlock();
            return Core::ERROR_ILLEGAL_STATE;
        }

        SYSLOG(Logging::Notification, (_T("Setting Window Visibility to %s"), visible ? "Visible" : "Hidden"));
        // Get the video sink element
        GstElement* videoSink = nullptr;
        g_object_get(_pipeline, "video-sink", &videoSink, NULL);

        if (videoSink != nullptr) {
            // Set alpha/opacity property if supported
            if (g_object_class_find_property(G_OBJECT_GET_CLASS(videoSink), "alpha")) {
                g_object_set(videoSink, "alpha", visible ? 1.0 : 0.0, NULL);
            }
            gst_object_unref(videoSink);
        }

        _adminLock.Unlock();
        return Core::ERROR_NONE;
    }

    Core::hresult GstreamerServiceImplementation::Register(Exchange::IGstreamerService::INotification* notification)
    {
        ASSERT(notification != nullptr);

        if (notification != nullptr) {
            _notificationDispatcher.Register(notification);
            return Core::ERROR_NONE;
        }

        return Core::ERROR_GENERAL;
    }

    Core::hresult GstreamerServiceImplementation::Unregister(Exchange::IGstreamerService::INotification* notification)
    {
        ASSERT(notification != nullptr);

        if (notification != nullptr) {
            _notificationDispatcher.Unregister(notification);
            return Core::ERROR_NONE;
        }

        return Core::ERROR_GENERAL;
    }

    // Static callback for GStreamer bus messages
    gboolean GstreamerServiceImplementation::BusCallback(GstBus* bus, GstMessage* msg, gpointer data)
    {
        GstreamerServiceImplementation* impl = static_cast<GstreamerServiceImplementation*>(data);

        switch (GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_ERROR: {
                GError* err;
                gchar* debug_info;
                gst_message_parse_error(msg, &err, &debug_info);
                
                string errorMsg = "Error from ";
                errorMsg += GST_OBJECT_NAME(msg->src);
                errorMsg += ": ";
                errorMsg += err->message;
                
                if (debug_info) {
                    errorMsg += " (Debug: ";
                    errorMsg += debug_info;
                    errorMsg += ")";
                }

                impl->_notificationDispatcher.NotifyError(errorMsg);

                g_clear_error(&err);
                g_free(debug_info);
                break;
            }
            case GST_MESSAGE_WARNING: {
                GError* err;
                gchar* debug_info;
                gst_message_parse_warning(msg, &err, &debug_info);
                
                string warningMsg = "Warning from ";
                warningMsg += GST_OBJECT_NAME(msg->src);
                warningMsg += ": ";
                warningMsg += err->message;

                SYSLOG(Logging::Notification, (_T("GStreamer Warning: %s"), warningMsg.c_str()));

                g_clear_error(&err);
                g_free(debug_info);
                break;
            }
            case GST_MESSAGE_STATE_CHANGED: {
                if (GST_MESSAGE_SRC(msg) == GST_OBJECT(impl->_pipeline)) {
                    GstState old_state, new_state, pending_state;
                    gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
                    
                    string newStateStr = impl->GetGstStateAsString(new_state);
                    impl->_notificationDispatcher.NotifyPipelineStateChanged(newStateStr);
                }
                break;
            }
            case GST_MESSAGE_EOS: {
                impl->_notificationDispatcher.NotifyPipelineStateChanged("EOS");
                break;
            }
            default:
                break;
        }

        return TRUE;
    }

    void GstreamerServiceImplementation::CleanupPipeline()
    {
        if (_busWatchId > 0) {
            g_source_remove(_busWatchId);
            _busWatchId = 0;
        }

        if (_bus != nullptr) {
            gst_object_unref(_bus);
            _bus = nullptr;
        }

        if (_pipeline != nullptr) {
            gst_element_set_state(_pipeline, GST_STATE_NULL);
            gst_object_unref(_pipeline);
            _pipeline = nullptr;
        }
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
