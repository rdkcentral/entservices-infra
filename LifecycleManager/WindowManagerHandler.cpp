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
#include <cstdio>

#define DEBUG_PRINTF(fmt, ...) \
    std::printf("[DEBUG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

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
    DEBUG_PRINTF("ERROR: RDKEMW-2806");
    bool ret = false;
    mEventHandler = eventHandler;
    mWindowManager = service->QueryInterfaceByCallsign<Exchange::IRDKWindowManager>("org.rdk.RDKWindowManager");
    DEBUG_PRINTF("ERROR: RDKEMW-2806");
    if (mWindowManager != nullptr)
    {
        DEBUG_PRINTF("ERROR: RDKEMW-2806");
        ret = true;
        Core::hresult registerResult = mWindowManager->Register(&mWindowManagerNotification);
        if (Core::ERROR_NONE != registerResult)
        {
            LOGINFO("Unable to register with windowmanager [%d] \n", registerResult);
        }
        DEBUG_PRINTF("ERROR: RDKEMW-2806");
    }
    else
    {
        LOGERR("windowmanager is null \n");
    }
    DEBUG_PRINTF("ERROR: RDKEMW-2806");
    return ret;
}

void WindowManagerHandler::terminate()
{
    DEBUG_PRINTF("ERROR: RDKEMW-2806");
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
    printf(" Received onUserInactivity event for %f minutes \n", minutes);
    fflush(stdout);
    JsonObject eventData;
    eventData["minutes"] = minutes;
    eventData["name"] = "onUserInactivity";
    _parent.onEvent(eventData);
}

void WindowManagerHandler::WindowManagerNotification::OnDisconnected(const std::string& client)
{
    printf(" Received onDisconnect event for client[%s] \n", client.c_str());
    fflush(stdout);
    JsonObject eventData;
    eventData["client"] = client;
    eventData["name"] = "onDisconnect";
    _parent.onEvent(eventData);
}

void WindowManagerHandler::onEvent(JsonObject& data)
{
    if (mEventHandler)
    {
        mEventHandler->onWindowManagerEvent(data);
    }
}

Core::hresult WindowManagerHandler::renderReady(std::string appInstanceId, bool& isReady)
{
    Core::hresult renderReadyResult = mWindowManager->RenderReady(appInstanceId, isReady);
    if (Core::ERROR_NONE != renderReadyResult)
    {
        printf("unable to get render ready from window manager [%d] \n", renderReadyResult);
	fflush(stdout);
    }
    return renderReadyResult;
}

Core::hresult WindowManagerHandler::enableDisplayRender(std::string appInstanceId, bool render)
{
    Core::hresult enableDisplayRenderResult = mWindowManager->EnableDisplayRender(appInstanceId, render);
    if (Core::ERROR_NONE != enableDisplayRenderResult)
    {
        printf("unable to perform enable display renderer from window manager [%d] \n", enableDisplayRenderResult);
	fflush(stdout);
    }
    return enableDisplayRenderResult;
}

void WindowManagerHandler::WindowManagerNotification::OnReady(const std::string &client)
{
    printf("MADANA Received onReady event for app[%s] \n", client.c_str());
    fflush(stdout);
    JsonObject eventData;
    eventData["appInstanceId"] = client;
    eventData["name"] = "onReady";
    _parent.onEvent(eventData);
}

} // namespace Plugin
} // namespace WPEFramework
