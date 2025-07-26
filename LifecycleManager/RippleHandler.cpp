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

#include "RippleHandler.h"
#include "UtilsLogging.h"
#include "tracing/Logging.h"
#include <cstdio>

#define DEBUG_PRINTF(fmt, ...) \
    std::printf("[DEBUG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

#define FIREBOLT_ENDPOINT "ws://127.0.0.1:3474/jsonrpc"

namespace WPEFramework {
namespace Plugin {

RippleHandler::RippleHandler()
: mWSEndPoint(nullptr), mRippleConnectionId(-1), mFireboltEndpoint(FIREBOLT_ENDPOINT), mEventHandler(nullptr)
{
    LOGINFO("Create RippleHandler Instance");
}

RippleHandler::~RippleHandler()
{
    LOGINFO("Delete RippleHandler Instance");
}

bool RippleHandler::initialize(IEventHandler* eventHandler)
{
    DEBUG_PRINTF("ERROR: RDKEMW-2806");
    mEventHandler = eventHandler;	
    mWSEndPoint = new WebSocketEndPoint();
    mWSEndPoint->initialize();
    DEBUG_PRINTF("ERROR: RDKEMW-2806");
    return true;
}

void RippleHandler::terminate()
{
    DEBUG_PRINTF("ERROR: RDKEMW-2806");
    mEventHandler = nullptr;
    if (nullptr != mWSEndPoint)
    {
        DEBUG_PRINTF("ERROR: RDKEMW-2806");
        mWSEndPoint->deinitialize();
        delete mWSEndPoint;
    }
    DEBUG_PRINTF("ERROR: RDKEMW-2806");
    mWSEndPoint = nullptr;
    mRippleConnectionId = -1;
    DEBUG_PRINTF("ERROR: RDKEMW-2806");
}

bool RippleHandler::sendIntent(std::string& appId, const std::string& intent, std::string& errorReason)
{
    if (mRippleConnectionId == -1)
    {
        mRippleConnectionId = mWSEndPoint->connect(mFireboltEndpoint, true);
        if (mRippleConnectionId == -1)
        {
            return false;
        }
    }

    //MESSAGE FORMAT
    //std::string sessionRequestMessage = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"LifecycleManagement.session\",\"params\":{\"session\":{\"app\":{\"id\":\"app1\",\"url\":\"https://www.google.com\"},\"runtime\":{\"id\":\"WebKitBrowser-1\"},\"launch\":{\"intent\":{\"action\":\"launch\",\"context\":{\"source\":\"user\"}}}}}}";

    std::string sessionRequestMessagePre = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"LifecycleManagement.session\",\"params\":{\"session\":{\"app\":{\"id\":\"";
    std::string sessionRequestMessagePost = "\",\"url\":\"\"},\"runtime\":{\"id\":\"WebKitBrowser-1\"},\"launch\":{\"intent\":";
    std::stringstream ss;
    ss << sessionRequestMessagePre << appId << sessionRequestMessagePost << intent << "}}}}";
    std::string response("");
    bool ret = mWSEndPoint->send(mRippleConnectionId, ss.str(), response);
    std::cout << "Response from ripple is " << ret << ":" << response << std::endl;
    if (!ret)
    {
        errorReason = std::move(response);
    }
    return ret;
}

} // namespace Plugin
} // namespace WPEFramework
