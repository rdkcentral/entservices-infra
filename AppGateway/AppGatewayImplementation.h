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

#pragma once

#include "Module.h"
#include "Resolver.h"
#include <interfaces/IAppGateway.h>
#include <interfaces/IConfiguration.h>
#include <interfaces/IAppNotifications.h>
#include "ContextUtils.h"
#include <com/com.h>
#include <core/core.h>
#include <map>


namespace WPEFramework {
namespace Plugin {
    using Context = Exchange::GatewayContext;
    class AppGatewayImplementation : public Exchange::IAppGatewayResolver, public Exchange::IConfiguration
    {

    public:
        AppGatewayImplementation();
        ~AppGatewayImplementation() override;

        // We do not allow this plugin to be copied !!
        AppGatewayImplementation(const AppGatewayImplementation&) = delete;
        AppGatewayImplementation& operator=(const AppGatewayImplementation&) = delete;

        BEGIN_INTERFACE_MAP(AppGatewayImplementation)
        INTERFACE_ENTRY(Exchange::IConfiguration)
        INTERFACE_ENTRY(Exchange::IAppGatewayResolver)
        END_INTERFACE_MAP

    public:
        Core::hresult Configure(Exchange::IAppGatewayResolver::IStringIterator *const &paths) override;
        Core::hresult Resolve(const Context& context, const string& origin ,const string& method, const string& params, string& result) override;

        // IConfiguration interface
        uint32_t Configure(PluginHost::IShell* service) override;

    private:

        class EXTERNAL RespondJob : public Core::IDispatch
        {
        protected:
            RespondJob(AppGatewayImplementation *parent, 
            const Context& context,
            const std::string& payload,
            const std::string& destination
            )
                : mParent(*parent), mPayload(payload), mContext(context), mDestination(destination)
            {
            }

        public:
            RespondJob() = delete;
        RespondJob(const RespondJob &) = delete;
            RespondJob &operator=(const RespondJob &) = delete;
            ~RespondJob()
            {
            }

        public:
            static Core::ProxyType<Core::IDispatch> Create(AppGatewayImplementation *parent,
                const Context& context, const std::string& payload, const std::string& origin)
            {
                return (Core::ProxyType<Core::IDispatch>(Core::ProxyType<RespondJob>::Create(parent, context, payload, origin)));
            }
            virtual void Dispatch()
            {
                if(ContextUtils::IsOriginGateway(mDestination)) {
                    mParent.ReturnMessageInSocket(mContext, std::move(mPayload));
                } else {
                    mParent.SendToLaunchDelegate(mContext, std::move(mPayload));
                }
                
            }

        private:
            AppGatewayImplementation &mParent;
            const std::string mPayload;
            const Context mContext;
            const std::string mDestination;
        };

        class AppIdRegistry{
        public:
            void Add(const uint32_t connectionId, string appId) {
                std::lock_guard<std::mutex> lock(mAppIdMutex);
                mAppIdMap[connectionId] = std::move(appId);
            }

            void Remove(const uint32_t connectionId) {
                std::lock_guard<std::mutex> lock(mAppIdMutex);
                mAppIdMap.erase(connectionId);
            }

            bool Get(const uint32_t connectionId, string& appId) {
                std::lock_guard<std::mutex> lock(mAppIdMutex);
                auto it = mAppIdMap.find(connectionId);
                if (it != mAppIdMap.end()) {
                    appId = it->second;
                    return true;
                }
                return false;
            }


        private:
            std::unordered_map<uint32_t,string> mAppIdMap;
            std::mutex mAppIdMutex;
        };

        Core::hresult HandleEvent(const Context &context, const string &alias, const string &event, const string &origin,  const bool listen);
                
        void ReturnMessageInSocket(const Context& context, const string payload ) {
            if (mAppGatewayResponder==nullptr) {
                mAppGatewayResponder = mService->QueryInterface<Exchange::IAppGatewayResponder>();
            }

            if (mAppGatewayResponder == nullptr) {
                LOGERR("AppGateway Responder not available");
                return;
            }
            if (Core::ERROR_NONE != mAppGatewayResponder->Respond(context, payload)) {
                LOGERR("Failed to Respond in Gateway");
            }
        }

        PluginHost::IShell* mService;
        ResolverPtr mResolverPtr;
        Exchange::IAppNotifications *mAppNotifications; // Shared pointer to AppNotifications
        Exchange::IAppGatewayResponder *mAppGatewayResponder;
        Exchange::IAppGatewayResponder *mInternalGatewayResponder; // Shared pointer to InternalGatewayResponder
        Exchange::IAppGatewayAuthenticator *mAuthenticator; // Shared pointer to Authenticator
        AppIdRegistry mAppIdRegistry;
        uint32_t InitializeResolver();
        uint32_t InitializeWebsocket();
        uint32_t ProcessComRpcRequest(const Context &context, const string& alias, const string& method, const string& params, string &resolution);
        uint32_t PreProcessEvent(const Context &context, const string& alias, const string &method, const string& origin, const string& params, string &resolution);
        string UpdateContext(const Context &context, const string& method, const string& params, const bool& onlyAdditionalContext = false);
        Core::hresult InternalResolve(const Context &context, const string &method, const string &params, const string &origin, string& resolution);
        Core::hresult FetchResolvedData(const Context &context, const string &method, const string &params, const string &origin, string& resolution);
        Core::hresult InternalResolutionConfigure(std::vector<std::string>&& configPaths);
        bool SetupAppGatewayAuthenticator();
        void SendToLaunchDelegate(const Context& context, const string& payload);
    };
} // namespace Plugin
} // namespace WPEFramework
