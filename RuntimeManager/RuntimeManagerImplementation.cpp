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
#include "DobbySpecGenerator.h"
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
        ,mStorageManagerObject(nullptr)
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

            if (nullptr != mStorageManagerObject)
            {
                releaseStorageManagerPluginObject();
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

        void RuntimeManagerImplementation::updateContainerInfo(std::shared_ptr<OCIContainerRequest>& request)
        {
            Core::SafeSyncType<Core::CriticalSection> lock(mRuntimeManagerImplLock);
            if (mRuntimeAppInfo.find(request->mAppInstanceId) != mRuntimeAppInfo.end())
            {
                printContainerInfo();
                LOGINFO("RuntimeAppInfo appInstanceId[%s] updated", request->mAppInstanceId.c_str());
                OCIRequestType type = request->mRequestType;
                switch (type)
                {
                    case OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_TERMINATE:
                    case OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_KILL:
                        mRuntimeAppInfo[request->mAppInstanceId].containerState = Exchange::IRuntimeManager::RUNTIME_STATE_TERMINATING;
                        break;
                    case OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_HIBERNATE:
                        mRuntimeAppInfo[request->mAppInstanceId].containerState = Exchange::IRuntimeManager::RUNTIME_STATE_HIBERNATING;
                        break;
                    case OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_GETINFO:
                        mRuntimeAppInfo[request->mAppInstanceId].getInfo = request->mResponseData.getInfo;
                        break;
                    case OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_RUN:
                        mRuntimeAppInfo[request->mAppInstanceId].descriptor = request->mResponseData.descriptor;
                        break;
                    case OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_WAKE:
                        mRuntimeAppInfo[request->mAppInstanceId].containerState = Exchange::IRuntimeManager::RUNTIME_STATE_WAKING;
                        break;
                    default:
                        break;
                }

                printContainerInfo();
            }
            else
            {
                LOGWARN("Missing appInstanceId[%s] in RuntimeAppInfo", request->mAppInstanceId.c_str());
            }
        }

        Core::hresult RuntimeManagerImplementation::handleContainerRequest(OCIContainerRequest& request)
        {
            Core::hresult status = Core::ERROR_GENERAL;

            if (!request.mAppInstanceId.empty())
            {
                string containerId = std::string(RUNTIME_APP_PORTAL) + (request.mAppInstanceId);
                int ret = -1;

                std::shared_ptr<OCIContainerRequest> requestData(&request, [](OCIContainerRequest*) {
                });

                mContainerLock.lock();
                requestData->mContainerId = std::move(containerId);
                mContainerRequest.push_back(requestData);
                mContainerLock.unlock();
                mContainerQueueCV.notify_one();

                do
                {
                    ret = sem_wait(&requestData->mSemaphore);
                } while (ret == -1 && errno == EINTR);

                if (ret == -1)
                {
                    LOGERR("OCIContainerRequest: sem_wait failed for Kill: %s", strerror(errno));
                }
                else
                {
                    if ((requestData->mSuccess == false) || (requestData->mResult != Core::ERROR_NONE))
                    {
                        LOGERR("OCIRequestType: %d  status: %d errorReason: %s",
                                        static_cast<int>(requestData->mRequestType),  requestData->mSuccess, requestData->mErrorReason.c_str());
                    }
                    else
                    {
                        status = requestData->mResult;
                        updateContainerInfo(requestData);
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

        WPEFramework::Plugin::RuntimeManagerImplementation::OCIContainerRequest::OCIContainerRequest()
            : mResult(Core::ERROR_GENERAL),
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
                                request->mResult = ociContainerObject->StartContainerFromDobbySpec( \
                                                                                        request->mContainerId,
                                                                                        request->mDobbySpec,
                                                                                        request->mCommand,
                                                                                        request->mWesterosSocket,
                                                                                        request->mResponseData.descriptor,
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
                                                                                        request->mResponseData.getInfo,
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

                /* Create Storage Manager Plugin Object */
                if (Core::ERROR_NONE != createStorageManagerPluginObject())
                {
                    LOGERR("Failed to create Storage Manager Object");
                }

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

        Core::hresult RuntimeManagerImplementation::createStorageManagerPluginObject()
        {
            #define MAX_STORAGE_MANAGER_OBJECT_CREATION_RETRIES 2

            Core::hresult status = Core::ERROR_GENERAL;
            uint8_t retryCount = 0;

            if (nullptr == mCurrentservice)
            {
                LOGERR("mCurrentservice is null");
            }
            else
            {
                do
                {
                    mStorageManagerObject = mCurrentservice->QueryInterfaceByCallsign<WPEFramework::Exchange::IStorageManager>("org.rdk.StorageManager");

                    if (nullptr == mStorageManagerObject)
                    {
                        LOGERR("storageManagerObject is null (Attempt %d)", retryCount + 1);
                        retryCount++;
                        std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    }
                    else
                    {
                        LOGINFO("Successfully created Storage Manager Object");
                        status = Core::ERROR_NONE;
                        break;
                    }
                } while (retryCount < MAX_STORAGE_MANAGER_OBJECT_CREATION_RETRIES);

                if (status != Core::ERROR_NONE)
                {
                    LOGERR("Failed to create Storage Manager Object after %d attempts", MAX_STORAGE_MANAGER_OBJECT_CREATION_RETRIES);
                }
            }
            return status;
        }

        void RuntimeManagerImplementation::releaseStorageManagerPluginObject()
        {
            ASSERT(nullptr != mStorageManagerObject);
            if(mStorageManagerObject)
            {
                LOGINFO("Storage Manager object released\n");
                mStorageManagerObject->Release();
                mStorageManagerObject = nullptr;
            }
        }

/*
* @brief : Returns the storage information for a given app id using Storage Manager plugin interface
*/
        Core::hresult RuntimeManagerImplementation::getAppStorageInfo(const string& appId, AppStorageInfo& appStorageInfo)
        {
            Core::hresult status = Core::ERROR_GENERAL;

            if (appId.empty())
            {
                LOGERR("Invalid appId");
            }
            else
            {
                /* Re-attempting to create Storage Manager Object if the previous attempt failed (i.e., object is null) */
                if (nullptr == mStorageManagerObject)
                {
                    if (Core::ERROR_NONE != createStorageManagerPluginObject())
                    {
                        LOGERR("Re-attempt failed to create Storage Manager Object");
                    }
                }

                if (nullptr != mStorageManagerObject)
                {
                    if (Core::ERROR_NONE == (status = mStorageManagerObject->GetStorage(appId, appStorageInfo.userId, appStorageInfo.groupId,
                        appStorageInfo.path, appStorageInfo.size, appStorageInfo.used)))
                    {
                        LOGINFO("Received Storage Manager info for %s [path %s, userId %d, groupId %d, size %d, used %d]",
                            appId.c_str(), appStorageInfo.path.c_str(), appStorageInfo.userId,
                            appStorageInfo.groupId, appStorageInfo.size, appStorageInfo.used);
                    }
                    else
                    {
                        LOGERR("Failed to get Storage Manager info");
                    }
                }
            }

            return status;
        }

        bool RuntimeManagerImplementation::generate(const ApplicationConfiguration& config, RuntimeConfig& runtimeConfig, std::string& dobbySpec)
        {
            DobbySpecGenerator generator;
            generator.generate(config, runtimeConfig, dobbySpec);

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

        Core::hresult RuntimeManagerImplementation::Run(const string& appId, const string& appInstanceId, const string& appPath, const string& runtimePath, IStringIterator* const& envVars, const uint32_t userId, const uint32_t groupId, IValueIterator* const& ports, IStringIterator* const& paths, IStringIterator* const& debugSettings)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            RuntimeAppInfo runtimeAppInfo;
            std::string xdgRuntimeDir = "";
            std::string waylandDisplay = "";
            std::string dobbySpec;
            AppStorageInfo appStorageInfo;

            //PENDING - This needs to be populated via argument
            RuntimeConfig runtimeConfig;

            ApplicationConfiguration config;
            config.mAppId = appId;
            config.mAppInstanceId = appInstanceId;

            uint32_t uid, gid;
            generateUserId(uid, gid); 
            config.mUserId = uid;
            config.mGroupId = gid;

            if (envVars)
            {
                std::string envVar;
                while (envVars->Next(envVar))
                {
                    if (envVar.find("XDG_RUNTIME_DIR=") == 0)
                    {
                        xdgRuntimeDir = envVar.substr(strlen("XDG_RUNTIME_DIR="));
                    }
                    else if (envVar.find("WAYLAND_DISPLAY=") == 0)
                    {
                        waylandDisplay = envVar.substr(strlen("WAYLAND_DISPLAY="));
                    }
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

            // if (paths)
            // {
            //     std::string path;
            //     while (paths->Next(path))
            //     {
            //         config.mPaths.push_back(path);
            //     }
            // }

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

            if (!xdgRuntimeDir.empty() && !waylandDisplay.empty())
            {
                std::string westerosSocket = xdgRuntimeDir + "/" + waylandDisplay;
                config.mWesterosSocketPath = westerosSocket;
            }

            if (!appId.empty())
            {
                appStorageInfo.userId = userId;
                appStorageInfo.groupId = groupId;
                if (Core::ERROR_NONE == getAppStorageInfo(appId, appStorageInfo))
                {
                    config.mAppStorageInfo.path = std::move(appStorageInfo.path);
                    config.mAppStorageInfo.userId = userId;
                    config.mAppStorageInfo.groupId = groupId;
                    config.mAppStorageInfo.size = std::move(appStorageInfo.size);
                    config.mAppStorageInfo.used = std::move(appStorageInfo.used);
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
            else if (false == RuntimeManagerImplementation::generate(config, runtimeConfig, dobbySpec))
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

                OCIContainerRequest request;
                request.mAppInstanceId = std::move(appInstanceId);
                request.mRequestType = OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_RUN;
                request.mDobbySpec = std::move(dobbySpec);
                request.mCommand = std::move(command);
                request.mWesterosSocket = std::move(westerosSocket);


                status = handleContainerRequest(request);

                if (status == Core::ERROR_NONE)
                {
                    LOGINFO("Update Info for %s",appInstanceId.c_str());
                    if (!appId.empty())
                    {
                        runtimeAppInfo.appId = std::move(appId);
                    }
                    runtimeAppInfo.appInstanceId = std::move(appInstanceId);
                    runtimeAppInfo.appPath = std::move(appPath);
                    runtimeAppInfo.runtimePath = std::move(runtimePath);
                    runtimeAppInfo.descriptor = std::move(request.mResponseData.descriptor);
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
            OCIContainerRequest request;
            request.mAppInstanceId = std::move(appInstanceId);
            request.mRequestType = OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_HIBERNATE;
            status = handleContainerRequest(request);

            return status;
        }

        Core::hresult RuntimeManagerImplementation::Wake(const string& appInstanceId, const RuntimeState runtimeState)
        {
            Core::hresult status = Core::ERROR_GENERAL;

            LOGINFO("Entered Wake Implementation");

            OCIContainerRequest request;
            request.mAppInstanceId = std::move(appInstanceId);
            request.mRequestType = OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_WAKE;

            RuntimeState currentRuntimeState = getRuntimeState(appInstanceId);
            if (Exchange::IRuntimeManager::RUNTIME_STATE_HIBERNATING == currentRuntimeState ||
                Exchange::IRuntimeManager::RUNTIME_STATE_HIBERNATED == currentRuntimeState)
            {
                status = handleContainerRequest(request);
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
            LOGINFO("Entered Terminate Implementation");

            OCIContainerRequest request;
            request.mAppInstanceId = std::move(appInstanceId);
            request.mRequestType = OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_TERMINATE;

            status = handleContainerRequest(request);
            return status;
        }

        Core::hresult RuntimeManagerImplementation::Kill(const string& appInstanceId)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            LOGINFO("Entered Kill Implementation");

            OCIContainerRequest request;
            request.mAppInstanceId = std::move(appInstanceId);
            request.mRequestType = OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_KILL;

            status = handleContainerRequest(request);

            return status;
        }

        Core::hresult RuntimeManagerImplementation::GetInfo(const string& appInstanceId, string& info)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            LOGINFO("Entered GetInfo Implementation");

            OCIContainerRequest request;
            request.mAppInstanceId = std::move(appInstanceId);
            request.mRequestType = OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_GETINFO;

            status = handleContainerRequest(request);

            if(status == Core::ERROR_NONE)
            {
                info = std::move(mRuntimeAppInfo[appInstanceId].getInfo);
            }

            return status;
        }

        Core::hresult RuntimeManagerImplementation::Annotate(const string& appInstanceId, const string& key, const string& value)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            OCIContainerRequest request;

            LOGINFO("Entered Annotate Implementation");

            if (key.empty())
            {
                LOGERR("Annotate: key is empty");
            }
            else
            {
                request.mAppInstanceId = std::move(appInstanceId);
                request.mRequestType = OCIRequestType::RUNTIME_OCI_REQUEST_METHOD_ANNONATE;
                request.mAnnotateKey = std::move(key);
                request.mAnnotateKeyValue = std::move(value);
                status = handleContainerRequest(request);
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

        void RuntimeManagerImplementation::generateUserId(uint32_t& userId, uint32_t& groupId)
        {
            //TODO Generate userid and groupid in random way
            userId = 30490;
            FILE* fp = fopen("/tmp/appuid", "r");
            if (fp != NULL)
            {
                char* line = NULL;
        	size_t len = 0;
        	while ((getline(&line, &len, fp)) != -1)
                {
        	    userId = atoi(line);
        	    break;
        	}
        	fclose(fp);
            }
            groupId = 30000;
        }
    } /* namespace Plugin */
} /* namespace WPEFramework */
