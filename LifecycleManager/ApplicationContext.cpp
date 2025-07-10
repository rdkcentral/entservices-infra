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

#include "ApplicationContext.h"
#include "State.h"
#include <cstdio>

#define DEBUG_PRINTF(fmt, ...) \
    std::printf("[DEBUG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

namespace WPEFramework
{
    namespace Plugin
    {
	ApplicationLaunchParams::ApplicationLaunchParams(): mAppId(""), mLaunchIntent(""), mLaunchArgs(""), mTargetState(Exchange::ILifecycleManager::LifecycleState::UNLOADED), mRuntimeConfigObject()
        {
	}

        ApplicationContext::ApplicationContext (std::string appId): mAppInstanceId(""), mAppId(std::move(appId)), mLastLifecycleStateChangeTime(), mActiveSessionId(""), mTargetLifecycleState(), mMostRecentIntent(""), mState(nullptr), mStateChangeId(0)
        {
            DEBUG_PRINTF("ERROR: RDKEMW-2806");
            mState = (void*) new UnloadedState(this);
            sem_init(&mReachedLoadingStateSemaphore, 0, 0);
            sem_init(&mAppRunningSemaphore, 0, 0);
            sem_init(&mAppReadySemaphore, 0, 0);
            sem_init(&mFirstFrameSemaphore, 0, 0);
            sem_init(&mFirstFrameAfterResumeSemaphore, 0, 0);
            sem_init(&mAppTerminatingSemaphore, 0, 0);
            DEBUG_PRINTF("ERROR: RDKEMW-2806");
        }

	ApplicationKillParams::ApplicationKillParams(): mForce(false)
        {
	}

        ApplicationContext::~ApplicationContext()
        {
            if (nullptr != mState)
	    {
                DEBUG_PRINTF("ERROR: RDKEMW-2806");
                State* state = (State*)mState;
                delete state;
	    }
	    mState = nullptr;
        }

        void ApplicationContext::setAppInstanceId(std::string& id)
        {
            DEBUG_PRINTF("ERROR: RDKEMW-2806");
            mAppInstanceId = id;
        }

        void ApplicationContext::setActiveSessionId(std::string& id)
        {
            DEBUG_PRINTF("ERROR: RDKEMW-2806");
            mActiveSessionId = id;
        }

        void ApplicationContext::setMostRecentIntent(const std::string& intent)
        {
            DEBUG_PRINTF("ERROR: RDKEMW-2806");
            mMostRecentIntent = intent;
        }

        void ApplicationContext::setLastLifecycleStateChangeTime(timespec changeTime)
	{
            DEBUG_PRINTF("ERROR: RDKEMW-2806");
            mLastLifecycleStateChangeTime = changeTime;
	}

        void ApplicationContext::setState(void* state)
	{
            DEBUG_PRINTF("ERROR: RDKEMW-2806");
            mState = state;
	}

	void ApplicationContext::setTargetLifecycleState(Exchange::ILifecycleManager::LifecycleState state)
	{
            DEBUG_PRINTF("ERROR: RDKEMW-2806");
            mTargetLifecycleState = state;
        }

        void ApplicationContext::setStateChangeId(uint32_t id)
	{
        DEBUG_PRINTF("ERROR: RDKEMW-2806");
            mStateChangeId = id;		
	}

        void ApplicationContext::setApplicationLaunchParams(const string& appId, const string& launchIntent, const string& launchArgs, Exchange::ILifecycleManager::LifecycleState targetState, const WPEFramework::Exchange::RuntimeConfig& runtimeConfigObject)
	{
        DEBUG_PRINTF("ERROR: RDKEMW-2806");
            mLaunchParams.mAppId = appId;
            mLaunchParams.mLaunchIntent = launchIntent;
            mLaunchParams.mLaunchArgs = launchArgs;
            mLaunchParams.mTargetState = targetState;
            mLaunchParams.mRuntimeConfigObject = runtimeConfigObject;
            DEBUG_PRINTF("ERROR: RDKEMW-2806");
	}

        void ApplicationContext::setApplicationKillParams(bool force)
	{
        DEBUG_PRINTF("ERROR: RDKEMW-2806");
            mKillParams.mForce = force;
        }

	std::string ApplicationContext::getAppId()
	{
        DEBUG_PRINTF("ERROR: RDKEMW-2806");
            return mAppId;
	}

	std::string ApplicationContext::getAppInstanceId()
	{
        DEBUG_PRINTF("ERROR: RDKEMW-2806");
            return mAppInstanceId;
	}

	Exchange::ILifecycleManager::LifecycleState ApplicationContext::getCurrentLifecycleState()
	{
        DEBUG_PRINTF("ERROR: RDKEMW-2806");
            State* state = (State*)mState;
            return state->getValue();
	}

        timespec ApplicationContext::getLastLifecycleStateChangeTime()
	{
        DEBUG_PRINTF("ERROR: RDKEMW-2806");
	    return mLastLifecycleStateChangeTime;	
	}

        std::string ApplicationContext::getActiveSessionId()
	{
        DEBUG_PRINTF("ERROR: RDKEMW-2806");
            return mActiveSessionId;
	}

	Exchange::ILifecycleManager::LifecycleState ApplicationContext::getTargetLifecycleState()
	{
        DEBUG_PRINTF("ERROR: RDKEMW-2806");
            return mTargetLifecycleState;
        }

        std::string ApplicationContext::getMostRecentIntent()
        {
            DEBUG_PRINTF("ERROR: RDKEMW-2806");
            return mMostRecentIntent;
        }

        void* ApplicationContext::getState()
        {
            DEBUG_PRINTF("ERROR: RDKEMW-2806");
            return mState;
        }

        uint32_t ApplicationContext::getStateChangeId()
	{
        DEBUG_PRINTF("ERROR: RDKEMW-2806");
            return mStateChangeId;
	}

	ApplicationLaunchParams& ApplicationContext::getApplicationLaunchParams()
	{
        DEBUG_PRINTF("ERROR: RDKEMW-2806");
            return mLaunchParams;
	}

	ApplicationKillParams& ApplicationContext::getApplicationKillParams()
	{
        DEBUG_PRINTF("ERROR: RDKEMW-2806");
            return mKillParams;
	}
    } /* namespace Plugin */
} /* namespace WPEFramework */
