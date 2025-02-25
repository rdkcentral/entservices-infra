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
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include "IEventHandler.h"
#include "RequestHandler.h"

namespace WPEFramework
{
    namespace Plugin
    {
        typedef Exchange::ILifecycleManager::LifecycleState Lifecycle;
        std::map<Exchange::ILifecycleManager::LifecycleState, std::list<Exchange::ILifecycleManager::LifecycleState>> StateHandler::mPossibleStateTransitions = std::map<Exchange::ILifecycleManager::LifecycleState, std::list<Exchange::ILifecycleManager::LifecycleState>>();
        std::map<Exchange::ILifecycleManager::LifecycleState, std::string> StateHandler::mStateStrings = std::map<Exchange::ILifecycleManager::LifecycleState, std::string>();
        uint32_t StateHandler::sStateChangeCount = 0;

        bool StateHandler::updateState(ApplicationContext* context, Exchange::ILifecycleManager::LifecycleState lifeCycleState, string& errorReason)
	{
            State* currentState = (State*) context->getState();
            bool result = false;
	    if ((nullptr != currentState) && (currentState->getValue() == lifeCycleState))
	    {
	        return true;	   
	    }
            State* newState = createState(context, lifeCycleState);
            if (nullptr != newState)
	    {
	        //context->setState(nullptr);
                result = newState->handle(errorReason);
                if (result)
		{
	           context->setState(newState);
		   delete currentState;
                }
            }
            return result;
	}

	State* StateHandler::createState(ApplicationContext* context, Exchange::ILifecycleManager::LifecycleState lifeCycleState)
	{
            State* state = nullptr;
            switch (lifeCycleState)
            {
	        case Exchange::ILifecycleManager::LifecycleState::LOADING:
                    state = new LoadingState(context);
                    break;
	        case Exchange::ILifecycleManager::LifecycleState::INITIALIZING:
                    state = new InitializingState(context);
                    break;
	        case Exchange::ILifecycleManager::LifecycleState::RUNREQUESTED:
                    state = new RunRequestedState(context);
                    break;
	        case Exchange::ILifecycleManager::LifecycleState::RUNNING:
                    state = new RunningState(context);
                    break;
	        case Exchange::ILifecycleManager::LifecycleState::ACTIVATEREQUESTED:
                    state = new ActivateRequestedState(context);
                    break;
	        case Exchange::ILifecycleManager::LifecycleState::ACTIVE:
                    state = new ActiveState(context);
                    break;
	        case Exchange::ILifecycleManager::LifecycleState::DEACTIVATEREQUESTED:
                    state = new DeactivateRequestedState(context);
                    break;
	        case Exchange::ILifecycleManager::LifecycleState::SUSPENDREQUESTED:
                    state = new SuspendRequestedState(context);
                    break;
	        case Exchange::ILifecycleManager::LifecycleState::SUSPENDED:
                    state = new SuspendedState(context);
                    break;
	        case Exchange::ILifecycleManager::LifecycleState::RESUMEREQUESTED:
                    state = new ResumeRequestedState(context);
                    break;
	        case Exchange::ILifecycleManager::LifecycleState::HIBERNATEREQUESTED:
                    state = new HibernateRequestedState(context);
                    break;
	        case Exchange::ILifecycleManager::LifecycleState::HIBERNATED:
                    state = new HibernatedState(context);
                    break;
	        case Exchange::ILifecycleManager::LifecycleState::WAKEREQUESTED:
                    state = new WakeRequestedState(context);
                    break;
    	        case Exchange::ILifecycleManager::LifecycleState::TERMINATEREQUESTED:
                    state = new TerminateRequestedState(context);
                    break;
    	        case Exchange::ILifecycleManager::LifecycleState::TERMINATING:
                    state = new TerminateState(context);
                    break;
                default:
		    state = nullptr;	
            }
            return state;
	}

