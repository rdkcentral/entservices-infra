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

#include "GStreamerPlayer.h"

namespace WPEFramework {

namespace {
    static Plugin::Metadata<Plugin::GStreamerPlayer> metadata(
        1, 0, 0, {}, {}, {}
    );
}

namespace Plugin {

SERVICE_REGISTRATION(GStreamerPlayer, 1, 0);

GStreamerPlayer::GStreamerPlayer()
    : _service(nullptr), _connectionId(0), _impl(nullptr)
{
}

GStreamerPlayer::~GStreamerPlayer() = default;

const string GStreamerPlayer::Initialize(PluginHost::IShell* service)
{
    ASSERT(service != nullptr);

    _service = service;
    _service->AddRef();

    _impl = _service->Root<Exchange::IGStreamerPlayer>(
        _connectionId, 2000, _T("GStreamerPlayerImplementation"));

    if (_impl != nullptr) {
        Exchange::JGStreamerPlayer::Register(*this, _impl);
    }

    return {};
}

void GStreamerPlayer::Deinitialize(PluginHost::IShell* service)
{
    if (_impl) {
        Exchange::JGStreamerPlayer::Unregister(*this);
        _impl->Release();
        _impl = nullptr;
    }

    if (_service) {
        _service->Release();
        _service = nullptr;
    }
}

string GStreamerPlayer::Information() const
{
    return "GStreamerPlayer Thunder Plugin";
}

}
}