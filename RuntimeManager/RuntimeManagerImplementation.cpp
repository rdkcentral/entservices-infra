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
#include <errno.h>

static bool sRunning = false;

namespace WPEFramework
{
    namespace Plugin
    {
        SERVICE_REGISTRATION(RuntimeManagerImplementation, 1, 0);
        RuntimeManagerImplementation* RuntimeManagerImplementation::_instance = nullptr;

        RuntimeManagerImplementation::RuntimeManagerImplementation()
        : mRuntimeManagerImplLock()
        , mContainerLock()
        , mCurrentservice(nullptr)
        , mContainerWorkerThread()
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
            LOGINFO("Call RuntimeManagerImplementation destructor");

            setRunningState(false);

            mContainerQueueCV.notify_all();

            if (mContainerWorkerThread.joinable())
            {
                mContainerWorkerThread.join();
                LOGINFO("Container Worker Thread joined successfully");
            }

            mContainerLock.lock();
            if (!mContainerRequest.empty())
            {
                mContainerRequest.clear();
            }
            mContainerLock.unlock();

            if (nullptr != mCurrentservice)
            {
               mCurrentservice->Release();
               mCurrentservice = nullptr;
            }
        }

        void RuntimeManagerImplementation::setRunningState(bool state)
        {
            Core::SafeSyncType<Core::CriticalSection> lock(mRuntimeManagerImplLock);
            sRunning = state;
        }

        bool RuntimeManagerImplementation::getRunningState()
        {
            Core::SafeSyncType<Core::CriticalSection> lock(mRuntimeManagerImplLock);
            return sRunning;
        }

        WPEFramework::Plugin::RuntimeManagerImplementation::OCIContainerRequest::OCIContainerRequest(
            OCIRequestType type, const std::string& containerId,
            const std::string& dobbySpec, const std::string& command, const std::string& westerosSocket)
            : mRequestType(type),
              mContainerId(containerId),
              mDobbySpec(dobbySpec),
              mCommand(command),
              mWesterosSocket(westerosSocket),
              mResult(Core::ERROR_GENERAL),
              mDescriptor(0),
              mSuccess(false),
              mErrorReason("")
        {
            if (0 != sem_init(&mSemaphore, 0, 0))
            {
                LOGINFO("Failed to initialise semaphore");
            }
        }

        WPEFramework::Plugin::RuntimeManagerImplementation::OCIContainerRequest::OCIContainerRequest(
                OCIRequestType type, const std::string& containerId)
                : mRequestType(type),
                  mContainerId(containerId),
                  mGetInfo(""),
                  mResult(Core::ERROR_GENERAL),
                  mDescriptor(0),
                  mSuccess(false),
                  mErrorReason("")
        {
            if (0 != sem_init(&mSemaphore, 0, 0))
            {
                LOGINFO("Failed to initialise semaphore");
            }
        }

        WPEFramework::Plugin::RuntimeManagerImplementation::OCIContainerRequest::~OCIContainerRequest()
        {
            if (0 != sem_destroy(&mSemaphore))
            {
                LOGINFO("Failed to destroy semaphore");
            }
        }

        Core::hresult RuntimeManagerImplementation::Register(Exchange::IRuntimeManager::INotification *notification)
        {
            ASSERT (nullptr != notification);
        
            Core::SafeSyncType<Core::CriticalSection> lock(mRuntimeManagerImplLock);
        
            /* Make sure we can't register the same notification callback multiple times */
            if (std::find(mRuntimeManagerNotification.begin(), mRuntimeManagerNotification.end(), notification) == mRuntimeManagerNotification.end())
            {
                LOGINFO("Register notification");
                mRuntimeManagerNotification.push_back(notification);
                notification->AddRef();
            }

            return Core::ERROR_NONE;
        }

        Core::hresult RuntimeManagerImplementation::Unregister(Exchange::IRuntimeManager::INotification *notification )
        {
            Core::hresult status = Core::ERROR_GENERAL;

            ASSERT (nullptr != notification);

            Core::SafeSyncType<Core::CriticalSection> lock(mRuntimeManagerImplLock);

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

            return status;
        }