        bool StateHandler::isValidTransition(Exchange::ILifecycleManager::LifecycleState start, Exchange::ILifecycleManager::LifecycleState target, std::map<Exchange::ILifecycleManager::LifecycleState, bool>& pathSequence, std::vector<Exchange::ILifecycleManager::LifecycleState>& foundPath)
        {
            bool pathPresent = false;
            if (start == target)
            {
                return true;
            }
            pathSequence[target] = true; 
            std::map<Exchange::ILifecycleManager::LifecycleState, std::list<Exchange::ILifecycleManager::LifecycleState>>::iterator transitionIter = mPossibleStateTransitions.find(target);
            if (transitionIter == mPossibleStateTransitions.end())
            {
                return false;
            }
            std::list<Exchange::ILifecycleManager::LifecycleState>& transitionList = transitionIter->second;
            for (auto iter = transitionList.begin(); iter != transitionList.end(); ++iter)
            {
                if (pathSequence.find(*iter) != pathSequence.end())
                {
                    continue;
                }

                pathPresent = isValidTransition(start, *iter, pathSequence, foundPath);
                if (pathPresent)
                {
	            foundPath.push_back(*iter);		
                    break;
                }
            }
            return pathPresent;
        }

	void StateHandler::initialize()
	{
            mPossibleStateTransitions[Lifecycle::LOADING] = std::list<Exchange::ILifecycleManager::LifecycleState>();    
            mPossibleStateTransitions[Lifecycle::INITIALIZING] = std::list<Exchange::ILifecycleManager::LifecycleState>(1, Lifecycle::LOADING); 
            mPossibleStateTransitions[Lifecycle::RUNREQUESTED] = std::list<Exchange::ILifecycleManager::LifecycleState>(1, Lifecycle::INITIALIZING);    

            mPossibleStateTransitions[Lifecycle::RUNNING] = std::list<Exchange::ILifecycleManager::LifecycleState>();
	    std::list<Exchange::ILifecycleManager::LifecycleState>& runningStatePre = mPossibleStateTransitions[Lifecycle::RUNNING];
	    runningStatePre.push_back(Lifecycle::RUNREQUESTED);
	    runningStatePre.push_back(Lifecycle::DEACTIVATEREQUESTED);
	    runningStatePre.push_back(Lifecycle::RESUMEREQUESTED);
          
            mPossibleStateTransitions[Lifecycle::ACTIVATEREQUESTED] = std::list<Exchange::ILifecycleManager::LifecycleState>();
	    std::list<Exchange::ILifecycleManager::LifecycleState>& activateRequestedStatePre = mPossibleStateTransitions[Lifecycle::ACTIVATEREQUESTED];
            //activateRequestedStatePre.push_backgLifecycle::INITIALIZING);
            activateRequestedStatePre.push_back(Lifecycle::RUNNING);

            mPossibleStateTransitions[Lifecycle::ACTIVE] = std::list<Exchange::ILifecycleManager::LifecycleState>(1, Lifecycle::ACTIVATEREQUESTED);    
            mPossibleStateTransitions[Lifecycle::DEACTIVATEREQUESTED] = std::list<Exchange::ILifecycleManager::LifecycleState>(1, Lifecycle::ACTIVE);    
            mPossibleStateTransitions[Lifecycle::SUSPENDREQUESTED] = std::list<Exchange::ILifecycleManager::LifecycleState>(1, Lifecycle::RUNNING);    

            mPossibleStateTransitions[Lifecycle::SUSPENDED] = std::list<Exchange::ILifecycleManager::LifecycleState>();
	    std::list<Exchange::ILifecycleManager::LifecycleState>& suspendedStatePre = mPossibleStateTransitions[Lifecycle::SUSPENDED];
            suspendedStatePre.push_back(Lifecycle::SUSPENDREQUESTED);
            suspendedStatePre.push_back(Lifecycle::WAKEREQUESTED);

            mPossibleStateTransitions[Lifecycle::RESUMEREQUESTED] = std::list<Exchange::ILifecycleManager::LifecycleState>(1, Lifecycle::SUSPENDED);    
            mPossibleStateTransitions[Lifecycle::HIBERNATEREQUESTED] = std::list<Exchange::ILifecycleManager::LifecycleState>(1, Lifecycle::SUSPENDED);    
            mPossibleStateTransitions[Lifecycle::HIBERNATED] = std::list<Exchange::ILifecycleManager::LifecycleState>(1, Lifecycle::HIBERNATEREQUESTED);    
            mPossibleStateTransitions[Lifecycle::WAKEREQUESTED] = std::list<Exchange::ILifecycleManager::LifecycleState>(1, Lifecycle::HIBERNATED);    

            mPossibleStateTransitions[Lifecycle::TERMINATEREQUESTED] = std::list<Exchange::ILifecycleManager::LifecycleState>();
	    std::list<Exchange::ILifecycleManager::LifecycleState>& terminateRequestedStatePre = mPossibleStateTransitions[Lifecycle::TERMINATEREQUESTED];
            terminateRequestedStatePre.push_back(Lifecycle::RUNNING);
            terminateRequestedStatePre.push_back(Lifecycle::SUSPENDED);

            mPossibleStateTransitions[Lifecycle::TERMINATING] = std::list<Exchange::ILifecycleManager::LifecycleState>(1, Lifecycle::TERMINATEREQUESTED);    

	    //state strings
            mStateStrings[Lifecycle::LOADING] = "Loading";
            mStateStrings[Lifecycle::INITIALIZING] = "Initializing"; 
            mStateStrings[Lifecycle::RUNREQUESTED] = "RunRequested";
            mStateStrings[Lifecycle::RUNNING] = "Running";
            mStateStrings[Lifecycle::ACTIVATEREQUESTED] = "ActivateRequested";
            mStateStrings[Lifecycle::ACTIVE] = "Active"; 
            mStateStrings[Lifecycle::DEACTIVATEREQUESTED] = "DeactivateRequested"; 
            mStateStrings[Lifecycle::SUSPENDREQUESTED] = "SuspendRequested"; 
            mStateStrings[Lifecycle::SUSPENDED] = "Suspended";
            mStateStrings[Lifecycle::RESUMEREQUESTED] = "ResumeRequested";
            mStateStrings[Lifecycle::HIBERNATEREQUESTED] = "HibernateRequested";
            mStateStrings[Lifecycle::HIBERNATED] = "Hibernated";
            mStateStrings[Lifecycle::WAKEREQUESTED] = "WakeRequested";
            mStateStrings[Lifecycle::TERMINATEREQUESTED] = "TerminateRequested";
            mStateStrings[Lifecycle::TERMINATING] = "Terminating";
	}

