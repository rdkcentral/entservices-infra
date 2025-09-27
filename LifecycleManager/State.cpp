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

#include "StateHandler.h"
#include <interfaces/IRDKWindowManager.h>
#include "RuntimeManagerHandler.h"
#include "RequestHandler.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#define DEBUG_PRINTF(fmt, ...) \
    std::printf("[DEBUG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

namespace WPEFramework
{
    namespace Plugin
    {
        bool UnloadedState::handle(string& errorReason)
	{
        DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
            return true;
	}

        bool LoadingState::handle(string& errorReason)
	{
        DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
            ApplicationContext* context = getContext();

            boost::uuids::uuid tag = boost::uuids::random_generator()();
            std::string generatedInstanceId =  boost::uuids::to_string(tag);
            context->setAppInstanceId(generatedInstanceId);
            sem_post(&context->mReachedLoadingStateSemaphore);
            DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
            return true;
	}

        bool InitializingState::handle(string& errorReason)
        {
            DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
            RuntimeManagerHandler* runtimeManagerHandler = RequestHandler::getInstance()->getRuntimeManagerHandler();
            bool ret = false;
	    if (nullptr != runtimeManagerHandler)
	    {
            DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
                ApplicationContext* context = getContext();
                ApplicationLaunchParams& launchParams = context->getApplicationLaunchParams();
                ret = runtimeManagerHandler->run(context->getAppId(), context->getAppInstanceId(), launchParams.mLaunchArgs, launchParams.mTargetState, launchParams.mRuntimeConfigObject, errorReason);
                ret = true;
                context->mPendingEventName = "onAppRunning";
                context->mPendingStateTransition = true;
	    }
        DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
            return ret;
        }

        bool PausedState::handle(string& errorReason)
	{
        DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
	    bool ret = false;
            ApplicationContext* context = getContext();
            if (Exchange::ILifecycleManager::LifecycleState::INITIALIZING == context->getCurrentLifecycleState())
	    {
            DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
                //TODO : Remove wait for now
                //context->mPendingEventName = "";
                //context->mPendingStateTransition = true;
                //sem_wait(&context->mAppReadySemaphore);
                ret = true;
	    }
	    else if (Exchange::ILifecycleManager::LifecycleState::SUSPENDED == context->getCurrentLifecycleState())
	    {
            DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
                RuntimeManagerHandler* runtimeManagerHandler = RequestHandler::getInstance()->getRuntimeManagerHandler();
	        if (nullptr != runtimeManagerHandler)
	        {
                DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
                    //ret = runtimeManagerHandler->resume(context->getAppInstanceId(), errorReason);
                    WindowManagerHandler *windowManagerHandler = RequestHandler::getInstance()->getWindowManagerHandler();
                    if (nullptr != windowManagerHandler)
                    {
                        DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
                        ApplicationContext *context = getContext();
                        Core::hresult retValue = windowManagerHandler->enableDisplayRender(context->getAppInstanceId(), true);
                        printf("enabled display in window manager [%d] \n", retValue);
                        fflush(stdout);
                    }
                    DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
                   ret = true;
                   //TODO: Error cases
	        }
            }
	    else if (Exchange::ILifecycleManager::LifecycleState::ACTIVE == context->getCurrentLifecycleState())
	    {
            DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
                ret = true;		    
            }
            return ret;
	}

        bool ActiveState::handle(string& errorReason)
	{
        DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
            WindowManagerHandler* windowManagerHandler = RequestHandler::getInstance()->getWindowManagerHandler();
	    if (nullptr != windowManagerHandler)
	    {
            DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
                ApplicationContext* context = getContext();
                bool isRenderReady = false;
		Core::hresult ret = windowManagerHandler->renderReady(context->getAppInstanceId(), isRenderReady);
                if (Core::ERROR_NONE == ret)
		{
            DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
                    if (isRenderReady)
		    {
                DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
		        return true;	
		    }
		    else
		    {
                DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
                        context->mPendingEventName = "onFirstFrame";
                        context->mPendingStateTransition = true;
		    }
                }
		else
		{
            DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
                    return false;
                }
	    }
        DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
            return true;
	}

        bool SuspendedState::handle(string& errorReason)
	{
        DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
	    bool ret = false;
            //TODO : Remove wait for now
            //ApplicationContext* context = getContext();
            //sem_wait(&context->mAppReadySemaphore);
            RuntimeManagerHandler* runtimeManagerHandler = RequestHandler::getInstance()->getRuntimeManagerHandler();
	    if (nullptr != runtimeManagerHandler)
	    {
            DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
                ApplicationContext* context = getContext();
                if (Exchange::ILifecycleManager::LifecycleState::HIBERNATED == context->getCurrentLifecycleState())
	        {
                DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
                    ret = runtimeManagerHandler->wake(context->getAppInstanceId(), Exchange::ILifecycleManager::LifecycleState::SUSPENDED, errorReason);
	        }
                else
	        {
                DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
                    //ret = runtimeManagerHandler->suspend(context->getAppInstanceId(), errorReason);
                    WindowManagerHandler *windowManagerHandler = RequestHandler::getInstance()->getWindowManagerHandler();
                    if (nullptr != windowManagerHandler)
                    {
                        DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
                        ApplicationContext *context = getContext();
                        Core::hresult retValue = windowManagerHandler->enableDisplayRender(context->getAppInstanceId(), false);
                        printf("disabled display in window manager [%d] \n", retValue);
                        fflush(stdout);
                        ret = true;
                    }
                    DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
                   //TODO: Handle error cases
                }
	    }
        DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
            return ret;
	}

        bool HibernatedState::handle(string& errorReason)
	{
        DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
            bool ret = false;
            RuntimeManagerHandler* runtimeManagerHandler = RequestHandler::getInstance()->getRuntimeManagerHandler();
	    if (nullptr != runtimeManagerHandler)
	    {
            DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
                ApplicationContext* context = getContext();
                ret = runtimeManagerHandler->hibernate(context->getAppInstanceId(), errorReason);
	    }
        DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
            return ret;
	}
/*
        bool WakeRequestedState::handle(string& errorReason)
        {
            bool ret = false;
            RuntimeManagerHandler* runtimeManagerHandler = RequestHandler::getInstance()->getRuntimeManagerHandler();
            if (nullptr != runtimeManagerHandler)
            {
                ApplicationContext* context = getContext();
                ret = runtimeManagerHandler->wake(context->getAppInstanceId(), context->getTargetLifecycleState(), errorReason);
            }
            return ret;
        }
*/

        bool TerminatingState::handle(string& errorReason)
        {
            DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
            bool success = false;
            RuntimeManagerHandler* runtimeManagerHandler = RequestHandler::getInstance()->getRuntimeManagerHandler();
            if (nullptr != runtimeManagerHandler)
            {
                DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
                ApplicationContext* context = getContext();
                ApplicationKillParams& killParams = context->getApplicationKillParams();
                if (killParams.mForce)
                {
                    DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
                    success = runtimeManagerHandler->kill(context->getAppInstanceId(), errorReason);
                }
                else
                {
                    DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
                    success = runtimeManagerHandler->terminate(context->getAppInstanceId(), errorReason);
                }
                if(success)
                {
                    DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
                    context->mPendingEventName = "onAppTerminating";
                    context->mPendingStateTransition = true;
                }
                DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
            }
            DEBUG_PRINTF("------------------------------- ISHVAR 2806 --------------------------------------");
            return success;
        }
    }
} /* namespace WPEFramework */
