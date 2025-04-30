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

#include "Module.h"
#include <interfaces/IRuntimeManager.h>
#include <interfaces/IConfiguration.h>
#include "UtilsLogging.h"
#include "tracing/Logging.h"
#include <mutex>
#include <interfaces/IOCIContainer.h>
#include <interfaces/IStorageManager.h>
#include <condition_variable>
#include "ApplicationConfiguration.h"

namespace WPEFramework
{
    namespace Plugin
    {
        class RuntimeManagerImplementation : public Exchange::IRuntimeManager, public Exchange::IConfiguration
        {
            public:
                enum RuntimeEventType
                {
                    RUNTIME_MANAGER_EVENT_UNKNOWN = 0,
                    RUNTIME_MANAGER_EVENT_STATECHANGED
                };
                enum class OCIRequestType
                {
                    RUNTIME_OCI_REQUEST_METHOD_UNKNOWN = 0,
                    RUNTIME_OCI_REQUEST_METHOD_RUN,
                    RUNTIME_OCI_REQUEST_METHOD_HIBERNATE,
                    RUNTIME_OCI_REQUEST_METHOD_WAKE,
                    RUNTIME_OCI_REQUEST_METHOD_SUSPEND,
                    RUNTIME_OCI_REQUEST_METHOD_RESUME,
                    RUNTIME_OCI_REQUEST_METHOD_TERMINATE,
                    RUNTIME_OCI_REQUEST_METHOD_KILL,
                    RUNTIME_OCI_REQUEST_METHOD_GETINFO,
                    RUNTIME_OCI_REQUEST_METHOD_ANNONATE,
                    RUNTIME_OCI_REQUEST_METHOD_MOUNT,
                    RUNTIME_OCI_REQUEST_METHOD_UNMOUNT
                };

                typedef struct _ResponseData
                {
                    std::string getInfo = "";
                    int32_t descriptor = -1;
                } ResponseData;

                struct OCIContainerRequest
                {
                    OCIContainerRequest();
                    ~OCIContainerRequest();

                    OCIRequestType mRequestType;
                    std::string mContainerId;
                    std::string mAppInstanceId;
                    std::string mDobbySpec;
                    std::string mCommand;
                    std::string mWesterosSocket;
                    sem_t mSemaphore;
                    Core::hresult mResult;
                    bool mSuccess;
                    std::string mErrorReason;
                    std::string mRequestId;
                    std::string mAnnotateKey;
                    std::string mAnnotateKeyValue;
                    ResponseData mResponseData;
                };

                typedef struct _RuntimeAppInfo
                {
                    std::string appId;
                    std::string appInstanceId;
                    std::string appPath;
                    std::string runtimePath;
                    uint32_t descriptor;
                    std::string getInfo;
                    Exchange::IRuntimeManager::RuntimeState containerState;
                } RuntimeAppInfo;

                class EXTERNAL Job : public Core::IDispatch
                {
                    protected:
                    Job(RuntimeManagerImplementation *mRuntimeManagerImpl, RuntimeEventType event, JsonValue &params)
                    : mRuntimeManagerImplementation(mRuntimeManagerImpl)
                    , _event(event)
                    , _params(params)
                    {
                        if (mRuntimeManagerImplementation != nullptr)
                        {
                            mRuntimeManagerImplementation->AddRef();
                        }
                    }

                    public:
                        Job() = delete;
                        Job(const Job&) = delete;
                        Job& operator=(const Job&) = delete;
                        ~Job()
                        {
                            if (mRuntimeManagerImplementation != nullptr)
                            {
                                mRuntimeManagerImplementation->Release();
                            }
                        }

                    public:
                        static Core::ProxyType<Core::IDispatch> Create(RuntimeManagerImplementation *mRuntimeManagerImpl, RuntimeEventType event, JsonValue params)
                        {
#ifndef  USE_THUNDER_R4
                            return (Core::proxy_cast<Core::IDispatch>(Core::ProxyType<Job>::Create(mRuntimeManagerImpl, event, params)));
#else
                            return (Core::ProxyType<Core::IDispatch>(Core::ProxyType<Job>::Create(mRuntimeManagerImpl, event, params)));
#endif
                        }

