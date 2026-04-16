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

#include "GStreamerPlayerImplementation.h"

namespace WPEFramework {
namespace Plugin {

SERVICE_REGISTRATION(GStreamerPlayerImplementation, 1, 0);

GStreamerPlayerImplementation::GStreamerPlayerImplementation()
    : _pipeline(nullptr)
{
    gst_init(nullptr, nullptr);
}

GStreamerPlayerImplementation::~GStreamerPlayerImplementation()
{
    Cleanup();
}

Core::hresult GStreamerPlayerImplementation::Start(const string& uri)
{
    Cleanup();

    std::string desc = "playbin uri=" + uri;
    _pipeline = gst_parse_launch(desc.c_str(), nullptr);

    if (!_pipeline)
        return Core::ERROR_GENERAL;

    gst_element_set_state(_pipeline, GST_STATE_PLAYING);
    NotifyState("PLAYING");

    return Core::ERROR_NONE;
}

Core::hresult GStreamerPlayerImplementation::Stop()
{
    Cleanup();
    NotifyState("STOPPED");
    return Core::ERROR_NONE;
}

Core::hresult GStreamerPlayerImplementation::PlayPause()
{
    GstState state;
    gst_element_get_state(_pipeline, &state, nullptr, 0);

    if (state == GST_STATE_PLAYING)
        gst_element_set_state(_pipeline, GST_STATE_PAUSED);
    else
        gst_element_set_state(_pipeline, GST_STATE_PLAYING);

    return Core::ERROR_NONE;
}

Core::hresult GStreamerPlayerImplementation::Seek(const int32_t offset)
{
    gint64 pos;
    gst_element_query_position(_pipeline, GST_FORMAT_TIME, &pos);

    pos += offset * GST_SECOND;
    if (pos < 0) pos = 0;

    gst_element_seek_simple(_pipeline,
        GST_FORMAT_TIME,
        GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
        pos);

    return Core::ERROR_NONE;
}

Core::hresult GStreamerPlayerImplementation::SetVolume(const double volume)
{
    double v = std::max(0.0, std::min(1.0, volume));
    g_object_set(_pipeline, "volume", v, NULL);
    return Core::ERROR_NONE;
}

Core::hresult GStreamerPlayerImplementation::GetState(string& state) const
{
    state = "UNKNOWN";
    return Core::ERROR_NONE;
}

Core::hresult GStreamerPlayerImplementation::Register(INotification* n)
{
    _clients.push_back(n);
    return Core::ERROR_NONE;
}

Core::hresult GStreamerPlayerImplementation::Unregister(INotification* n)
{
    _clients.remove(n);
    return Core::ERROR_NONE;
}

void GStreamerPlayerImplementation::Cleanup()
{
    if (_pipeline) {
        gst_element_set_state(_pipeline, GST_STATE_NULL);
        gst_object_unref(_pipeline);
        _pipeline = nullptr;
    }
}

void GStreamerPlayerImplementation::NotifyState(const string& state)
{
    for (auto* c : _clients) {
        c->OnStateChanged(state);
    }
}

}
}