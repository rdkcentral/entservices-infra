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

#ifndef __THUNDERUTILS_H__
#define __THUNDERUTILS_H__
#define SERVER_DETAILS  "127.0.0.1:9998"
#include "UtilsLogging.h"
using namespace WPEFramework;
using namespace std;
class ThunderUtils {
    public:
    static uint32_t getServiceState(PluginHost::IShell* shell, const string& callsign, PluginHost::IShell::state& state)
            {
                LOGDBG("Checking Service state");
                uint32_t result;
                auto interface = shell->QueryInterfaceByCallsign<PluginHost::IShell>(callsign);
                if (interface == nullptr) {
                    result = Core::ERROR_UNAVAILABLE;
                    LOGERR("No interface for %s", callsign.c_str());
                } else {
                    LOGDBG("Got Valid instance");
                    interface->AddRef();
                    result = Core::ERROR_NONE;
                    LOGDBG("Got state");
                    state = interface->State();
                    interface->Release();
                }
                return result;
            }

    static std::shared_ptr<WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>> getThunderControllerClient(std::string callsign="")
    {

        Core::SystemInfo::SetEnvironment(_T("THUNDER_ACCESS"), (_T(SERVER_DETAILS)));
        std::shared_ptr<WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>> thunderClient = make_shared<WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>>(callsign.c_str(), "", false);
        return thunderClient;
    }
};
#endif

