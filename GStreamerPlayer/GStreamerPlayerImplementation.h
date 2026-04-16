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
#include <interfaces/IGStreamerPlayer.h>
#include <gst/gst.h>
#include <list>

namespace WPEFramework {
namespace Plugin {

class GStreamerPlayerImplementation : public Exchange::IGStreamerPlayer {
public:
    GStreamerPlayerImplementation();
    ~GStreamerPlayerImplementation() override;

    BEGIN_INTERFACE_MAP(GStreamerPlayerImplementation)
    INTERFACE_ENTRY(Exchange::IGStreamerPlayer)
    END_INTERFACE_MAP

    Core::hresult Start(const string& uri) override;
    Core::hresult Stop() override;
    Core::hresult PlayPause() override;
    Core::hresult Seek(const int32_t offset) override;
    Core::hresult SetVolume(const double volume) override;
    Core::hresult GetState(string& state) const override;

    Core::hresult Register(INotification* notification) override;
    Core::hresult Unregister(INotification* notification) override;

private:
    void Cleanup();
    void NotifyState(const string& state);

private:
    GstElement* _pipeline;
    std::list<INotification*> _clients;
};

}
}