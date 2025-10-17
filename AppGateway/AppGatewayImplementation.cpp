/**
 * If not stated otherwise in this file or this component's LICENSE
 * file the following copyright and licenses apply:
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
 **/

#include <string>
#include <plugins/JSONRPC.h>
#include <plugins/IShell.h>
#include "AppGatewayImplementation.h"
#include "UtilsLogging.h"
#include "ContextUtils.h"
#include "ObjectUtils.h"
#include <fstream>
#include <streambuf>
#include "UtilsConnections.h"
#include "UtilsCallsign.h"
#include "UtilsFirebolt.h"
#define APPGATEWAY_SOCKET_ADDRESS "0.0.0.0:3473"
#define DEFAULT_CONFIG_PATH "/etc/app-gateway/resolution.base.json"

//#define APPGATEWAY_SOCKET_ADDRESS "0.0.0.0:3473"

namespace WPEFramework
{
    namespace Plugin
    {
        SERVICE_REGISTRATION(AppGatewayImplementation, 1, 0, 0);

        class ResolutionPaths : public Core::JSON::Container
        {
        private:
            ResolutionPaths(const ResolutionPaths &) = delete;
            ResolutionPaths &operator=(const ResolutionPaths &) = delete;
        public:
            ResolutionPaths()
            : Core::JSON::Container()
            {
                Add(_T("paths"), &paths);
            }
            ~ResolutionPaths()
            {}
            std::vector<std::string> GetConfigPaths() {
                std::vector<std::string> configPaths;
                auto index = paths.Elements();
                while (index.Next()) {
                    configPaths.push_back(index.Current().Value().c_str());
                }
                return configPaths;
            }

        public:
            Core::JSON::ArrayType<Core::JSON::String> paths;

        };

        AppGatewayImplementation::AppGatewayImplementation()
            : mService(nullptr),
            mResolverPtr(nullptr), 
            mWsManager(), 
            mAppNotifications(nullptr),
	    mAuthenticator(nullptr),
            mInternalGatewayResponder(nullptr), 
            mApp2AppProvider(nullptr),
	    mAppIdRegistry()
        {
            LOGINFO("AppGatewayImplementation constructor");
        }

        AppGatewayImplementation::~AppGatewayImplementation()
        {
            LOGINFO("AppGatewayImplementation destructor");
            if (nullptr != mService)
            {
                mService->Release();
                mService = nullptr;
            }

            if (nullptr != mAppNotifications)
            {
                mAppNotifications->Release();
                mAppNotifications = nullptr;
            }

            if (nullptr != mInternalGatewayResponder)
            {
                mInternalGatewayResponder->Release();
                mInternalGatewayResponder = nullptr;
            }

            if (nullptr != mApp2AppProvider)
            {
                mApp2AppProvider->Release();
                mApp2AppProvider = nullptr;
            }

            if (nullptr != mAuthenticator)
            {
                mAuthenticator->Release();
                mAuthenticator = nullptr;
            }

            // Shared pointer will automatically clean up
            mResolverPtr.reset();
        }

        uint32_t AppGatewayImplementation::Configure(PluginHost::IShell *shell)
        {
            LOGINFO("Configuring AppGateway");
            uint32_t result = Core::ERROR_NONE;
            ASSERT(shell != nullptr);
            mService = shell;
            mService->AddRef();

            result = InitializeResolver();
            if (Core::ERROR_NONE != result) {
                return result;
            }

            result = InitializeWebsocket();

            return result;
        }
        
        uint32_t AppGatewayImplementation::InitializeResolver() {
            // Initialize resolver after setting mService
            mResolverPtr = std::make_shared<Resolver>(mService, DEFAULT_CONFIG_PATH);
            if (mResolverPtr == nullptr)
            {
                LOGERR("Failed to create Resolver instance");
                return Core::ERROR_GENERAL;
            }

            ResolutionPaths paths;
            Core::OptionalType<Core::JSON::Error> error;
            std::ifstream resolutionPathsFile(PLUGIN_PRODUCT_CFG);

            if(!resolutionPathsFile.is_open())
            {
                LOGERR("Failed to open AppGateway config file: %s", PLUGIN_PRODUCT_CFG);
                return Core::ERROR_GENERAL;
            }
            else
            {
                std::string resolutionPathsContent((std::istreambuf_iterator<char>(resolutionPathsFile)), std::istreambuf_iterator<char>());
                if (paths.FromString(resolutionPathsContent, error) == false)
                {
                    LOGERR("Failed to parse AppGateway config file, error: '%s', content: '%s'.",
                           (error.IsSet() ? error.Value().Message().c_str() : "Unknown"),
                           resolutionPathsContent.c_str());
                } else {
                    auto configPaths = paths.GetConfigPaths();
                    if (configPaths.empty()) {
                        LOGERR("No configuration paths found in AppGateway config file");
                        return Core::ERROR_BAD_REQUEST;
                    }
                    LOGINFO("Found %zu configuration paths in AppGateway config file", configPaths.size());
                    Core::hresult configResult = InternalResolutionConfigure(std::move(configPaths));
                    if (configResult != Core::ERROR_NONE) {
                        LOGERR("Failed to configure resolutions from provided paths");
                        return configResult;
                    }
                }
            }
            return Core::ERROR_NONE;
        }

