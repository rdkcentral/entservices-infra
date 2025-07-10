/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
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
**/

#include "RequestHandler.h"
#include "StateTransitionHandler.h"
#include <interfaces/IRDKWindowManager.h>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cstdio>

#define DEBUG_PRINTF(fmt, ...) \
    std::printf("[DEBUG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

namespace WPEFramework
{
    namespace Plugin
    {
        RequestHandler* RequestHandler::mInstance = nullptr;

        RequestHandler* RequestHandler::getInstance()
	{
            if (nullptr == mInstance)
            {
                mInstance = new RequestHandler();
            }
            return mInstance;
	}

        RequestHandler::RequestHandler(): mRuntimeManagerHandler(nullptr), mWindowManagerHandler(nullptr), mRippleHandler(nullptr), mEventHandler(nullptr)
	{
	}

        RequestHandler::~RequestHandler()
	{
	}

        bool RequestHandler::initialize(PluginHost::IShell* service, IEventHandler* eventHandler)
	{
        DEBUG_PRINTF("ERROR: RDKEMW-2806");
	    bool ret = false;
	    mEventHandler = eventHandler;	
            mRippleHandler = new RippleHandler();
            ret = mRippleHandler->initialize(eventHandler);
	    if (!ret)
	    {
                printf("unable to initialize with ripple \n");
		fflush(stdout);
		return ret;
	    }
            DEBUG_PRINTF("ERROR: RDKEMW-2806");
            mRuntimeManagerHandler = new RuntimeManagerHandler();
            ret = mRuntimeManagerHandler->initialize(service, eventHandler);
	    if (!ret)
	    {
                printf("unable to initialize with runtimemanager \n");
		fflush(stdout);
		return ret;
	    }
            DEBUG_PRINTF("ERROR: RDKEMW-2806");
            mWindowManagerHandler = new WindowManagerHandler();
            ret = mWindowManagerHandler->initialize(service, eventHandler);
	    if (!ret)
	    {
                printf("unable to initialize with windowmanager \n");
		fflush(stdout);
		return ret;
	    }
            DEBUG_PRINTF("ERROR: RDKEMW-2806");
            StateTransitionHandler::getInstance()->initialize();
            DEBUG_PRINTF("ERROR: RDKEMW-2806");
            return ret;
	}

        void RequestHandler::terminate()
	{
                DEBUG_PRINTF("ERROR: RDKEMW-2806");
            StateTransitionHandler::getInstance()->terminate();
            mRippleHandler->terminate();
            if (mWindowManagerHandler)
            {
                DEBUG_PRINTF("ERROR: RDKEMW-2806");
                mWindowManagerHandler->terminate();
                delete mWindowManagerHandler;
		mWindowManagerHandler = nullptr;
            }

            if (mRuntimeManagerHandler)
            {
                DEBUG_PRINTF("ERROR: RDKEMW-2806");
                mRuntimeManagerHandler->deinitialize();
                delete mRuntimeManagerHandler;
		mRuntimeManagerHandler = nullptr;
            }
	}

        bool RequestHandler::launch(ApplicationContext* context, const string& launchIntent, const Exchange::ILifecycleManager::LifecycleState targetLifecycleState, string& errorReason)
	{
        DEBUG_PRINTF("ERROR: RDKEMW-2806");
            bool success = updateState(context, targetLifecycleState, errorReason);
            return success;
	}

        bool RequestHandler::terminate(ApplicationContext* context, bool force, string& errorReason)
	{
            bool success = false;
	    success = updateState(context, Exchange::ILifecycleManager::LifecycleState::TERMINATING, errorReason);
            return success;
	}

        bool RequestHandler::sendIntent(ApplicationContext* context, const string& intent, string& errorReason)
	{
	   std::string appId = context->getAppId();
           bool ret = mRippleHandler->sendIntent(appId, intent, errorReason);
           if (ret)
	   {
               context->setMostRecentIntent(intent);
	   }
           return ret;
	}

	bool RequestHandler::updateState(ApplicationContext* context, Exchange::ILifecycleManager::LifecycleState state, string& errorReason)
	{
           DEBUG_PRINTF("ERROR: RDKEMW-2806");
           StateTransitionRequest request(context, state);
           StateTransitionHandler::getInstance()->addRequest(request); 
           return true;
	}

        RuntimeManagerHandler* RequestHandler::getRuntimeManagerHandler()
	{
            return mRuntimeManagerHandler;
	}

        WindowManagerHandler* RequestHandler::getWindowManagerHandler()
	{
            return mWindowManagerHandler;
	}

        IEventHandler* RequestHandler::getEventHandler()
	{
            DEBUG_PRINTF("ERROR: RDKEMW-2806");
            return mEventHandler;
	}

    } /* namespace Plugin */
} /* namespace WPEFramework */
