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
#include "App2AppProviderImplementation.h"
#include "UtilsLogging.h"
#include "StringUtils.h"
#include "UtilsUUID.h"
#include "UtilsCallsign.h"



namespace WPEFramework
{
    namespace Plugin
    {

        SERVICE_REGISTRATION(App2AppProviderImplementation, 1, 0);

        App2AppProviderImplementation::App2AppProviderImplementation() : 
        mShell(nullptr),
        mAppGateway(nullptr),
        mLaunchDelegate(nullptr)
        {
        }

        App2AppProviderImplementation::~App2AppProviderImplementation()
        {
            // Cleanup resources if needed
            if (mShell != nullptr)
            {
                mShell->Release();
                mShell = nullptr;
            }

            if (mAppGateway != nullptr)
            {
                mAppGateway->Release();
                mAppGateway = nullptr;
            }

            if (mLaunchDelegate != nullptr)
            {
                mLaunchDelegate->Release();
                mLaunchDelegate = nullptr;
            }
        }

        Core::hresult App2AppProviderImplementation::RegisterProvider(const Context &context /* @in */,
        const bool& provide ,
        const string &capability
        ) {
            string lower_c = StringUtils::toLower(capability);
            if (provide) {
                mProviderRegistry.Add(lower_c, context);
                // Add additional entry for cases where same app provides capability with a suffix
                // for eg create is a capability and multiple apps can create the same capability
                // we will have 2 entries <create,context> and <create.appId, context>
                if (!context.appId.empty()) {
                    string lower_app_id = StringUtils::toLower(context.appId);
                    string combinedKey = lower_c + "." + lower_app_id;
                    mProviderRegistry.Add(combinedKey, context);
                }
            } else {
                // Even if same capability is provided by multiple apps, we will remove both entries
                // Given there is an established precedence that this particular capability is a composite
                // key. For any provider deregistration we need to clean up the entry with just the
                // capability. From this point onwards the Consumer MUST make the request with appId
                // which is registered as the only other available provider
                mProviderRegistry.Remove(lower_c);
                if (!context.appId.empty()) {
                    string lower_app_id = StringUtils::toLower(context.appId);
                    string combinedKey = lower_c + "." + lower_app_id;
                    mProviderRegistry.Remove(combinedKey);
                }
            }
            return Core::ERROR_NONE;
        }

        Core::hresult App2AppProviderImplementation::InvokeProvider(const Context &context /* @in */,
        const string &capability ,
        const string &params
        ) {
            string lower_c = StringUtils::toLower(capability);
            JsonObject paramsObject;
            if (paramsObject.FromString(params)) {
                Context providerContext;
                // Check if paramsObject has an entry called appId string
                // if so get the appId
                if (paramsObject.HasLabel("appId") && paramsObject["appId"].IsSet() && paramsObject["appId"].Content() == JsonValue::type::STRING) {
                    string appId = paramsObject["appId"].String();
                    if (!appId.empty()) {
                        string lower_app_id = StringUtils::toLower(appId);
                        string combinedKey = lower_c + "." + lower_app_id;
                        if (mProviderRegistry.Get(combinedKey, providerContext)) {
                            BrokerProvider(context, std::move(providerContext), std::move(paramsObject));
                            return Core::ERROR_NONE;
                        }
                    }
                }

                if (mProviderRegistry.Get(lower_c, providerContext)) {
                    BrokerProvider(context, std::move(providerContext), std::move(paramsObject));
                    return Core::ERROR_NONE;
                }
            }

            return Core::ERROR_GENERAL;
        }

