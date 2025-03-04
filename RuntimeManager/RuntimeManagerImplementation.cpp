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
//TODO - Remove the hardcoding to enable compatibility with a common middleware. The app portal name should be configurable in some way
#define RUNTIME_APP_PORTAL "com.sky.as.apps"

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

        void RuntimeManagerImplementation::updateContainerInfo(OCIRequestType type, const std::string& appInstanceId, const OCIContainerRequest& request, ContainerRequestData& containerReqData)
        {
            Core::SafeSyncType<Core::CriticalSection> lock(mRuntimeManagerImplLock);
            if (mRuntimeAppInfo.find(appInstanceId) != mRuntimeAppInfo.end())
            {
                printContainerInfo();
                LOGINFO("RuntimeAppInfo appInstanceId[%s] updated", appInstanceId.c_str());
                switch (type)
                {
                    case OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_TERMINATE:
                    case OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_KILL:
                        mRuntimeAppInfo[appInstanceId].containerState = Exchange::IRuntimeManager::RUNTIME_STATE_TERMINATING;
                        break;
                    case OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_HIBERNATE:
                        mRuntimeAppInfo[appInstanceId].containerState = Exchange::IRuntimeManager::RUNTIME_STATE_HIBERNATING;
                        break;
                    case OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_GETINFO:
                        containerReqData.getInfo = request.mGetInfo;
                        break;
                    case OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_RUN:
                        containerReqData.descriptor = request.mDescriptor;
                        break;
                    case OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_WAKE:
                        mRuntimeAppInfo[appInstanceId].containerState = Exchange::IRuntimeManager::RUNTIME_STATE_WAKING;
                        break;
                    default:
                        break;
                }

                printContainerInfo();
            }
            else
            {
                LOGWARN("Missing appInstanceId[%s] in RuntimeAppInfo", appInstanceId.c_str());
            }
        }

        Core::hresult RuntimeManagerImplementation::handleContainerRequest(const std::string& appInstanceId, OCIRequestType type, ContainerRequestData& containerReqData)
        {
            Core::hresult status = Core::ERROR_GENERAL;

            if (!appInstanceId.empty())
            {
                string containerId = std::string(RUNTIME_APP_PORTAL) + appInstanceId;
                int ret = -1;

                mContainerLock.lock();
                std::shared_ptr<OCIContainerRequest> request = std::make_shared<OCIContainerRequest>(type, containerId, containerReqData);
                mContainerRequest.push_back(request);
                mContainerLock.unlock();
                mContainerQueueCV.notify_one();

                do
                {
                    ret = sem_wait(&request->mSemaphore);
                } while (ret == -1 && errno == EINTR);

                if (ret == -1)
                {
                    LOGERR("OCIContainerRequest: sem_wait failed for Kill: %s", strerror(errno));
                }
                else
                {
                    if ((request->mSuccess == false) || (request->mResult != Core::ERROR_NONE))
                    {
                        LOGERR("OCIRequestType: %d descriptor: %d status: %d errorReason: %s", 
                                        static_cast<int>(type), request->mDescriptor, request->mSuccess, request->mErrorReason.c_str());
                    }
                    else
                    {
                        status = request->mResult;
                        updateContainerInfo(type, appInstanceId, *request, containerReqData);
                    }
                }
            }
            else
            {
                LOGERR("appInstanceId param is missing");
            }

            LOGINFO("handleContainerRequest done with status: %d", status);
            return status;
        }

        void RuntimeManagerImplementation::printContainerInfo()
        {
            for (const auto& pair : mRuntimeAppInfo) {
               LOGINFO("RuntimeAppInfo -> appInstanceId[%s] : appPath[%s]\n", pair.first.c_str(), pair.second.appPath.c_str());
               LOGINFO("RuntimeAppInfo -> runtimePath[%s] : descriptor[%d] containerState[%d]\n", pair.second.runtimePath.c_str(), pair.second.descriptor, pair.second.containerState);
            }
        }

        WPEFramework::Plugin::RuntimeManagerImplementation::OCIContainerRequest::OCIContainerRequest(
            OCIRequestType type, const std::string& containerId, const ContainerRequestData& containerReqData)
            : mRequestType(type),
              mContainerId(containerId),
              mDobbySpec(containerReqData.dobbySpec),
              mCommand(containerReqData.command),
              mWesterosSocket(containerReqData.westerosSocket),
              mGetInfo(""),
              mResult(Core::ERROR_GENERAL),
              mDescriptor(0),
              mSuccess(false),
              mErrorReason(""),
              mAnnotateKey(containerReqData.key),
              mAnnotateKeyValue(containerReqData.value)
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
                            break;

                            case OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_HIBERNATE:
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
                            break;

                            case OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_WAKE:
                            {
                                //Question: How should we pass the requested run state to the container?
                                //There is no argument in the ociContainer interface to pass the input state.
                                request->mResult = ociContainerObject->WakeupContainer(request->mContainerId,
                                                                                       request->mSuccess,
                                                                                       request->mErrorReason);

                                if (Core::ERROR_NONE != request->mResult)
                                {
                                    LOGERR("Failed to WakeupContainer");
                                    request->mErrorReason = "Failed to WakeupContainer";
                                }
                            }
                            break;

                            case OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_SUSPEND:
                            case OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_RESUME:
                            {
                                LOGWARN("Unknown Method type %d", static_cast<int>(request->mRequestType));
                                request->mResult = Core::ERROR_GENERAL;
                                request->mErrorReason = "Unknown Method type";
                            }
                            break;

                            case OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_TERMINATE:
                            {
                                request->mResult = ociContainerObject->StopContainer(request->mContainerId,
                                                                                    false,
                                                                                    request->mSuccess,
                                                                                    request->mErrorReason);
                                if (Core::ERROR_NONE != request->mResult)
                                {
                                    LOGERR("Failed to StopContainer to terminate");
                                    request->mErrorReason = "Failed to StopContainer";
                                }
                            }
                            break;

                            case OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_KILL:
                            {
                                request->mResult = ociContainerObject->StopContainer( request->mContainerId,
                                                                                      true,
                                                                                      request->mSuccess,
                                                                                      request->mErrorReason);
                                if (Core::ERROR_NONE != request->mResult)
                                {
                                    LOGERR("Failed to StopContainer");
                                    request->mErrorReason = "Failed to StopContainer";
                                }
                            }
                            break;

                            case OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_GETINFO:
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
                            break;

                            case OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_ANNONATE:
                            {
                                request->mResult = ociContainerObject->Annotate( request->mContainerId,
                                                                                 request->mAnnotateKey,
                                                                                 request->mAnnotateKeyValue,
                                                                                 request->mSuccess,
                                                                                 request->mErrorReason);
                                if (Core::ERROR_NONE != request->mResult)
                                {
                                    LOGERR("Failed to Annotate property key: %s value: %s", request->mAnnotateKey.c_str(), request->mAnnotateKeyValue.c_str());
                                    request->mErrorReason = "Failed to Annotate property key";
                                }
                            }
                            break;

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

        Exchange::IRuntimeManager::RuntimeState RuntimeManagerImplementation::getRuntimeState(const string& appInstanceId)
        {
            Exchange::IRuntimeManager::RuntimeState runtimeState = Exchange::IRuntimeManager::RUNTIME_STATE_UNKNOWN;

            Core::SafeSyncType<Core::CriticalSection> lock(mRuntimeManagerImplLock);

            if (!appInstanceId.empty())
            {
                if(mRuntimeAppInfo.find(appInstanceId) == mRuntimeAppInfo.end())
                {
                   LOGERR("Missing appInstanceId[%s] in RuntimeAppInfo", appInstanceId.c_str());
                }
                else
                {
                   runtimeState = mRuntimeAppInfo[appInstanceId].containerState;
                }
            }
            else
            {
                LOGERR("appInstanceId param is missing");
            }

            return runtimeState;
        }

        Core::hresult RuntimeManagerImplementation::Run(const string& appInstanceId, const string& appPath, const string& runtimePath, IStringIterator* const& envVars, const uint32_t userId, const uint32_t groupId, IValueIterator* const& ports, IStringIterator* const& paths, IStringIterator* const& debugSettings)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            RuntimeAppInfo runtimeAppInfo;
            std::string xdgRuntimeDir = "";
            std::string waylandDisplay = "";
            std::string dobbySpec;

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
                ContainerRequestData containerReqData;

                containerReqData.dobbySpec = std::move(dobbySpec);
                containerReqData.command = std::move(command);
                containerReqData.westerosSocket = std::move(westerosSocket);

                status = handleContainerRequest(appInstanceId, OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_RUN, containerReqData);

                if (status == Core::ERROR_NONE)
                {
                    LOGINFO("Update Info for %s",appInstanceId.c_str());
                    runtimeAppInfo.appInstanceId = std::move(appInstanceId);
                    runtimeAppInfo.appPath = std::move(appPath);
                    runtimeAppInfo.runtimePath = std::move(runtimePath);
                    runtimeAppInfo.descriptor = std::move(containerReqData.descriptor);
                    runtimeAppInfo.containerState = Exchange::IRuntimeManager::RUNTIME_STATE_STARTING;

                    /* Insert/update runtime app info */
                    Core::SafeSyncType<Core::CriticalSection> lock(mRuntimeManagerImplLock);
                    mRuntimeAppInfo[runtimeAppInfo.appInstanceId] = std::move(runtimeAppInfo);
                }
            }

            return status;
        }

        Core::hresult RuntimeManagerImplementation::Hibernate(const string& appInstanceId)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            ContainerRequestData containerReqData;

            status = handleContainerRequest(appInstanceId, OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_HIBERNATE, containerReqData);

            return status;
        }

        Core::hresult RuntimeManagerImplementation::Wake(const string& appInstanceId, const RuntimeState runtimeState)
        {
            Core::hresult status = Core::ERROR_GENERAL;

            LOGINFO("Entered Wake Implementation");
            ContainerRequestData containerReqData;
            RuntimeState currentRuntimeState = getRuntimeState(appInstanceId);
            if (Exchange::IRuntimeManager::RUNTIME_STATE_HIBERNATING == currentRuntimeState ||
                Exchange::IRuntimeManager::RUNTIME_STATE_HIBERNATED == currentRuntimeState)
            {
                status = handleContainerRequest(appInstanceId, OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_WAKE, containerReqData);
            }
            else
            {
                LOGERR("Container is Not in Hibernating/Hiberanted state");
            }

            return status;
        }

        Core::hresult RuntimeManagerImplementation::Suspend(const string& appInstanceId)
        {
            Core::hresult status = Core::ERROR_NONE;

            LOGINFO("Suspend Implementation - Stub!");

            return status;
        }

        Core::hresult RuntimeManagerImplementation::Resume(const string& appInstanceId)
        {
            Core::hresult status = Core::ERROR_NONE;

            LOGINFO("Resume Implementation - Stub!");

            return status;
        }

        Core::hresult RuntimeManagerImplementation::Terminate(const string& appInstanceId)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            ContainerRequestData containerReqData;
            LOGINFO("Entered Terminate Implementation");

            status = handleContainerRequest(appInstanceId, OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_TERMINATE, containerReqData);
            return status;
        }

        Core::hresult RuntimeManagerImplementation::Kill(const string& appInstanceId)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            ContainerRequestData containerReqData;

            LOGINFO("Entered Kill Implementation");

            status = handleContainerRequest(appInstanceId, OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_KILL, containerReqData);

            return status;
        }

        Core::hresult RuntimeManagerImplementation::GetInfo(const string& appInstanceId, string& info)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            ContainerRequestData containerReqData;

            LOGINFO("Entered GetInfo Implementation");

            status = handleContainerRequest(appInstanceId, OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_GETINFO, containerReqData);

            if(status == Core::ERROR_NONE)
            {
                info = containerReqData.getInfo;
            }

            return status;
        }

        Core::hresult RuntimeManagerImplementation::Annotate(const string& appInstanceId, const string& key, const string& value)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            ContainerRequestData containerReqData;
            LOGINFO("Entered Annotate Implementation");

            if (key.empty())
            {
                LOGERR("Annotate: key is empty");
            }
            else
            {
                containerReqData.key = std::move(key);
                containerReqData.value = std::move(value);

                status = handleContainerRequest(appInstanceId, OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_ANNONATE, containerReqData);
            }
            return status;
        }

        Core::hresult RuntimeManagerImplementation::Mount()
        {
            Core::hresult status = Core::ERROR_NONE;

            LOGINFO("Mount Implementation - Stub!");

            return status;
        }

        Core::hresult RuntimeManagerImplementation::Unmount()
        {
            Core::hresult status = Core::ERROR_NONE;

            LOGINFO("Unmount Implementation - Stub!");

            return status;
        }
    } /* namespace Plugin */
} /* namespace WPEFramework */