                        virtual void Dispatch()
                        {
                            mRuntimeManagerImplementation->Dispatch(_event, _params);
                        }

                    private:
                        RuntimeManagerImplementation *mRuntimeManagerImplementation;
                        const RuntimeEventType _event;
                        const JsonValue _params;
                };

                RuntimeManagerImplementation ();
                ~RuntimeManagerImplementation () override;

                RuntimeManagerImplementation (const RuntimeManagerImplementation &) = delete;
                RuntimeManagerImplementation & operator=(const RuntimeManagerImplementation &) = delete;

                static RuntimeManagerImplementation* getInstance();

                BEGIN_INTERFACE_MAP(RuntimeManagerImplementation)
                INTERFACE_ENTRY(Exchange::IRuntimeManager)
                INTERFACE_ENTRY(Exchange::IConfiguration)
                END_INTERFACE_MAP

                /* IRuntimeManager methods  */
                virtual Core::hresult Register(Exchange::IRuntimeManager::INotification *notification) override;
                virtual Core::hresult Unregister(Exchange::IRuntimeManager::INotification *notification) override;

                virtual Core::hresult Run(const string& appId, const string& appInstanceId, const string& appPath, const string& runtimePath, IStringIterator* const& envVars, const uint32_t userId, const uint32_t groupId, IValueIterator* const& ports, IStringIterator* const& paths, IStringIterator* const& debugSettings);
                virtual Core::hresult Hibernate(const string& appInstanceId) override;
                virtual Core::hresult Wake(const string& appInstanceId, const RuntimeState runtimeState) override;
                virtual Core::hresult Suspend(const string& appInstanceId) override;
                virtual Core::hresult Resume(const string& appInstanceId) override;
                virtual Core::hresult Terminate(const string& appInstanceId) override;
                virtual Core::hresult Kill(const string& appInstanceId) override;
                virtual Core::hresult GetInfo(const string& appInstanceId, string& info) override;
                virtual Core::hresult Annotate(const string& appInstanceId, const string& key, const string& value) override;
                virtual Core::hresult Mount() override;
                virtual Core::hresult Unmount() override;

                // IConfiguration methods
                uint32_t Configure(PluginHost::IShell* service) override;

            private: /* private methods */
                Core::hresult createOCIContainerPluginObject(Exchange::IOCIContainer*& ociContainerRemoteObject);
                void releaseOCIContainerPluginObject(Exchange::IOCIContainer*& ociContainerRemoteObject);
                Core::hresult createStorageManagerPluginObject();
                void releaseStorageManagerPluginObject();
                void setRunningState(bool state);
                bool getRunningState();
                static bool generate(const ApplicationConfiguration& config, RuntimeConfig& runtimeConfig, std::string& dobbySpec);
                Core::hresult handleContainerRequest(OCIContainerRequest& request);
                void updateContainerInfo(std::shared_ptr<OCIContainerRequest>&  requestData);
                void printContainerInfo();
                Exchange::IRuntimeManager::RuntimeState getRuntimeState(const string& appInstanceId);
                Core::hresult getAppStorageInfo(const string& appId, AppStorageInfo& appStorageInfo);

            private: /* members */
                mutable Core::CriticalSection mRuntimeManagerImplLock;
                std::mutex mContainerLock;
                PluginHost::IShell* mCurrentservice;
                std::list<Exchange::IRuntimeManager::INotification*> mRuntimeManagerNotification;
                std::thread mContainerWorkerThread;
                std::map<std::string, RuntimeAppInfo> mRuntimeAppInfo;
                std::vector<std::shared_ptr<WPEFramework::Plugin::RuntimeManagerImplementation::OCIContainerRequest>> mContainerRequest;
                std::condition_variable mContainerQueueCV;
                Exchange::IStorageManager *mStorageManagerObject;

            private: /* internal methods */
                void dispatchEvent(RuntimeEventType, const JsonValue &params);
                void Dispatch(RuntimeEventType event, const JsonValue params);
                void OCIContainerWorkerThread(void);
                void generateUserId(uint32_t& userId, uint32_t& groupId);

                friend class Job;

            public/*members*/:
                static RuntimeManagerImplementation* _instance;
        };
    } /* namespace Plugin */
} /* namespace WPEFramework */