        void App2AppProviderImplementation::BrokerProvider(const Context &context, const Context providerContext, const JsonObject paramsObject) {
            // Check if paramsObject has an entry called appId string
            // if so get the appId
            string correlationId = UtilsUUID::GenerateUUID();
            mTransactionRegistry.Add(correlationId, context);
            JsonObject object;
            object["correlationId"] = correlationId;
            object["params"] = paramsObject;
            string dispatchParams;
            object.ToString(dispatchParams);
            LOGINFO("Invoking provider %s with correlationId %s", dispatchParams.c_str(), correlationId.c_str());
            // Dispatch to provider
            Core::IWorkerPool::Instance().Submit(ForwardJob::Create(this, providerContext, dispatchParams));

        }

        bool App2AppProviderImplementation::ExtractCorrelationIdAndKey(const string &payload, const string &key, string& correlationId, string& result ) {
            JsonObject responseObject;
            if (responseObject.FromString(payload)) {
                if (responseObject.IsSet()) {
                    JsonValue cid = responseObject["correlationId"];
                    if (cid.IsSet() && !cid.IsNull()) {
                        correlationId = cid.String();
                        JsonValue resultObject = responseObject[key.c_str()];
                        if (resultObject.IsSet()) {
                            result = resultObject.String();
                            return true;
                        }
                    }
                }
            }
            return false;
        }


        Core::hresult App2AppProviderImplementation::HandleProviderResponse(const string &payload /* @in @opaque */,
        const string &capability /* @in */
        ) {
            string correlationId;
            string result;
            if (ExtractCorrelationIdAndKey(payload, "result", correlationId, result)) {
                Context requestContext;
                if (mTransactionRegistry.Get(correlationId, requestContext)) {
                    mTransactionRegistry.Remove(correlationId);

                    // Forward response to gateway or launch delegate based on origin
                    Core::IWorkerPool::Instance().Submit(ForwardJob::Create(this, requestContext, result));
                    return Core::ERROR_NONE;
                } else {
                    LOGERR("No matching transaction for correlationId %s", correlationId.c_str());
                }
                return Core::ERROR_NONE;
            }

            return Core::ERROR_GENERAL;
        }

        Core::hresult App2AppProviderImplementation::HandleProviderError(const string &payload /* @in @opaque */,
        const string &capability /* @in */
        ) {
            return Core::ERROR_NONE;
        }

        Core::hresult App2AppProviderImplementation::Cleanup(const uint32_t connectionId /* @in */, const string &origin /* @in */) 
        {
            LOGINFO("Cleanup [connectionId=%d origin=%s]", connectionId, origin.c_str());
            return Core::ERROR_NONE;
        }
        

        uint32_t App2AppProviderImplementation::Configure(PluginHost::IShell *shell)
        {
            LOGINFO("Configuring App2AppProviderImplementation");
            uint32_t result = Core::ERROR_NONE;
            ASSERT(shell != nullptr);
            mShell = shell;
            mShell->AddRef();
            return result;
        }

        void App2AppProviderImplementation::SendToGateway(const Context& context, const string& payload)
        {
            if (mAppGateway == nullptr) {
                mAppGateway = mShell->QueryInterfaceByCallsign<Exchange::IAppGatewayResponderInternal>(APP_GATEWAY_CALLSIGN);
                if (mAppGateway==nullptr) {
                    LOGERR("AppGateway interface not available");
                    return;
                }
            }

            Exchange::Context gatewayContext = ContextUtils::ConvertProviderToAppGatewayContext(context);
            mAppGateway->Respond(gatewayContext, payload);

        }

        void App2AppProviderImplementation::SendToLaunchDelegate(const Context& context, const string& payload)
        {
            if ( mLaunchDelegate==nullptr ) {
                mLaunchDelegate = mShell->QueryInterfaceByCallsign<Exchange::IAppGatewayResponderInternal>(INTERNAL_GATEWAY_CALLSIGN);
                if (mLaunchDelegate == nullptr) {
                    LOGERR("Launch Delegate Not available");
                    return;
                }
            }

            Exchange::Context gatewayContext = ContextUtils::ConvertProviderToAppGatewayContext(context);
            mLaunchDelegate->Respond(gatewayContext, payload);

        }
        
    }
}