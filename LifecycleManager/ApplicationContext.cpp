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

namespace WPEFramework
{
    namespace Plugin
    {
	ApplicationLaunchParams::ApplicationLaunchParams(): mAppId(""), mAppPath(""), mAppConfig(""), mRuntimeAppId(""), mRuntimePath(""), mRuntimeConfig(""), mLaunchIntent(""), mEnvironmentVars(""), mEnableDebugger(false), mLaunchArgs("")
        {
	}

        ApplicationContext::ApplicationContext (std::string appId): mAppInstanceId(""), mAppId(appId), mLastLifecycleStateChangeTime(), mActiveSessionId(""), mTargetLifecycleState(), mMostRecentIntent(""), mState(nullptr), mStateChangeId(0)
        {
            mState = (void*) new LoadingState(this);
        }

	ApplicationKillParams::ApplicationKillParams(): mForce(false)
        {
	}

        ApplicationContext::~ApplicationContext()
        {
            if (nullptr != mState)
	    {
                State* state = (State*)mState;
                delete state;
	    }
	    mState = nullptr;
        }

        void ApplicationContext::setAppInstanceId(std::string& id)
        {
            mAppInstanceId = id;
        }

        void ApplicationContext::setActiveSessionId(std::string& id)
        {
            mActiveSessionId = id;
        }

        void ApplicationContext::setMostRecentIntent(const std::string& intent)
        {
            mMostRecentIntent = intent;
        }

        void ApplicationContext::setLastLifecycleStateChangeTime(timespec changeTime)
	{
            mLastLifecycleStateChangeTime = changeTime;
	}

        void ApplicationContext::setState(void* state)
	{
            mState = state;
	}

	void ApplicationContext::setTargetLifecycleState(Exchange::ILifecycleManager::LifecycleState state)
	{
            mTargetLifecycleState = state;
        }

        void ApplicationContext::setStateChangeId(uint32_t id)
	{
            mStateChangeId = id;		
	}

        void ApplicationContext::setApplicationLaunchParams(const string& appId, const string& appPath, const string& appConfig, const string& runtimeAppId, const string& runtimePath, const string& runtimeConfig, const string& launchIntent, const string& environmentVars, const bool enableDebugger, const string& launchArgs)
	{
            mLaunchParams.mAppId = appId;
            mLaunchParams.mAppPath = appPath;
            mLaunchParams.mAppConfig = appConfig;
            mLaunchParams.mRuntimeAppId = runtimeAppId;
            mLaunchParams.mRuntimePath = runtimePath;
            mLaunchParams.mRuntimeConfig = runtimeConfig;
            mLaunchParams.mLaunchIntent = launchIntent;
            mLaunchParams.mEnvironmentVars = environmentVars;
            mLaunchParams.mEnableDebugger = enableDebugger;
            mLaunchParams.mLaunchArgs = launchArgs;
	}

        void ApplicationContext::setApplicationKillParams(bool force)
	{
            mKillParams.mForce = force;
        }

	std::string ApplicationContext::getAppId()
	{
            return mAppId;
	}

	std::string ApplicationContext::getAppInstanceId()
	{
            return mAppInstanceId;
	}

	Exchange::ILifecycleManager::LifecycleState ApplicationContext::getCurrentLifecycleState()
	{
            State* state = (State*)mState;
            return state->getValue();
	}

        timespec ApplicationContext::getLastLifecycleStateChangeTime()
	{
	    return mLastLifecycleStateChangeTime;	
	}

        std::string ApplicationContext::getActiveSessionId()
	{
            return mActiveSessionId;
	}

	Exchange::ILifecycleManager::LifecycleState ApplicationContext::getTargetLifecycleState()
	{
            return mTargetLifecycleState;
        }

        std::string ApplicationContext::getMostRecentIntent()
        {
            return mMostRecentIntent;
        }

        void* ApplicationContext::getState()
        {
            return mState;
        }

        uint32_t ApplicationContext::getStateChangeId()
	{
            return mStateChangeId;
	}

	ApplicationLaunchParams& ApplicationContext::getApplicationLaunchParams()
	{
            return mLaunchParams;
	}

	ApplicationKillParams& ApplicationContext::getApplicationKillParams()
	{
            return mKillParams;
	}
    } /* namespace Plugin */
} /* namespace WPEFramework */