        void RuntimeManagerImplementation::dispatchEvent(RuntimeEventType event, const JsonValue &params)
        {
            Core::IWorkerPool::Instance().Submit(Job::Create(this, event, params));
        }

        void RuntimeManagerImplementation::Dispatch(RuntimeEventType event, const JsonValue params)
        {
            Core::SafeSyncType<Core::CriticalSection> lock(mRuntimeManagerImplLock);

            std::list<Exchange::IRuntimeManager::INotification*>::const_iterator index(mRuntimeManagerNotification.begin());

            switch (event)
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
        }

        void RuntimeManagerImplementation::OCIContainerWorkerThread(void)
        {
            Exchange::IOCIContainer* ociContainerObject = nullptr;
            std::shared_ptr<OCIContainerRequest> request = nullptr;

            /* Creating OCIContainer Object */
            if (Core::ERROR_NONE != createOCIContainerPluginObject(ociContainerObject))
            {
                LOGERR("Failed to createOCIContainerPluginObject");
            }

            while (getRunningState())
            {
                std::unique_lock<std::mutex> lock(mContainerLock);
                mContainerQueueCV.wait(lock, [this] {return !mContainerRequest.empty() || !getRunningState();});

                if (!mContainerRequest.empty())
                {
                    request = mContainerRequest.front();
                    mContainerRequest.erase(mContainerRequest.begin());
                    if (nullptr == request)
                    {
                        LOGINFO("empty request");
                        continue;
                    }

                    /* Re-attempting to create ociContainerObject if the previous attempt failed (i.e., object is null) */
                    if (nullptr == ociContainerObject)
                    {
                        if (Core::ERROR_NONE != createOCIContainerPluginObject(ociContainerObject))
                        {
                            LOGERR("Failed to create OCIContainerPluginObject");
                            request->mResult = Core::ERROR_GENERAL;
                            request->mErrorReason = "Plugin is either not activated or not available";
                        }
                    }

                    if (nullptr != ociContainerObject)
                    {
                        switch (request->mRequestType)
                        {
                            case OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_RUN:
                            {
                                if (nullptr != ociContainerObject)
                                {
                                    request->mResult = ociContainerObject->StartContainerFromDobbySpec( \
                                                                                            request->mContainerId,
                                                                                            request->mDobbySpec,
                                                                                            request->mCommand,
                                                                                            request->mWesterosSocket,
                                                                                            request->mDescriptor,
                                                                                            request->mSuccess,
                                                                                            request->mErrorReason);
                                    if (Core::ERROR_NONE != request->mResult)
                                    {
                                        LOGERR("Failed to StartContainerFromDobbySpec");
                                        request->mErrorReason = "Failed to StartContainerFromDobbySpec";
                                    }
                                }
                            }
                            break;

                            case OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_HIBERNATE:
                            {
                                if (nullptr != ociContainerObject)
                                {
                                    LOGINFO("Runtime Hibernate Method");
                                    std::string options = "";
                                    request->mResult = ociContainerObject->HibernateContainer(request->mContainerId,
                                                                                              options,
                                                                                              request->mSuccess,
                                                                                              request->mErrorReason);
                                    if (Core::ERROR_NONE != request->mResult)
                                    {
                                        LOGERR("Failed to HibernateContainer");
                                        request->mErrorReason = "Failed to HibernateContainer";
                                    }
                                }
                            }
                            break;
                            case OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_WAKE:
                            case OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_SUSPEND:
                            case OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_RESUME:
                            case OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_TERMINATE:
                            case OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_KILL:
                            case OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_GETINFO:
                            {
                                if (nullptr != ociContainerObject)
                                {
                                    LOGINFO("Runtime GetInfo Method");
                                    request->mResult = ociContainerObject->GetContainerInfo(request->mContainerId,
                                                                                            request->mGetInfo,
                                                                                            request->mSuccess,
                                                                                            request->mErrorReason);
                                    if (Core::ERROR_NONE != request->mResult)
                                    {
                                        LOGERR("Failed to GetContainerInfo");
                                        request->mErrorReason = "Failed to GetContainerInfo";
                                    }
                                }
                            }
                            break;
                            case OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_ANNONATE:
                            case OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_MOUNT:
                            case OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_UNMOUNT:
                            default:
                            {
                                LOGWARN("Unknown Method type %d", static_cast<int>(request->mRequestType));
                                request->mResult = Core::ERROR_GENERAL;
                                request->mErrorReason = "Unknown Method type";
                            }
                            break;
                        }
                    }

                    if (0 != sem_post(&request->mSemaphore))
                    {
                        LOGERR("sem_post failed. Error: %s", strerror(errno));
                    }
                }
            }

            /* Release OCI Object */
            releaseOCIContainerPluginObject(ociContainerObject);
        }