        uint32_t AppGatewayImplementation::InitializeWebsocket(){
            // Initialize WebSocket server
            WebSocketConnectionManager::Config config(APPGATEWAY_SOCKET_ADDRESS);
            std::string configLine = mService->ConfigLine();
            Core::OptionalType<Core::JSON::Error> error;
            if (config.FromString(configLine, error) == false)
            {
                LOGERR("Failed to parse config line, error: '%s', config line: '%s'.",
                       (error.IsSet() ? error.Value().Message().c_str() : "Unknown"),
                       configLine.c_str());
            }

            LOGINFO("Connector: %s", config.Connector.Value().c_str());
            Core::NodeId source(config.Connector.Value().c_str());
            LOGINFO("Parsed port: %d", source.PortNumber());
            mWsManager.SetMessageHandler(
                [this](const std::string &method, const std::string &params, const int requestId, const uint32_t connectionId)
                {
                    Core::IWorkerPool::Instance().Submit(WsMsgJob::Create(this, method, params, requestId, connectionId));
                });

            mWsManager.SetAuthHandler(
                [this](const uint32_t connectionId, const std::string &token) -> bool
                {
                    string sessionId = Utils::ResolveQuery(token, "session");
                    if (sessionId.empty())
                    {
                        LOGERR("No session token provided");
                        return false;
                    }

                    if ( mAuthenticator==nullptr ) {
                        mAuthenticator = mService->QueryInterfaceByCallsign<Exchange::IAppGatewayAuthenticatorInternal>(GATEWAY_AUTHENTICATOR_CALLSIGN);
                        if (mAuthenticator == nullptr) {
                            LOGERR("Authenticator Not available");
                            return false;
                        }
                    }

                    string appId;
                    if (Core::ERROR_NONE == mAuthenticator->Authenticate(sessionId,appId)) {
                        LOGINFO("APP ID %s", appId.c_str());
                        mAppIdRegistry.Add(connectionId, std::move(appId));
                        return true;
                    }

                    return false;
                });

            mWsManager.SetDisconnectHandler(
                [this](const uint32_t connectionId)
                {
                    LOGINFO("Connection disconnected: %d", connectionId);
                    mAppIdRegistry.Remove(connectionId);
                    if (mAppNotifications != nullptr) {
                        if (Core::ERROR_NONE != mAppNotifications->Cleanup(connectionId, APP_GATEWAY_CALLSIGN)) {
                            LOGERR("AppNotifications Cleanup failed for connectionId: %d", connectionId);
                        }
                    }
                    
                    if (mApp2AppProvider != nullptr) {
                        if (Core::ERROR_NONE != mApp2AppProvider->Cleanup(connectionId, APP_GATEWAY_CALLSIGN)) {
                            LOGERR("App2AppProvider Cleanup failed for connectionId: %d", connectionId);
                        }
                    }
                }
            );
            mWsManager.Start(source);
            return Core::ERROR_NONE;
        }

        Core::hresult AppGatewayImplementation::Configure(Exchange::IAppGateway::IStringIterator *const &paths)
        {
            LOGINFO("Call AppGatewayImplementation::Configure");

            if (paths == nullptr)
            {
                LOGERR("Configure called with null paths iterator");
                return Core::ERROR_BAD_REQUEST;
            }

            if (mResolverPtr == nullptr)
            {
                LOGERR("Resolver not initialized");
                return Core::ERROR_GENERAL;
            }

            // Clear existing resolutions before loading new configuration
            // mResolverPtr->ClearResolutions();

            std::vector<std::string> configPaths;

            // Collect all paths first
            std::string currentPath;
            while (paths->Next(currentPath) == true)
            {
                configPaths.push_back(currentPath);
                LOGINFO("Found config path: %s", currentPath.c_str());
            }

            if (configPaths.empty())
            {
                LOGERR("No valid configuration paths provided");
                return Core::ERROR_BAD_REQUEST;
            }

            LOGINFO("Processing %zu configuration paths in override order", configPaths.size());
            return InternalResolutionConfigure(std::move(configPaths));
            
        }

