/*
 * Copyright 2023 Comcast Cable Communications Management, LLC
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
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "Module.h"
#include <interfaces/IApp2AppProvider.h>
#include <interfaces/IConfiguration.h>
#include <mutex>
#include <map>
#include "UtilsLogging.h"
#include <interfaces/IAppGateway.h>
#include "ContextUtils.h"

namespace WPEFramework {
namespace Plugin {
    class App2AppProviderImplementation : public Exchange::IApp2AppProvider, public Exchange::IConfiguration {
    private:

        App2AppProviderImplementation(const App2AppProviderImplementation&) = delete;
        App2AppProviderImplementation& operator=(const App2AppProviderImplementation&) = delete;

    public:
        App2AppProviderImplementation();
        ~App2AppProviderImplementation();

        BEGIN_INTERFACE_MAP(App2AppProviderImplementation)
        INTERFACE_ENTRY(Exchange::IApp2AppProvider)
        INTERFACE_ENTRY(Exchange::IConfiguration)
        END_INTERFACE_MAP

        Core::hresult RegisterProvider(const Context &context /* @in */,
        const bool& provide /* @in */,
        const string &capability /* @in */
        ) override;

        Core::hresult InvokeProvider(const Context &context /* @in */,
        const string &capability /* @in */,
        const string &params /* @in @opaque*/
        ) override;

         Core::hresult HandleProviderResponse(const string &payload /* @in @opaque */,
        const string &capability /* @in */
        ) override;

        Core::hresult HandleProviderError(const string &payload /* @in @opaque */,
        const string &capability /* @in */
        ) override;

        Core::hresult Cleanup(const uint32_t connectionId /* @in */, const string &origin /* @in */) override;
                                                   
        // IConfiguration interface
        uint32_t Configure(PluginHost::IShell* shell);

    private:
        template <typename KeyType, typename ValueType>
        class RegistryMap {
        public:
            void Add(const KeyType& key, const ValueType& value) {
                std::lock_guard<std::mutex> lock(mContextMutex);
                mContextMap[key] = value;
            }

            void Remove(const KeyType& key) {
                std::lock_guard<std::mutex> lock(mContextMutex);
                mContextMap.erase(key);
            }

            bool Get(const KeyType& key, ValueType& value) {
                std::lock_guard<std::mutex> lock(mContextMutex);
                auto it = mContextMap.find(key);
                if (it != mContextMap.end()) {
                    value = it->second;
                    return true;
                }
                return false;
            }
            std::unordered_map<KeyType, ValueType> mContextMap;
            std::mutex mContextMutex;
        };

        // Instantiate RegistryMap for KeyType of string and ValueType of Context
        RegistryMap<std::string, Context> mProviderRegistry;

        // Transaction map
        RegistryMap<std::string, Context> mTransactionRegistry;

        class EXTERNAL ForwardJob : public Core::IDispatch
        {
        protected:
            ForwardJob(App2AppProviderImplementation *parent, 
            const Context& context,
            const std::string& payload
            )
                : mParent(*parent), mPayload(payload), mContext(context)
            {
            }

        public:
            ForwardJob() = delete;
            ForwardJob(const ForwardJob &) = delete;
            ForwardJob &operator=(const ForwardJob &) = delete;
            ~ForwardJob()
            {
            }

        public:
            static Core::ProxyType<Core::IDispatch> Create(App2AppProviderImplementation *parent,
                const Context& context, const std::string& payload)
            {
                return (Core::ProxyType<Core::IDispatch>(Core::ProxyType<ForwardJob>::Create(parent, context, payload)));
            }
            virtual void Dispatch()
            {
                if (ContextUtils::IsOriginGateway(mContext.origin)) {
                    mParent.SendToGateway(mContext, mPayload);
                } else {
                    mParent.SendToLaunchDelegate(mContext, mPayload);
                }
            }

        private:
            App2AppProviderImplementation &mParent;
            const std::string mPayload;
            const Context mContext;
        };

        PluginHost::IShell* mShell;
        Exchange::IAppGatewayResponderInternal *mAppGateway; 
        Exchange::IAppGatewayResponderInternal *mLaunchDelegate;

        void SendToGateway(const Context& context, const string& payload);
        void SendToLaunchDelegate(const Context& context, const string& payload);
        bool ExtractCorrelationIdAndKey(const string &payload, const string &key, string& correlationId, string& result );
        void BrokerProvider(const Context &context, const Context providerContext, const JsonObject paramsObject);
    };
}
}
