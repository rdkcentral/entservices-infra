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

#include "TelemetryMetricsReporting.h"
#include "UtilsLogging.h"
#include <time.h>


namespace WPEFramework
{
namespace Plugin
{
    TelemetryMetricsReporting::TelemetryMetricsReporting() : mTelemetryMetricsPluginObject(nullptr), mController(nullptr)
    {
        if(Core::ERROR_NONE != createTelemetryMetricsPluginObject())
        {
            LOGERR("Failed to create TelemetryMetrics plugin object");
        }
    }

    TelemetryMetricsReporting::~TelemetryMetricsReporting()
    {
        releaseTelemetryMetricsPluginObject();
    }

    TelemetryMetricsReporting& TelemetryMetricsReporting::getInstance()
    {
        LOGINFO("Get TelemetryMetricsReporting Instance");
        static TelemetryMetricsReporting instance;
        return instance;
    }

/*
* Creates TelemetryMetrics plugin object to access interface methods
*/
    Core::hresult TelemetryMetricsReporting::createTelemetryMetricsPluginObject()
    {
        Core::hresult status = Core::ERROR_GENERAL;

        mAdminLock.Lock();
        mEngine = Core::ProxyType<RPC::InvokeServerType<1, 0, 4>>::Create();
        mCommunicatorClient = Core::ProxyType<RPC::CommunicatorClient>::Create(Core::NodeId("/tmp/communicator"), Core::ProxyType<Core::IIPCServer>(mEngine));

        if (!mCommunicatorClient.IsValid())
        {
            LOGINFO("Invalid communicator client");
        }
        else
        {
            mController = mCommunicatorClient->Open<PluginHost::IShell>(_T("org.rdk.TelemetryMetrics"), ~0, 3000);
            if (mController)
            {
                LOGINFO("Create mController is success");
                mTelemetryMetricsPluginObject = mController->QueryInterface<Exchange::ITelemetryMetrics>();
                status = Core::ERROR_NONE;
            }
            else
            {
                mCommunicatorClient->Close(RPC::CommunicationTimeOut);
                mCommunicatorClient.Release();
                mEngine.Release();
            }
        }
        mAdminLock.Unlock();
        return status;
    }

/*
* Releases TelemetryMetrics plugin object
*/
    void TelemetryMetricsReporting::releaseTelemetryMetricsPluginObject()
    {
        mAdminLock.Lock();
        if(mTelemetryMetricsPluginObject)
        {
            LOGINFO("TelemetryMetrics object released\n");
            mTelemetryMetricsPluginObject->Release();
            mTelemetryMetricsPluginObject = nullptr;
        }

        if (mController)
        {
            mController->Release();
            mController = nullptr;
        }

        if (mCommunicatorClient.IsValid())
        {
            mCommunicatorClient->Close(RPC::CommunicationTimeOut);
            mCommunicatorClient.Release();
        }

        if (mEngine.IsValid())
        {
            mEngine.Release();
        }
        mAdminLock.Unlock();
    }

/*
* Called by individual plugins to record telemetry data with TelemetryMetrics plugin
*/
    Core::hresult TelemetryMetricsReporting::recordTelemetryMetrics(const string& markerName, const string& id, const string& telemetryMetrics)
    {
        Core::hresult status = Core::ERROR_GENERAL;

        mAdminLock.Lock();
        /* Checking if mTelemetryMetricsPluginObject is not valid then create the object */
        if (nullptr == mTelemetryMetricsPluginObject)
        {
            if (Core::ERROR_NONE != createTelemetryMetricsPluginObject())
            {
                LOGERR("Failed to create TelemetryMetrics plugin object");
            }
        }

        if (nullptr != mTelemetryMetricsPluginObject)
        {
            LOGINFO("Record telemetry metrics invoked for marker %s, record id %s, record %s", markerName.c_str(), id.c_str(), telemetryMetrics.c_str());
            status = mTelemetryMetricsPluginObject->Record(id, telemetryMetrics, markerName);
        }
        else
        {
            LOGERR("TelemetryMetrics plugin object is invalid");
        }
        mAdminLock.Unlock();

        return status;
    }

/*
* Called by individual plugins to publish recorded telemetry data
*/
    Core::hresult TelemetryMetricsReporting::publishTelemetryMetrics(const string& markerName, const string& id)
    {
        Core::hresult status = Core::ERROR_GENERAL;

        LOGINFO("Publish telemetry metrics invoked for record id %s, marker %s", id.c_str(), markerName.c_str());

        mAdminLock.Lock();
        /* Checking if mTelemetryMetricsPluginObject is not valid then create the object */
        if (nullptr == mTelemetryMetricsPluginObject)
        {
            if (Core::ERROR_NONE != createTelemetryMetricsPluginObject())
            {
                LOGERR("Failed to create TelemetryMetrics plugin object");
            }
        }

        if (nullptr != mTelemetryMetricsPluginObject)
        {
            status = mTelemetryMetricsPluginObject->Publish(id, markerName);
        }
        else
        {
            LOGERR("TelemetryMetrics plugin object is invalid");
        }
        mAdminLock.Unlock();

        return status;
    }

/*
* Provides current time stamp in milliseconds
*/
    uint64_t TelemetryMetricsReporting::getCurrentTimestamp()
    {
        timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (((uint64_t)ts.tv_sec * 1000) + ((uint64_t)ts.tv_nsec/1000000));
    }

} /* namespace Plugin */
} /* namespace WPEFramework */