        Core::hresult AppGatewayImplementation::InternalResolutionConfigure(std::vector<std::string> configPaths){
            // Process all paths in order - later paths override earlier ones
            bool anyConfigLoaded = false;
            for (size_t i = 0; i < configPaths.size(); i++)
            {
                const std::string &configPath = configPaths[i];
                LOGINFO("Processing config path %zu/%zu: %s", i + 1, configPaths.size(), configPath.c_str());

                if (mResolverPtr->LoadConfig(configPath))
                {
                    LOGINFO("Successfully loaded configuration from: %s", configPath.c_str());
                    anyConfigLoaded = true;
                }
                else
                {
                    LOGERR("Failed to load configuration from: %s", configPath.c_str());
                    // Continue processing other paths instead of failing completely
                }
            }

            if (!anyConfigLoaded)
            {
                LOGERR("Failed to load configuration from any provided path");
                return Core::ERROR_GENERAL;
            }

            LOGINFO("Configuration complete. Final resolutions loaded with override priority (later paths take precedence)");
            return Core::ERROR_NONE;

        }



        Core::hresult AppGatewayImplementation::Respond(const Context &context, const string &payload)
        {
            LOGINFO("Call AppGatewayImplementation::Respond");
            Core::IWorkerPool::Instance().Submit(RespondJob::Create(this, context.connectionId, context.requestId, payload, APP_GATEWAY_CALLSIGN));

            return Core::ERROR_NONE;
        }

        Core::hresult AppGatewayImplementation::Resolve(const Context &context, const string &method, const string &params)
        {
            LOGINFO("method=%s params=%s", method.c_str(), params.c_str());
            return InternalResolve(context, method, params, INTERNAL_GATEWAY_CALLSIGN);
        }

        Core::hresult AppGatewayImplementation::InternalResolve(const Context &context, const string &method, const string &params, const string &origin)
        {
            string resolution;
            Core::hresult result = FetchResolvedData(context, method, params, origin, resolution);

            LOGINFO("Final resolution: %s", resolution.c_str());
            Core::IWorkerPool::Instance().Submit(RespondJob::Create(this, context.connectionId, context.requestId, resolution, origin));
            return result;
        }

        Core::hresult AppGatewayImplementation::FetchResolvedData(const Context &context, const string &method, const string &params, const string &origin, string& resolution) {
            JsonObject params_obj;
            Core::hresult result = Core::ERROR_NONE;
            if (mResolverPtr == nullptr)
            {
                LOGERR("Resolver not initialized");
                ErrorUtils::CustomInitialize("Resolver not initialized", resolution);
                return Core::ERROR_GENERAL;
            }

            // Check if resolver has any resolutions loaded
            if (!mResolverPtr->IsConfigured())
            {
                LOGERR("Resolver not configured - no resolutions loaded. Call Configure() first.");
                ErrorUtils::CustomInitialize("Resolver not configured", resolution);
                return Core::ERROR_GENERAL;
            }
            // Resolve the alias from the method
            std::string alias = mResolverPtr->ResolveAlias(method);

            if (alias.empty())
            {
                LOGERR("No alias found for method: %s", method.c_str());
                ErrorUtils::NotSupported(resolution);
                return Core::ERROR_GENERAL;
            }
            LOGDBG("Resolved method '%s' to alias '%s'", method.c_str(), alias.c_str());
            string providerCapability;
            ProviderMethodType methodType;
            
            // Check if the given method is an event
            if (mResolverPtr->HasEvent(method)) {
                result = PreProcessEvent(context, alias, method, origin, params, resolution);
            } else if(mResolverPtr->HasProviderCapability(method, providerCapability, methodType)) {
                result = PreProcessProvider(context, method, params, providerCapability, methodType, origin, resolution);
            } else if(mResolverPtr->HasComRpcRequestSupport(method)) {
                result = ProcessComRpcRequest(context, alias, method, params, resolution);
            } else {
                // Check if includeContext is enabled for this method
                std::string finalParams = UpdateContext(context, method, params);
                LOGDBG("Final Request params alias=%s Params = %s", alias.c_str(), finalParams.c_str());

                result = mResolverPtr->CallThunderPlugin(alias, finalParams, resolution);
                if (result != Core::ERROR_NONE) {
                    LOGERR("Failed to retrieve resolution from Thunder method %s", alias.c_str());
                    ErrorUtils::CustomInternal("Failed with internal error", resolution);
                } else {
                    if (resolution.empty()) {
                        resolution = "null";
                    }
                }
            }
            return result;
        }
        