        bool StateHandler::changeState(ApplicationContext* context, Exchange::ILifecycleManager::LifecycleState lifecycleState, string& errorReason)
	{
            if (!context)
	    {
                return false;
	    }
            Exchange::ILifecycleManager::LifecycleState currentLifecycleState = context->getCurrentLifecycleState();

            if (currentLifecycleState == lifecycleState)
	    {
	        return true;
	    }

            std::vector<Exchange::ILifecycleManager::LifecycleState> statePath;
            std::map<Exchange::ILifecycleManager::LifecycleState, bool> seenPaths;
            bool isValidRequest = StateHandler::isValidTransition(currentLifecycleState, lifecycleState, seenPaths, statePath);
            if (!isValidRequest)
            {
                errorReason = "Invalid launch request in current state";
                return false;
            }
            //ensure final state is pushed here
            statePath.push_back(lifecycleState);

            bool result = false;
            IEventHandler* eventHandler = RequestHandler::getInstance()->getEventHandler();
            // start from next state
	    for (size_t stateIndex=1; stateIndex<statePath.size(); stateIndex++)
	    {
                Exchange::ILifecycleManager::LifecycleState oldLifecycleState = ((State*)context->getState())->getValue();
                result = updateState(context, statePath[stateIndex], errorReason);
                if (!result)
		{
                    break;
		}

                struct timespec stateChangeTime;
                timespec_get(&stateChangeTime, TIME_UTC);
                context->setLastLifecycleStateChangeTime(stateChangeTime);
                context->setStateChangeId(sStateChangeCount);
                sStateChangeCount++;

                if (nullptr != eventHandler)
                {
		    Exchange::ILifecycleManager::LifecycleState newLifecycleState = ((State*)context->getState())->getValue();
                    JsonObject eventData;
                    eventData["appId"] = context->getAppId();
                    eventData["appInstanceId"] = context->getAppInstanceId();
                    eventData["oldLifecycleState"] = (uint32_t)oldLifecycleState;
                    eventData["newLifecycleState"] = (uint32_t)newLifecycleState;
                    if (newLifecycleState == Exchange::ILifecycleManager::LifecycleState::ACTIVATEREQUESTED)
                    {
                        eventData["navigationIntent"] = context->getMostRecentIntent();
                    }
                    eventData["errorReason"] = errorReason;
                    eventHandler->onStateChangeEvent(eventData);
                }
	    }
	    return result;
	}

    } /* namespace Plugin */
} /* namespace WPEFramework */
