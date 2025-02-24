/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2020 RDK Management
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

#include <string>
#include <memory>
#include <iostream>
#include <mutex>
#include <thread>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <plugins/System.h>
#include "UtilsLogging.h"
#include "tracing/Logging.h"
#include <core/core.h>
#include <com/com.h>
#include <plugins/plugins.h>
#include <interfaces/ILifecycleManager.h>


namespace WPEFramework
{
    namespace Plugin
    {
        class LifecycleInterfaceConnector
        {
                public/*members*/:
                    static LifecycleInterfaceConnector* _instance;

                public:
                    LifecycleInterfaceConnector(PluginHost::IShell* service);
                    ~LifecycleInterfaceConnector();
                    Core::hresult createLifecycleManagerRemoteObject();
                    void releaseLifecycleManagerRemoteObject();
                    Core::hresult launch(const string& appId, const string& intent, const string& launchArgs);
                    Core::hresult preLoadApp(const string& appId, const string& launchArgs, string& error);
                    Core::hresult closeApp(const string& appId);
                    Core::hresult terminateApp(const string& appId);
                    Core::hresult killApp(const string& appId);
                    Core::hresult sendIntent(const string& appId, const string& intent);
                    Core::hresult getLoadedApps(string& apps);
                    void OnAppStateChanged(const string& appId, Exchange::ILifecycleManager::LifecycleState state, const string& errorReason);
                    string GetAppInstanceId(const string& appId) const;
                    void RemoveApp(const string& appId);

                private:
                    mutable Core::CriticalSection mAdminLock;
                    Exchange::ILifecycleManager *mLifecycleManagerRemoteObject;
                    PluginHost::IShell* mCurrentservice;
        };
    }
}
