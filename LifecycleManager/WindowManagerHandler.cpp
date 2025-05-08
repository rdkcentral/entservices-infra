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

bool WindowManagerHandler::createDisplay(const string& appPath, const string& appConfig, const string& runtimeAppId, const string& runtimePath, const string& runtimeConfig, const string& launchArgs, const string& displayName, string& errorReason)
{
    JsonObject displayParams;
    displayParams["client"] = runtimeAppId;
    displayParams["displayName"] = displayName;

    uint32_t userId=0, groupId=0;
    generateUserId(userId, groupId);

    displayParams["ownerId"] = userId;
    displayParams["groupId"] = groupId;
    string displayParamsString;
    displayParams.ToString(displayParamsString);
    Core::hresult createDisplayResult = mWindowManager->CreateDisplay(displayParamsString);
    if (Core::ERROR_NONE != createDisplayResult)
    {
        errorReason = "unable to create display for application";
        return false;
    }
    return true;
}

std::pair<std::string, std::string> WindowManagerHandler::generateDisplayName()
{
    std::pair<std::string, std::string> name;

    int xdgRuntimeDirFd = -1;

    const char *xdgRuntimeDir = getenv("XDG_RUNTIME_DIR");
    if (xdgRuntimeDir)
    {
        xdgRuntimeDirFd = open(xdgRuntimeDir, O_CLOEXEC | O_DIRECTORY);
        if (xdgRuntimeDirFd < 0)
        {
            printf("failed to open XDG_RUNTIME_DIR '%s' %d\n", xdgRuntimeDir, errno);
            fflush(stdout);
        }
        else
        {
            name.first = xdgRuntimeDir;
        }
    }

    // if failed to open the dir then open the default /tmp
    if (xdgRuntimeDirFd < 0)
    {
        xdgRuntimeDirFd = open("/tmp", O_CLOEXEC | O_DIRECTORY);
        if (xdgRuntimeDirFd < 0)
        {
            printf("failed to open XDG_RUNTIME_DIR /tmp %d\n", errno);
            fflush(stdout);
            return name;
        }

        name.first = "/tmp";
    }

    std::ifstream f("/tmp/specchange");
    if (f.good())
    {
        name.second.assign("testdisplay");
        f.close();
    }
    else
    {	    
    // generate  a new random name
    for (int attempt = 0; attempt < 5; attempt++)
    {
        // a bit overkill for generating a random file name, but gcc whines
        // about mktemp
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 25);

        char buf[] = "westeros-XXXXXX";
        for (size_t i = 0; i < 6; i++)
            buf[9 + i] = 'a' + dis(gen);

        // sanity check we don't already have a socket with the same name
        if (faccessat(xdgRuntimeDirFd, buf, F_OK, 0) != 0)
        {
            name.second.assign(buf);
            break;
        }
    }
    }
    if (close(xdgRuntimeDirFd) < 0)
    {
        printf("failed to close XDG_RUNTIME_DIR \n");
        fflush(stdout);
    }

    return name;
}

void WindowManagerHandler::WindowManagerNotification::OnUserInactivity(const double minutes)
{
}

void WindowManagerHandler::generateUserId(uint32_t& userId, uint32_t& groupId)
{
    //TODO Generate userid and groupid in random way
    userId = 30490;
    FILE* fp = fopen("/tmp/appuid", "r");
    if (fp != NULL)
    {
        char* line = NULL;
        size_t len = 0;
        while ((getline(&line, &len, fp)) != -1)
        {
            userId = atoi(line);
            break;
        }
        fclose(fp);
    }
    groupId = 30000;
}

} // namespace Plugin
} // namespace WPEFramework
