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

namespace WPEFramework
{
    namespace Plugin
    {
        bool LoadingState::handle(string& errorReason)
	{
            return true;
	}

        bool InitializingState::handle(string& errorReason)
        {
            WindowManagerHandler* windowManagerHandler = RequestHandler::getInstance()->getWindowManagerHandler();
            bool result = false;
            if (nullptr != windowManagerHandler)
            {
                ApplicationContext* context = getContext();
                ApplicationLaunchParams& launchParams = context->getApplicationLaunchParams();
                result = windowManagerHandler->createDisplay(launchParams.mAppPath, launchParams.mAppConfig, launchParams.mRuntimeAppId, launchParams.mRuntimePath, launchParams.mRuntimeConfig, launchParams.mLaunchArgs, launchParams.mDisplayName, errorReason);
            }

            return result;
        }

        bool RunRequestedState::handle(string& errorReason)
	{
            bool ret = false;
            RuntimeManagerHandler* runtimeManagerHandler = RequestHandler::getInstance()->getRuntimeManagerHandler();
	    if (nullptr != runtimeManagerHandler)
	    {
                ApplicationContext* context = getContext();

                boost::uuids::uuid tag = boost::uuids::random_generator()();
                std::string generatedInstanceId =  boost::uuids::to_string(tag);
                context->setAppInstanceId(generatedInstanceId);

                ApplicationLaunchParams& launchParams = context->getApplicationLaunchParams();

                ret = runtimeManagerHandler->run(context->getAppId(), generatedInstanceId, launchParams.mAppPath, launchParams.mAppConfig, launchParams.mRuntimeAppId, launchParams.mRuntimePath, launchParams.mRuntimeConfig, launchParams.mEnvironmentVars, launchParams.mEnableDebugger, launchParams.mLaunchArgs, launchParams.mXdgRuntimeDir, launchParams.mDisplayName, errorReason);
	    }
            return ret;
	}

        bool RunningState::handle(string& errorReason)
	{
            return true;
	}

        bool ActivateRequestedState::handle(string& errorReason)
	{
            ApplicationContext* context = getContext();
            boost::uuids::uuid tag = boost::uuids::random_generator()();
            std::string generatedSessionId =  boost::uuids::to_string(tag);
            context->setActiveSessionId(generatedSessionId);
            return true;
	}

        bool ActiveState::handle(string& errorReason)
	{
            return true;
	}

        bool DeactivateRequestedState::handle(string& errorReason)
	{
            return true;
	}

        bool SuspendRequestedState::handle(string& errorReason)
	{
	    bool ret = false;
            RuntimeManagerHandler* runtimeManagerHandler = RequestHandler::getInstance()->getRuntimeManagerHandler();
	    if (nullptr != runtimeManagerHandler)
	    {
                ApplicationContext* context = getContext();
                ret = runtimeManagerHandler->suspend(context->getAppInstanceId(), errorReason);
	    }
            return ret;
	}

        bool SuspendedState::handle(string& errorReason)
	{
            return true;
	}

        bool ResumeRequestedState::handle(string& errorReason)
	{
	    bool ret = false;
            RuntimeManagerHandler* runtimeManagerHandler = RequestHandler::getInstance()->getRuntimeManagerHandler();
	    if (nullptr != runtimeManagerHandler)
	    {
                ApplicationContext* context = getContext();
                ret = runtimeManagerHandler->resume(context->getAppInstanceId(), errorReason);
	    }
            return ret;
	}

        bool HibernateRequestedState::handle(string& errorReason)
	{
            bool ret = false;
            RuntimeManagerHandler* runtimeManagerHandler = RequestHandler::getInstance()->getRuntimeManagerHandler();
	    if (nullptr != runtimeManagerHandler)
	    {
                ApplicationContext* context = getContext();
                ret = runtimeManagerHandler->hibernate(context->getAppInstanceId(), errorReason);
	    }
            return ret;
	}

        bool HibernatedState::handle(string& errorReason)
	{
            return true;
	}

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

        bool TerminateRequestedState::handle(string& errorReason)
	{
            bool success = false;
            RuntimeManagerHandler* runtimeManagerHandler = RequestHandler::getInstance()->getRuntimeManagerHandler();
	    if (nullptr != runtimeManagerHandler)
	    {
                ApplicationContext* context = getContext();
                ApplicationKillParams& killParams = context->getApplicationKillParams();
                if (killParams.mForce)
                {
                    success = runtimeManagerHandler->kill(context->getAppInstanceId(), errorReason);
                }
                else
                {
                    success = runtimeManagerHandler->terminate(context->getAppInstanceId(), errorReason);
                }
            }
            return success;
	}

        bool TerminateState::handle(string& errorReason)
	{
            return true;
	}

    } /* namespace Plugin */
} /* namespace WPEFramework */
