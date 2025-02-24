/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
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
*/

#pragma once

#include "Module.h"
#include <interfaces/Ids.h>
#include <interfaces/IAppManager.h>
#include <interfaces/IStore2.h>
#include <interfaces/IConfiguration.h>
#include "tracing/Logging.h"
#include <com/com.h>
#include <core/core.h>
#include <plugins/plugins.h>
#include "LifecycleInterfaceConnector.h"
#include <map>


namespace WPEFramework {
namespace Plugin {

    class AppManagerImplementation : public Exchange::IAppManager, public Exchange::IConfiguration {

    public:
        AppManagerImplementation();
        ~AppManagerImplementation() override;

        static AppManagerImplementation* getInstance();
        AppManagerImplementation(const AppManagerImplementation&) = delete;
        AppManagerImplementation& operator=(const AppManagerImplementation&) = delete;

        BEGIN_INTERFACE_MAP(AppManagerImplementation)
        INTERFACE_ENTRY(Exchange::IAppManager)
        INTERFACE_ENTRY(Exchange::IConfiguration)
        END_INTERFACE_MAP

    public:
        typedef enum _ApplicationType
        {
            APPLICATION_TYPE_UNKNOWN = 0,
            APPLICATION_TYPE_INTERACTIVE,
            APPLICATION_TYPE_SYSTEM
        } ApplicationType;

        typedef struct _LoadedAppInfo
        {
            string appInstanceId;
            string activeSessionId;
            string appIntent;
            ApplicationType type;
            Exchange::IAppManager::AppLifecycleState currentAppState;
            Exchange::ILifecycleManager::LifecycleState currentAppLifecycleState;
            Exchange::IAppManager::AppLifecycleState targetAppState;
        } LoadedAppInfo;

        std::map<std::string, LoadedAppInfo> mLoadedAppInfo;

        enum EventNames {
            APP_STATE_CHANGED
        };
        class EXTERNAL Job : public Core::IDispatch {
        protected:
             Job(AppManagerImplementation *appManagerImplementation, EventNames event, JsonObject &params)
                : mAppManagerImplementation(appManagerImplementation)
                , _event(event)
                , _params(params) {
                if (mAppManagerImplementation != nullptr) {
                    mAppManagerImplementation->AddRef();
                }
            }

       public:
            Job() = delete;
            Job(const Job&) = delete;
            Job& operator=(const Job&) = delete;
            ~Job() {
                if (mAppManagerImplementation != nullptr) {
                    mAppManagerImplementation->Release();
                }
            }

       public:
            static Core::ProxyType<Core::IDispatch> Create(AppManagerImplementation *appManagerImplementation, EventNames event, JsonObject params) {
#ifndef USE_THUNDER_R4
                return (Core::proxy_cast<Core::IDispatch>(Core::ProxyType<Job>::Create(appManagerImplementation, event, params)));
#else
                return (Core::ProxyType<Core::IDispatch>(Core::ProxyType<Job>::Create(appManagerImplementation, event, params)));
#endif
            }

            virtual void Dispatch() {
                mAppManagerImplementation->Dispatch(_event, _params);
            }
        private:
            AppManagerImplementation *mAppManagerImplementation;
            const EventNames _event;
            const JsonObject _params;
        };

    public:
        Core::hresult Register(Exchange::IAppManager::INotification *notification) override ;
        Core::hresult Unregister(Exchange::IAppManager::INotification *notification) override ;
        Core::hresult LaunchApp(const string& appId , const string& intent , const string& launchArgs) override;
        Core::hresult CloseApp(const string& appId) override;
        Core::hresult TerminateApp(const string& appId) override;
        Core::hresult KillApp(const string& appId) override;
        Core::hresult GetLoadedApps(string& appData) override;
        Core::hresult SendIntent(const string& appId , const string& intent) override;
        Core::hresult PreloadApp(const string& appId , const string& launchArgs ,string& error) override;
        Core::hresult GetAppProperty(const string& appId , const string& key , string& value) override;
        Core::hresult SetAppProperty(const string& appId , const string& key ,const string& value) override;
        Core::hresult GetInstalledApps(string& apps) override;
        Core::hresult IsInstalled(const string& appId, bool& installed) override;
        Core::hresult StartSystemApp(const string& appId) override;
        Core::hresult StopSystemApp(const string& appId) override;
        Core::hresult ClearAppData(const string& appId) override;
        Core::hresult ClearAllAppData() override;
        Core::hresult GetAppMetadata(const string& appId, const string& metaData, string& result) override;
        Core::hresult GetMaxRunningApps(int32_t& maxRunningApps) const override;
        Core::hresult GetMaxHibernatedApps(int32_t& maxHibernatedApps) const override;
        Core::hresult GetMaxHibernatedFlashUsage(int32_t& maxHibernatedFlashUsage) const override;
        Core::hresult GetMaxInactiveRamUsage(int32_t& maxInactiveRamUsage) const override;

        // IConfiguration methods
        uint32_t Configure(PluginHost::IShell* service) override;

    private: /* private methods */
        Core::hresult createPersistentStoreRemoteStoreObject();
        void releasePersistentStoreRemoteStoreObject();
    private:
        mutable Core::CriticalSection mAdminLock;
        std::list<Exchange::IAppManager::INotification*> mAppManagerNotification;
        LifecycleInterfaceConnector* mLifecycleInterfaceConnector;
        Exchange::IStore2* mPersistentStoreRemoteStoreObject;
        PluginHost::IShell* mCurrentservice;

        void dispatchEvent(EventNames, const JsonObject &params);
        void Dispatch(EventNames event, const JsonObject params);
        friend class Job;

    public/*members*/:
        static AppManagerImplementation* _instance;
    };
} /* namespace Plugin */
} /* namespace WPEFramework */
