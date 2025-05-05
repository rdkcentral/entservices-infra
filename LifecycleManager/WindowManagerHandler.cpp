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

#include "WindowManagerHandler.h"
#include <fstream>
#include <random>

namespace WPEFramework {
namespace Plugin {

WindowManagerHandler::WindowManagerHandler()
: mWindowManager(nullptr), mWindowManagerNotification(*this), mEventHandler(nullptr)
{
    LOGINFO("Create WindowManagerHandler Instance");
}

WindowManagerHandler::~WindowManagerHandler()
{
    LOGINFO("Delete WindowManagerHandler Instance");
}

bool WindowManagerHandler::initialize(PluginHost::IShell* service, IEventHandler* eventHandler)
{
    bool ret = false;
    mEventHandler = eventHandler;
    mWindowManager = service->QueryInterfaceByCallsign<Exchange::IRDKWindowManager>("org.rdk.RDKWindowManager");
    if (mWindowManager != nullptr)
    {
        ret = true;
        Core::hresult registerResult = mWindowManager->Register(&mWindowManagerNotification);
        if (Core::ERROR_NONE != registerResult)
        {
            LOGINFO("Unable to register with windowmanager [%d] \n", registerResult);
        }
    }
    else
    {
        LOGERR("windowmanager is null \n");
    }
    return ret;
}

void WindowManagerHandler::terminate()
{
    Core::hresult unregisterResult = mWindowManager->Unregister(&mWindowManagerNotification);
    if (Core::ERROR_NONE != unregisterResult)
    {
        LOGINFO("Unable to unregister from runtimemanager [%d] \n", unregisterResult);
    }
    uint32_t result = mWindowManager->Release();
    LOGINFO("Window manager releases [%d]\n", result);
    mWindowManager = nullptr;
}

void WindowManagerHandler::WindowManagerNotification::OnUserInactivity(const double minutes)
{
}

void WindowManagerHandler::WindowManagerNotification::OnDisconnected(const std::string& client)
{
    LOGINFO("Shreyas LCM WindowManagerNotification::OnDisconnected client: %s", client.c_str());
}

//TODO
/*
void WindowManagerHandler::WindowManagerNotification::OnReady(std::string appId)
{
    printf("MADANA Received onReady event for app[%s] \n", appId.c_str());
    fflush(stdout);
}
*/
} // namespace Plugin
} // namespace WPEFramework