        string AppGatewayImplementation::UpdateContext(const Context &context, const string& method, const string& params){
            // Check if includeContext is enabled for this method
            std::string finalParams = params;
            if (mResolverPtr->HasIncludeContext(method)) {
                LOGDBG("Method '%s' requires context inclusion", method.c_str());

                // Construct params with context
                JsonObject contextObj;
                contextObj["appId"] = context.appId;
                contextObj["connectionId"] = context.connectionId;
                contextObj["requestId"] = context.requestId;
                JsonObject paramsObj;
                if (!paramsObj.FromString(params))
                {
                    LOGWARN("Failed to parse original params as JSON: %s", params.c_str());
                }
                paramsObj["context"] = contextObj;

                paramsObj.ToString(finalParams);

                LOGDBG("Modified params with context: %s", finalParams.c_str());
            }
            return finalParams;
        }

        uint32_t AppGatewayImplementation::ProcessComRpcRequest(const Context &context, const string& alias, const string& method, const string& params, string &resolution) {
            uint32_t result = Core::ERROR_GENERAL;
            Exchange::IAppGatewayRequestHandler *requestHandler = mService->QueryInterfaceByCallsign<Exchange::IAppGatewayRequestHandler>(alias);
            if (requestHandler != nullptr) {
                if (Core::ERROR_NONE != requestHandler->HandleAppGatewayRequest(context, method, params, resolution)) {
                    LOGERR("HandleAppGatewayRequest failed for callsign: %s", alias.c_str());
                    ErrorUtils::CustomInternal("HandleAppGatewayRequest failed", resolution);
                } else {
                    result = Core::ERROR_NONE;
                }
                requestHandler->Release();
            } else {
                LOGERR("Bad configuration %s Not available with COM RPC", alias.c_str());
                ErrorUtils::NotAvailable(resolution);
            }

            return result;
        }
        

        uint32_t AppGatewayImplementation::PreProcessEvent(const Context &context, const string& alias, const string &method, const string& origin, const string& params,
        string &resolution) {
            JsonObject params_obj;
            if (params_obj.FromString(params)) {
                    bool resultValue;
                    // Use ObjectUtils::HasBooleanEntry and populate resultValue
                    if (ObjectUtils::HasBooleanEntry(params_obj, "listen", resultValue)) {
                        LOGDBG("Event method '%s' with listen: %s", method.c_str(), resultValue ? "true" : "false");
                        auto ret_value = HandleEvent(context, alias, method, origin, resultValue);
                        JsonObject returnData;
                        JsonObject returnResult;
                        returnResult["listening"] = resultValue;
                        returnResult["event"] = method;
                        returnData["result"] = returnResult;
                        returnData.ToString(resolution);
                        return ret_value;
                    } else {
                        LOGERR("Event method '%s' missing required boolean 'listen' parameter", method.c_str());
                        ErrorUtils::CustomBadRequest("Missing required boolean 'listen' parameter", resolution);
                        return Core::ERROR_BAD_REQUEST;
                    }
            } else {
                    LOGERR("Event method '%s' called without parameters", method.c_str());
                    ErrorUtils::CustomBadRequest("Event methods require parameters", resolution);
                    return Core::ERROR_BAD_REQUEST;
            }
        }

