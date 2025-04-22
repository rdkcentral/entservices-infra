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

#include "WindowManagerConnector.h"
#include <fstream>
#include <random>

namespace WPEFramework {
namespace Plugin {

WindowManagerConnector::WindowManagerConnector()
: mWindowManager(nullptr), mWindowManagerNotification(*this)
{
    LOGINFO("Create WindowManagerConnector Instance");
}

WindowManagerConnector::~WindowManagerConnector()
{
    LOGINFO("Delete WindowManagerConnector Instance");
}

bool WindowManagerConnector::initializePlugin(PluginHost::IShell* service)
{
    bool ret = false;
    if (nullptr == service)
    {
        LOGWARN("service is null \n");
    }
    else if (nullptr == (mWindowManager = service->QueryInterfaceByCallsign<Exchange::IRDKWindowManager>("org.rdk.RDKWindowManager")))
    {
        LOGWARN("Failed to create WindowManager object\n");
    }
    else
    {
        LOGINFO("Created WindowManager Object \n");
        mWindowManager->AddRef();
        ret = true;
        mPluginInitialized = true;
        Core::hresult registerResult = mWindowManager->Register(&mWindowManagerNotification);
        if (Core::ERROR_NONE != registerResult)
        {
            LOGINFO("Unable to register with windowmanager [%d] \n", registerResult);
        }
    }
    return ret;
}

void WindowManagerConnector::releasePlugin()
{
    if(mPluginInitialized == false)
    {
        LOGERR("WindowManagerConnector is not initialized \n");
        return;
    }
    Core::hresult releaseResult = mWindowManager->Unregister(&mWindowManagerNotification);
    if (Core::ERROR_NONE != releaseResult)
    {
        LOGINFO("Unable to unregister from windowmanager [%d] \n", releaseResult);
    }
    uint32_t result = mWindowManager->Release();
    mPluginInitialized = false;
    LOGINFO("Window manager releases [%d]\n", result);
    mWindowManager = nullptr;
}

bool WindowManagerConnector::createDisplay(const string& appInstanceId)
{
    if(mPluginInitialized == false)
    {
        LOGERR("WindowManagerConnector is not initialized \n");
        return false;
    }
    string displayName = "wst-" + appInstanceId;
    JsonObject displayParams;
    displayParams["client"] = appInstanceId;
    displayParams["displayName"] = displayName;
    string displayParamsString;
    displayParams.ToString(displayParamsString);

    Core::hresult result = mWindowManager->CreateDisplay(displayParamsString);
    if (Core::ERROR_NONE != result)
    {
        LOGERR("Failed to create display for application [%s] error [%d] \n",appInstanceId.c_str(), result);
        return false;
    }

    return true;
}

bool WindowManagerConnector::isPluginInitialized()
{
    return mPluginInitialized;
}

void WindowManagerConnector::WindowManagerNotification::OnUserInactivity(const double minutes)
{
}

} // namespace Plugin
} // namespace WPEFramework
