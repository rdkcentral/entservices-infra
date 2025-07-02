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

#include "StateTransitionHandler.h"
#include "StateHandler.h"
#include <thread>
#include <mutex>
#include <vector>
#include <iostream>

namespace WPEFramework
{
    namespace Plugin
    {
        static std::thread requestHandlerThread;
        std::mutex gRequestMutex;
        sem_t gRequestSemaphore;
        std::vector<std::shared_ptr<StateTransitionRequest>> gRequests;
        static bool sRunning = true;

        StateTransitionHandler* StateTransitionHandler::mInstance = nullptr;

        StateTransitionHandler* StateTransitionHandler::getInstance()
	{
            if (nullptr == mInstance)
            {
                mInstance = new StateTransitionHandler();
            }
            return mInstance;
	}

        StateTransitionHandler::StateTransitionHandler()
	{
	}

        StateTransitionHandler::~StateTransitionHandler()
	{
	}

        bool StateTransitionHandler::initialize()
	{
            StateHandler::initialize();
            sem_init(&gRequestSemaphore, 0, 0);
	    std::cout<<"--------------------topic/2806----------16>";
            requestHandlerThread = std::thread([=]() {
                bool isRunning = true;
                gRequestMutex.lock();
                isRunning = sRunning;
                gRequestMutex.unlock();
		std::cout<<"--------------------topic/2806----------17>";
                while(isRunning)
		{
		    std::cout<<"--------------------topic/2806----------18>";
                    gRequestMutex.lock();
                    while (gRequests.size() > 0)
                    {
			std::cout<<"--------------------topic/2806----------19>";
	                std::shared_ptr<StateTransitionRequest> request = gRequests.front();
                        if (!request)
                        {
			    std::cout<<"--------------------topic/2806----------20>";
                            gRequests.erase(gRequests.begin());
                            continue;
                        }
                        std::string errorReason;
                        bool success = StateHandler::changeState(*request, errorReason);
			std::cout<<"--------------------topic/2806----------21>";
                        if (!success)
                        {
                            printf("MADANA ERROR IN STATE TRANSITION ... %s\n",errorReason.c_str());
			    fflush(stdout);
                            //TODO: Decide on what to do on state transition error
                            break;
                        }
                        gRequests.erase(gRequests.begin());
			std::cout<<"--------------------topic/2806----------22>";
                    }
                    gRequestMutex.unlock();
		    std::cout<<"--------------------topic/2806----------23>";
                    sem_wait(&gRequestSemaphore);
		    std::cout<<"--------------------topic/2806----------24>";
                    gRequestMutex.lock();
                    isRunning = sRunning;
                    gRequestMutex.unlock();
		    std::cout<<"--------------------topic/2806----------25>";
                }
            });
	    return true;	
	}

	void StateTransitionHandler::terminate()
	{
            gRequestMutex.lock();
            sRunning = false;
            gRequestMutex.unlock();
            sem_post(&gRequestSemaphore);
            requestHandlerThread.join();
	}

	void StateTransitionHandler::addRequest(StateTransitionRequest& request)
	{
           //TODO: Pass contect and state as argument to function
	   std::cout<<"--------------------topic/2806----------9>";
	   std::shared_ptr<StateTransitionRequest> stateTransitionRequest = std::make_shared<StateTransitionRequest>(request.mContext, request.mTargetState);
	   std::cout<<"--------------------topic/2806----------10>";
	   gRequestMutex.lock();
	   std::cout<<"--------------------topic/2806----------11>";
           gRequests.push_back(stateTransitionRequest);
	   std::cout<<"--------------------topic/2806----------12>";
	   gRequestMutex.unlock();
	   std::cout<<"--------------------topic/2806----------13>";
           sem_post(&gRequestSemaphore);
	   std::cout<<"--------------------topic/2806----------14>";
	}

    } /* namespace Plugin */
} /* namespace WPEFramework */