        uint32_t RuntimeManagerImplementation::Configure(PluginHost::IShell* service)
        {
            uint32_t result = Core::ERROR_GENERAL;

            if (service != nullptr)
            {
                Core::SafeSyncType<Core::CriticalSection> lock(mRuntimeManagerImplLock);

                mCurrentservice = service;
                mCurrentservice->AddRef();

                /* Set IsRunning to true */
                setRunningState(true);

                /* Create the worker thread */
                try
                {
                    mContainerWorkerThread = std::thread(&RuntimeManagerImplementation::OCIContainerWorkerThread, RuntimeManagerImplementation::getInstance());
                    LOGINFO("Container Worker thread created");
                    result = Core::ERROR_NONE;
                }
                catch (const std::system_error& e)
                {
                    LOGERR("Failed to create container worker thread: %s", e.what());
                }
            }
            else
            {
                LOGERR("service is null");
            }
            return result;
        }



        Core::hresult RuntimeManagerImplementation::createOCIContainerPluginObject(Exchange::IOCIContainer*& ociContainerObject)
        {
            #define MAX_OCI_OBJECT_CREATION_RETRIES 2

            Core::hresult status = Core::ERROR_GENERAL;
            uint8_t retryCount = 0;

            if (nullptr == mCurrentservice)
            {
                LOGERR("mCurrentservice is null");
                goto err_ret;
            }

            do
            {
                ociContainerObject = mCurrentservice->QueryInterfaceByCallsign<WPEFramework::Exchange::IOCIContainer>("org.rdk.OCIContainer");
                if (nullptr == ociContainerObject)
                {
                    LOGERR("ociContainerObject is null (Attempt %d)", retryCount + 1);
                    retryCount++;
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                }
                else
                {
                    LOGINFO("Successfully created OCI Container Object");
                    status = Core::ERROR_NONE;
                    break;
                }
            } while (retryCount < MAX_OCI_OBJECT_CREATION_RETRIES);

            if (status != Core::ERROR_NONE)
            {
                LOGERR("Failed to create OCIContainer Object after %d attempts", MAX_OCI_OBJECT_CREATION_RETRIES);
            }
err_ret:
            return status;
        }

        void RuntimeManagerImplementation::releaseOCIContainerPluginObject(Exchange::IOCIContainer*& ociContainerObject)
        {
            ASSERT(nullptr != ociContainerObject);
            if(ociContainerObject)
            {
                LOGINFO("releaseOCIContainerPluginObject\n");
                ociContainerObject->Release();
                ociContainerObject = nullptr;
            }
        }