        uint32_t AppGatewayImplementation::PreProcessProvider(const Context &context, const string& method, const string& params, 
            const string& providerCapability, const ProviderMethodType &methodType, 
            const string &origin, string &resolution){
            LOGDBG("params=%s providerCapability=%s ", params.c_str(), providerCapability.c_str());
            JsonObject params_obj;
            if (mApp2AppProvider == nullptr) {
                mApp2AppProvider = mService->QueryInterfaceByCallsign<Exchange::IApp2AppProvider>(APP_TO_APP_PROVIDER_CALLSIGN);
                if (mApp2AppProvider == nullptr) {
                    LOGERR("IApp2AppProvider interface not available");
                    return Core::ERROR_GENERAL;
                }
            }
            Exchange::IApp2AppProvider::Context providerContext = ContextUtils::ConvertAppGatewayToProviderContext(context, origin);
            switch (methodType) {
                case ProviderMethodType::REGISTER:
                    LOGDBG("Register %s ", providerCapability.c_str());
                    bool resultValue;
                    if (params_obj.FromString(params)) {
                        if (ObjectUtils::HasBooleanEntry(params_obj, "listen", resultValue)) {
                            LOGDBG("Invoking RegisterProvider for capability '%s'", providerCapability.c_str());
                            JsonObject returnData;
                            JsonObject returnResult;
                            returnResult["listening"] = resultValue;
                            returnResult["event"] = method;
                            returnData["result"] = returnResult;
                            returnData.ToString(resolution);
                            return mApp2AppProvider->RegisterProvider(providerContext, resultValue, providerCapability);
                        } else {
                            ErrorUtils::CustomBadRequest("Event methods require parameters", resolution);
                        }
                    }
                    else {
                        LOGERR("Failed to parse params as JSON: %s", params.c_str());
                        ErrorUtils::CustomBadRequest("Invalid JSON format", resolution);
                    }
                    break;
                case ProviderMethodType::INVOKE:
                    LOGDBG("Invoking InvokeProvider for capability '%s' with params: %s", providerCapability.c_str(), params.c_str());
                    return mApp2AppProvider->InvokeProvider(providerContext, providerCapability, params);
                
                case ProviderMethodType::RESULT:
                    LOGDBG("Invoking ResultProvider for capability '%s' with params: %s", providerCapability.c_str(), params.c_str());
                    return mApp2AppProvider->HandleProviderResponse(providerCapability, params);
                case ProviderMethodType::ERROR:
                    LOGDBG("Invoking ResultProvider for capability '%s' with params: %s", providerCapability.c_str(), params.c_str());
                    return mApp2AppProvider->HandleProviderError(providerCapability, params);
                default:
                    LOGERR("Unknown ProviderMethodType for method '%s'", providerCapability.c_str());
                    
                    ErrorUtils::CustomBadRequest("Unknown ProviderMethodType", resolution);
                    return Core::ERROR_BAD_REQUEST;
            }

            return Core::ERROR_GENERAL;
        }

        Core::hresult AppGatewayImplementation::HandleEvent(const Context &context, const string &alias,  const string &event, const string &origin, const bool listen) {
            if (mAppNotifications == nullptr) {
                mAppNotifications = mService->QueryInterfaceByCallsign<Exchange::IAppNotifications>(APP_NOTIFICATIONS_CALLSIGN);
                if (mAppNotifications == nullptr) {
                    LOGERR("IAppNotifications interface not available");
                    return Core::ERROR_GENERAL;
                }
            }

            return mAppNotifications->Subscribe(ContextUtils::ConvertAppGatewayToNotificationContext(context,origin), listen, alias, event);
        }

        void AppGatewayImplementation::DispatchWsMsg(const std::string &method,
                                                     const std::string &params,
                                                     const int requestId,
                                                     const uint32_t connectionId)
        {

            LOGINFO("Received message: method=%s, params=%s, requestId=%d, connectionId=%d",
                    method.c_str(), params.c_str(), requestId, connectionId);

            std::string resolution;
            string appId;

            if (mAppIdRegistry.Get(connectionId, appId)) {
                // App Id is available
                Context context = {
                requestId,
                connectionId,
                std::move(appId)};
                InternalResolve(context, method, params, APP_GATEWAY_CALLSIGN);

            } else {
                LOGERR("No App ID found for connection %d. Terminate connection", connectionId);
                mWsManager.Close(connectionId);
            }
        }

        void AppGatewayImplementation::SendToLaunchDelegate(const Context& context, const string& payload)
        {
            if ( mInternalGatewayResponder==nullptr ) {
                mInternalGatewayResponder = mService->QueryInterfaceByCallsign<Exchange::IAppGatewayResponderInternal>(INTERNAL_GATEWAY_CALLSIGN);
                if (mInternalGatewayResponder == nullptr) {
                    LOGERR("Internal Responder not available Not available");
                    return;
                }
            }

            mInternalGatewayResponder->Respond(context, payload);

        }

    } // namespace Plugin
} // namespace WPEFramework
