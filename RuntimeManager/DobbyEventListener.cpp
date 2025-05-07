/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2025 RDK Management
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

#include "DobbyEventListener.h"
#include "UtilsLogging.h"
#include "tracing/Logging.h"
#include <sstream>

namespace WPEFramework {
    namespace Plugin {

        DobbyEventListener::DobbyEventListener()
            : mOCIContainerNotification(this),mEventHandler(nullptr)
        {
            LOGINFO("Creating DobbyEventListener instance");
        }

        DobbyEventListener::~DobbyEventListener()
        {
            LOGINFO("Delete DobbyEventListener instance");
        }

        bool DobbyEventListener::initialize(PluginHost::IShell* service, IEventHandler* eventHandler, Exchange::IOCIContainer* ociContainerObject)
        {
            bool ret = false;
            mEventHandler = eventHandler;
            mOCIContainerObject = ociContainerObject;
            if (nullptr == mOCIContainerObject)
            {
                LOGERR("Failed to get OCIContainer object");
            }
            else
            {
                Core::hresult registerResult = mOCIContainerObject->Register(&mOCIContainerNotification);
                if(Core::ERROR_NONE != registerResult)
                {
                    LOGERR("Failed to register OCIContainerNotification");
                    return false;
                }
                else
                {
                    LOGINFO("OCIContainerNotification registered successfully");
                }
                ret = true;
            }
            return ret;
        }

        bool DobbyEventListener::deinitialize()
        {
            if (nullptr != mOCIContainerObject)
            {
                Core::hresult unregisterResult = mOCIContainerObject->Unregister(&mOCIContainerNotification);
                if(Core::ERROR_NONE != unregisterResult)
                {
                    LOGERR("Failed to unregister OCIContainerNotification");
                }
                mOCIContainerObject = nullptr;
            }
            return true;
        }

        void DobbyEventListener::OCIContainerNotification::OnContainerStarted(const string& containerId, const string& name)
        {
            LOGINFO("OnContainer started: %s", name.c_str());
            LOGINFO("OnContainer ID: %s", containerId.c_str());
            JsonObject data;
            data["containerId"] = containerId;
            data["name"] = name;
            data["eventName"] = "onContainerStarted";
            _parent.onEvent(data);
        }

        void DobbyEventListener::OCIContainerNotification::OnContainerStopped(const string& containerId, const string& name)
        {
            LOGINFO("Container stopped: %s", name.c_str());
            JsonObject data;
            data["containerId"] = containerId;
            data["name"] = name;
            data["eventName"] = "onContainerStopped";
            _parent.onEvent(data);
        }

        void DobbyEventListener::OCIContainerNotification::OnContainerFailed(const string& containerId, const string& name, uint32_t error)
        {
            LOGINFO("Container failed: %s", name.c_str());
            JsonObject data;
            data["containerId"] = containerId;
            data["name"] = name;
            data["errorCode"] = error;
            data["eventName"] = "onContainerFailed";
            _parent.onEvent(data);
        }

        void DobbyEventListener::OCIContainerNotification::OnContainerStateChanged(const string& containerId, Exchange::IOCIContainer::ContainerState state)
        {
            LOGINFO("Container state changed: %s", containerId.c_str());
            JsonObject data;
            data["containerId"] = containerId;
            data["state"] = static_cast<int>(state);
            data["eventName"] = "onContainerStateChanged";
            _parent.onEvent(data);
        }
        void DobbyEventListener::onEvent(JsonObject& data)
        {
            if (mEventHandler != nullptr)
            {
                mEventHandler->onOCIContainerEvent(data["name"].String(), data);
            }
            else
            {
                LOGERR("Event handler is null");
            }
        }





    }// Namespace Plugin
}// Namespace WPEFramework