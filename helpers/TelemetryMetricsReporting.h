/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2025 RDK Management
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

#include <interfaces/ITelemetryMetrics.h>
#include <plugins/plugins.h>
#include <utility>
#include <boost/variant.hpp>


namespace WPEFramework
{
namespace Plugin
{

class TelemetryMetricsReporting
{
    public /*methods*/:
        TelemetryMetricsReporting(const TelemetryMetricsReporting&) = delete;
        TelemetryMetricsReporting& operator=(const TelemetryMetricsReporting&) = delete;
        static TelemetryMetricsReporting& getInstance();

        Core::hresult recordTelemetryMetrics(const string& markerName, const string& id, const string& telemetryMetrics);
        Core::hresult publishTelemetryMetrics(const string& markerName, const string& id);
        uint64_t getCurrentTimestamp();

    private /*methods*/:
        TelemetryMetricsReporting();
        ~TelemetryMetricsReporting();
        Core::hresult createTelemetryMetricsPluginObject();
        void releaseTelemetryMetricsPluginObject();

    private /*members*/:
        Exchange::ITelemetryMetrics *mTelemetryMetricsPluginObject;
        PluginHost::IShell *mController;
        Core::ProxyType<RPC::InvokeServerType<1, 0, 4>> mEngine;
        Core::ProxyType<RPC::CommunicatorClient> mCommunicatorClient;
        mutable Core::CriticalSection mAdminLock;

};

} /* namespace Plugin */
} /* namespace WPEFramework */

