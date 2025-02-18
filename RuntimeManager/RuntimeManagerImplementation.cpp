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

#include "RuntimeManagerImplementation.h"

namespace WPEFramework
{
    namespace Plugin
    {
        SERVICE_REGISTRATION(RuntimeManagerImplementation, 1, 0);
        RuntimeManagerImplementation* RuntimeManagerImplementation::_instance = nullptr;

        RuntimeManagerImplementation::RuntimeManagerImplementation()
        : mRuntimeManagerImplLock()
        , mCurrentservice(nullptr)
        {
            LOGINFO("Create RuntimeManagerImplementation Instance");
            if (nullptr == RuntimeManagerImplementation::_instance)
            {
                RuntimeManagerImplementation::_instance = this;
            }
        }

        RuntimeManagerImplementation* RuntimeManagerImplementation::getInstance()
        {
            return _instance;
        }

        RuntimeManagerImplementation::~RuntimeManagerImplementation()
        {
            LOGINFO("Delete RuntimeManagerImplementation Instance");
            if (nullptr != mCurrentservice)
            {
               mCurrentservice->Release();
               mCurrentservice = nullptr;
            }
        }

        Core::hresult RuntimeManagerImplementation::Register(Exchange::IRuntimeManager::INotification *notification)
        {
            ASSERT (nullptr != notification);
        
            mRuntimeManagerImplLock.Lock();
        
            /* Make sure we can't register the same notification callback multiple times */
            if (std::find(mRuntimeManagerNotification.begin(), mRuntimeManagerNotification.end(), notification) == mRuntimeManagerNotification.end())
            {
                LOGINFO("Register notification");
                mRuntimeManagerNotification.push_back(notification);
                notification->AddRef();
            }
        
            mRuntimeManagerImplLock.Unlock();
        
            return Core::ERROR_NONE;
        }
        
        Core::hresult RuntimeManagerImplementation::Unregister(Exchange::IRuntimeManager::INotification *notification )
        {
            Core::hresult status = Core::ERROR_GENERAL;
        
            ASSERT (nullptr != notification);
        
            mRuntimeManagerImplLock.Lock();
        
            /* Make sure we can't unregister the same notification callback multiple times */
            auto itr = std::find(mRuntimeManagerNotification.begin(), mRuntimeManagerNotification.end(), notification);
            if (itr != mRuntimeManagerNotification.end())
            {
                (*itr)->Release();
                LOGINFO("Unregister notification");
                mRuntimeManagerNotification.erase(itr);
                status = Core::ERROR_NONE;
            }
            else
            {
                LOGERR("notification not found");
            }
        
            mRuntimeManagerImplLock.Unlock();
        
            return status;
        }

        uint32_t RuntimeManagerImplementation::Configure(PluginHost::IShell* service)
        {
            uint32_t result = Core::ERROR_GENERAL;

            if (service != nullptr)
            {
                mCurrentservice = service;
                mCurrentservice->AddRef();

                LOGINFO("Create Thread for calling OCI Container methods\n");

                result = Core::ERROR_NONE;
            }
            else
            {
                LOGERR("service is null \n");
            }
        
            return result;
        }

        void RuntimeManagerImplementation::dispatchEvent(EventNames event, const JsonValue &params)
        {
            Core::IWorkerPool::Instance().Submit(Job::Create(this, event, params));
        }
        
        void RuntimeManagerImplementation::Dispatch(EventNames event, const JsonValue params)
        {
             mRuntimeManagerImplLock.Lock();
        
             std::list<Exchange::IRuntimeManager::INotification*>::const_iterator index(mRuntimeManagerNotification.begin());
        
             switch(event)
             {
                 case RUNTIME_MANAGER_EVENT_STATECHANGED:
                     while (index != mRuntimeManagerNotification.end())
                     {
                         string appId("");
                         RuntimeState state = RUNTIME_STATE_STARTING;
                         (*index)->OnStateChanged(appId, state);
                         ++index;
                     }
                 break;
        
                 default:
                     LOGWARN("Event[%u] not handled", event);
                     break;
             }
        
             mRuntimeManagerImplLock.Unlock();
        }
        
        Core::hresult RuntimeManagerImplementation::Run(const string& appInstanceId, const string& appPath, const string& runtimePath, IStringIterator* const& envVars, const uint32_t userId, const uint32_t groupId, IValueIterator* const& ports, IStringIterator* const& paths, IStringIterator* const& debugSettings)
        {
            Core::hresult status = Core::ERROR_NONE;

            LOGINFO("Entered Run Implementation");

            return status;
        }
        
        Core::hresult RuntimeManagerImplementation::Hibernate(const string& appInstanceId)
        {
            Core::hresult status = Core::ERROR_NONE;

            LOGINFO("Entered Hibernate Implementation");

            return status;
        }
        
        Core::hresult RuntimeManagerImplementation::Wake(const string& appInstanceId, const RuntimeState runtimeState)
        {
            Core::hresult status = Core::ERROR_NONE;

            LOGINFO("Entered Wake Implementation");

            return status;
        }
        
        Core::hresult RuntimeManagerImplementation::Suspend(const string& appInstanceId)
        {
            Core::hresult status = Core::ERROR_NONE;

            LOGINFO("Entered Suspend Implementation");

            return status;
        }
        
        Core::hresult RuntimeManagerImplementation::Resume(const string& appInstanceId)
        {
            Core::hresult status = Core::ERROR_NONE;

            LOGINFO("Entered Resume Implementation");

            return status;
        }
        
        Core::hresult RuntimeManagerImplementation::Terminate(const string& appInstanceId)
        {
            Core::hresult status = Core::ERROR_NONE;

            LOGINFO("Entered Terminate Implementation");

            return status;
        }
        
        Core::hresult RuntimeManagerImplementation::Kill(const string& appInstanceId)
        {
            Core::hresult status = Core::ERROR_NONE;

            LOGINFO("Entered Kill Implementation");

            return status;
        }

        Core::hresult RuntimeManagerImplementation::GetInfo(const string& appInstanceId, string& info) const
        {
            Core::hresult status = Core::ERROR_NONE;

            LOGINFO("Entered GetInfo Implementation");

            return status;
        }

        Core::hresult RuntimeManagerImplementation::Annotate(const string& appInstanceId, const string& key, const string& value)
        {
            Core::hresult status = Core::ERROR_NONE;

            LOGINFO("Entered Annotate Implementation");

            return status;
        }

        Core::hresult RuntimeManagerImplementation::Mount()
        {
            Core::hresult status = Core::ERROR_NONE;

            LOGINFO("Entered Mount Implementation");

            return status;
        }

        Core::hresult RuntimeManagerImplementation::Unmount()
        {
            Core::hresult status = Core::ERROR_NONE;

            LOGINFO("Entered Unmount Implementation");

            return status;
        }
    } /* namespace Plugin */
} /* namespace WPEFramework */