        bool RuntimeManagerImplementation::generate(const ApplicationConfiguration& config, std::string& dobbySpec)
        {
            (void)config;

            /* TODO: Hardcoded Dobby Spec JSON to be removed after RDKEMW-1632 is ready */
            dobbySpec = R"({
                "version": "1.0",
                "cwd": "/",
                "args": ["sleep", "600"],
                "env": [],
                "user": {
                    "uid": 1000,
                    "gid": 1000
                },
                "console": {
                    "limit": 65536,
                    "path": "/tmp/container.log"
                },
                "etc": {
                    "group": ["root:x:0:"],
                    "passwd": ["root::0:0:root:/:/bin/false"]
                },
                "memLimit": 41943040,
                "network": "nat",
                "mounts": []
            })";

            return true;
        }

        Core::hresult RuntimeManagerImplementation::Run(const string& appInstanceId, const string& appPath, const string& runtimePath, IStringIterator* const& envVars, const uint32_t userId, const uint32_t groupId, IValueIterator* const& ports, IStringIterator* const& paths, IStringIterator* const& debugSettings)
        {
            Core::hresult status = Core::ERROR_NONE;
            RuntimeAppInfo runtimeAppInfo;
            std::string xdgRuntimeDir = "";
            std::string waylandDisplay = "";
            std::string containerId = "com.sky.as.apps" + appInstanceId;
            std::string dobbySpec;
            int ret = -1;

            /* Below code to be enabled once dobbySpec generator ticket is ready */
            ApplicationConfiguration config;
            config.mAppInstanceId = appInstanceId;
            config.mAppPath = appPath;
            config.mRuntimePath = runtimePath;
            config.mUserId = userId;
            config.mGroupId = groupId;

            if (envVars)
            {
                std::string envVar;
                while (envVars->Next(envVar))
                {
                    config.mEnvVars.push_back(envVar);
                }
            }

            if (ports)
            {
                std::uint32_t port;
                while (ports->Next(port))
                {
                    config.mPorts.push_back(port);
                }
            }

            if (paths)
            {
                std::string path;
                while (paths->Next(path))
                {
                    config.mPaths.push_back(path);
                }
            }

            if (debugSettings)
            {
                std::string debugSetting;
                while (debugSettings->Next(debugSetting))
                {
                    config.mDebugSettings.push_back(debugSetting);
                }
            }

            LOGINFO("ApplicationConfiguration populated for InstanceId: %s", appInstanceId.c_str());

            if (!envVars)
            {
                LOGERR("envVars is null inside Run()");
            }
            else
            {
                for (const std::string &envVar : config.mEnvVars)
                {
                    LOGINFO("Inside Run() - Found: %s", envVar.c_str());
                    if (envVar.find("XDG_RUNTIME_DIR=") == 0)
                    {
                        xdgRuntimeDir = envVar.substr(strlen("XDG_RUNTIME_DIR="));
                    }
                    else if (envVar.find("WAYLAND_DISPLAY=") == 0)
                    {
                        waylandDisplay = envVar.substr(strlen("WAYLAND_DISPLAY="));
                    }

                    if (!xdgRuntimeDir.empty() && !waylandDisplay.empty())
                    {
                        break;
                    }
                }
            }

            if (xdgRuntimeDir.empty() || waylandDisplay.empty())
            {
                LOGERR("Missing required environment variables: XDG_RUNTIME_DIR=%s, WAYLAND_DISPLAY=%s",
                    xdgRuntimeDir.empty() ? "NOT FOUND" : xdgRuntimeDir.c_str(),
                    waylandDisplay.empty() ? "NOT FOUND" : waylandDisplay.c_str());
                status = Core::ERROR_GENERAL;
            }
            /* Generate dobbySpec */
            else if (false == RuntimeManagerImplementation::generate(config, dobbySpec))
            {
                LOGERR("Failed to generate dobbySpec");
                status = Core::ERROR_GENERAL;
            }
            else
            {
                /* Generated dobbySpec */
                LOGINFO("Generated dobbySpec: %s", dobbySpec.c_str());

                LOGINFO("Environment Variables: XDG_RUNTIME_DIR=%s, WAYLAND_DISPLAY=%s",
                     xdgRuntimeDir.c_str(), waylandDisplay.c_str());
                std::string westerosSocket = xdgRuntimeDir + "/" + waylandDisplay;
                std::string command = "";

                mContainerLock.lock();
                std::shared_ptr<OCIContainerRequest> request = std::make_shared<OCIContainerRequest>(OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_RUN, containerId, dobbySpec, command, westerosSocket);
                mContainerRequest.push_back(request);
                mContainerLock.unlock();
                mContainerQueueCV.notify_one();

                do
                {
                    ret = sem_wait(&request->mSemaphore);
                } while (ret == -1 && errno == EINTR);

                if (ret == -1)
                {
                    LOGERR("OCIContainerRequest: sem_wait failed for RUN: %s", strerror(errno));
                    request->mResult = Core::ERROR_GENERAL;
                }
                else
                {
                    bool success = request->mSuccess;

                    if (success == false)
                    {
                        LOGINFO("descriptor: %d errorReason: %s", request->mDescriptor, request->mErrorReason.c_str());
                    }
                    status = request->mResult;

                    if (request->mResult == Core::ERROR_NONE)
                    {
                        LOGINFO("Update Info for %s",appInstanceId.c_str());
                        runtimeAppInfo.appInstanceId = std::move(appInstanceId);
                        runtimeAppInfo.appPath = std::move(appPath);
                        runtimeAppInfo.runtimePath = std::move(runtimePath);
                        runtimeAppInfo.descriptor = std::move(request->mDescriptor);
                        runtimeAppInfo.containerState = Exchange::IRuntimeManager::RUNTIME_STATE_RUNNING;

                        /* Insert/update runtime app info */
                        Core::SafeSyncType<Core::CriticalSection> lock(mRuntimeManagerImplLock);
                        mRuntimeAppInfo[runtimeAppInfo.appInstanceId] = std::move(runtimeAppInfo);
                    }
                }
            }

            return status;
        }

        Core::hresult RuntimeManagerImplementation::Hibernate(const string& appInstanceId)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            int ret = -1;

            if (appInstanceId.empty())
            {
                LOGERR("appInstanceId is empty");
            }
            else
            {
                string containerId = "com.sky.as.apps" + appInstanceId;

                LOGINFO("Entered Hibernate Implementation");
                mContainerLock.lock();
                std::shared_ptr<OCIContainerRequest> request = std::make_shared<OCIContainerRequest>(OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_HIBERNATE, containerId);
                mContainerRequest.push_back(request);
                mContainerLock.unlock();
                mContainerQueueCV.notify_one();

                do
                {
                    ret = sem_wait(&request->mSemaphore);
                } while (ret == -1 && errno == EINTR);

                if (ret == -1)
                {
                    LOGERR("OCIContainerRequest: sem_wait failed for HIBERNATE: %s", strerror(errno));
                    request->mResult = Core::ERROR_GENERAL;
                }
                else
                {
                    if (false == request->mSuccess)
                    {
                        LOGERR(" status: %d errorReason: %s",request->mResult, request->mErrorReason.c_str());
                    }
                    status = request->mResult;
                    if(request->mResult == Core::ERROR_NONE)
                    {
                        Core::SafeSyncType<Core::CriticalSection> lock(mRuntimeManagerImplLock);
                        if(mRuntimeAppInfo.find(appInstanceId) != mRuntimeAppInfo.end())
                        {
                            mRuntimeAppInfo[appInstanceId].containerState = Exchange::IRuntimeManager::RUNTIME_STATE_HIBERNATED;
                            LOGINFO("RuntimeAppInfo state for appInstanceId[%s] updated", mRuntimeAppInfo[appInstanceId].appInstanceId.c_str());
                        }
                        else
                        {
                            LOGERR("Missing appInstanceId[%s] in RuntimeAppInfo", appInstanceId.c_str());
                        }
                    }
                }
            }
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

        Core::hresult RuntimeManagerImplementation::GetInfo(const string& appInstanceId, string& info)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            int ret = -1;
            if (appInstanceId.empty())
            {
                LOGERR("appInstanceId is empty");
            }
            else
            {
                string containerId = "com.sky.as.apps" + appInstanceId;

                LOGINFO("Entered GetInfo Implementation");

                mContainerLock.lock();
                std::shared_ptr<OCIContainerRequest> request = std::make_shared<OCIContainerRequest>(OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_GETINFO, containerId);
                mContainerRequest.push_back(request);
                mContainerLock.unlock();
                mContainerQueueCV.notify_one();

                do
                {
                    ret = sem_wait(&request->mSemaphore);
                } while (ret == -1 && errno == EINTR);

                if (ret == -1)
                {
                    LOGERR("OCIContainerRequest: sem_wait failed for GETINFO: %s", strerror(errno));
                    request->mResult = Core::ERROR_GENERAL;
                }
                else
                {
                    if (false == request->mSuccess)
                    {
                        LOGERR(" status: %d errorReason: %s",request->mResult, request->mErrorReason.c_str());
                    }
                    else
                    {
                        info = request->mGetInfo;
                        LOGINFO(" status: %d",request->mResult);
                    }
                    status = request->mResult;
                }
            }
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
