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

#pragma once

#include <interfaces/ILifecycleManager.h>
#include <map>
#include <time.h>
#include <string>

namespace WPEFramework
{
    namespace Plugin
    {
        struct ApplicationLaunchParams
	{
            ApplicationLaunchParams();
            string mAppId;
	    string mAppPath;
	    string mAppConfig;
	    string mRuntimeAppId;
	    string mRuntimePath;
	    string mRuntimeConfig;
	    string mLaunchIntent;
	    string mEnvironmentVars;
	    bool   mEnableDebugger;
	    string mLaunchArgs;
	};

        struct ApplicationKillParams
	{
            ApplicationKillParams();
	    bool mForce;
        };

        class ApplicationContext
	{
            public:
                ApplicationContext (std::string appId);
                virtual ~ApplicationContext ();

                void setAppInstanceId(std::string& id);
                void setActiveSessionId(std::string& id);
                void setMostRecentIntent(const std::string& intent);
                void setLastLifecycleStateChangeTime(timespec changeTime);
		void setState(void* state);
                void setTargetLifecycleState(Exchange::ILifecycleManager::LifecycleState state);
                void setStateChangeId(uint32_t id);
                void setApplicationLaunchParams(const string& appId, const string& appPath, const string& appConfig, const string& runtimeAppId, const string& runtimePath, const string& runtimeConfig, const string& launchIntent, const string& environmentVars, const bool enableDebugger, const string& launchArgs);
                void setApplicationKillParams(bool force);

                void* getState();
                std::string getAppId();
                std::string getAppInstanceId();
		Exchange::ILifecycleManager::LifecycleState getCurrentLifecycleState();
                timespec getLastLifecycleStateChangeTime();
                std::string getActiveSessionId();
		Exchange::ILifecycleManager::LifecycleState getTargetLifecycleState();
                std::string getMostRecentIntent();
                uint32_t getStateChangeId();
                ApplicationLaunchParams& getApplicationLaunchParams();
                ApplicationKillParams& getApplicationKillParams();

	    private:
                std::string mAppInstanceId;
		std::string mAppId;
                timespec mLastLifecycleStateChangeTime;
                std::string mActiveSessionId;
                Exchange::ILifecycleManager::LifecycleState mTargetLifecycleState;
                std::string mMostRecentIntent;
                void* mState;
                uint32_t mStateChangeId;
                ApplicationLaunchParams mLaunchParams;
                ApplicationKillParams mKillParams;
        };
    } /* namespace Plugin */
} /* namespace WPEFramework */
