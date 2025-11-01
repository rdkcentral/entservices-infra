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

#include <stdlib.h>
#include <errno.h>
#include <string>
#include <iomanip>
#include <iostream>

#include "ResourceManagerImplementation.h"
#include "UtilsgetRFCConfig.h"
#include "UtilsLogging.h"

static std::string sThunderSecurityToken;

#ifdef ENABLE_DEBUG
#define DBGINFO(fmt, ...) LOGINFO(fmt, ##__VA_ARGS__)
#else
#define DBGINFO(fmt, ...)
#endif

namespace WPEFramework {
    namespace Plugin {

        SERVICE_REGISTRATION(ResourceManagerImplementation, 1, 0);

        ResourceManagerImplementation* ResourceManagerImplementation::_instance = nullptr;

    /* static */ ResourceManagerImplementation* ResourceManagerImplementation::instance(ResourceManagerImplementation* resourceManagerImpl)
    {
        if (resourceManagerImpl != nullptr) {
            _instance = resourceManagerImpl;
        }
        return _instance;
    }

    ResourceManagerImplementation::ResourceManagerImplementation()
        : _adminLock()
        , _service(nullptr)
#if defined(ENABLE_ERM) || defined(ENABLE_L1TEST)
        , mEssRMgr(nullptr)
#endif
        , mDisableBlacklist(true)
        , mDisableReserveTTS(true)
        , mAppsAVBlacklistStatus()
    {
        LOGINFO("ResourceManagerImplementation Constructor");

        // Set static instance
        _instance = this;

#ifdef ENABLE_ERM
        mEssRMgr = EssRMgrCreate();
        std::cout << "EssRMgrCreate " << ((mEssRMgr != nullptr) ? "succeeded" : "failed") << std::endl;

        RFC_ParamData_t param;
        mDisableBlacklist = true;
        mDisableReserveTTS = true;

        if (true == Utils::getRFCConfig("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Resourcemanager.Blacklist.Enable", param)) {
            mDisableBlacklist = ((param.type == WDMP_BOOLEAN) && (strncasecmp(param.value, "false", 5) == 0));
        }
        if (true == Utils::getRFCConfig("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Resourcemanager.ReserveTTS.Enable", param)) {
            mDisableReserveTTS = ((param.type == WDMP_BOOLEAN) && (strncasecmp(param.value, "false", 5) == 0));
        }

        std::cout << "RFC Blacklist disabled: " << std::boolalpha << mDisableBlacklist << std::endl;
        std::cout << "RFC ReserveTTS disabled: " << std::boolalpha << mDisableReserveTTS << std::endl;
#else
        std::cout << "ENABLE_ERM not defined" << std::endl;
#endif
    }

    ResourceManagerImplementation::~ResourceManagerImplementation() 
    {
        LOGINFO("ResourceManagerImplementation Destructor");

#ifdef ENABLE_ERM
        if (mEssRMgr != nullptr) {
            EssRMgrDestroy(mEssRMgr);
            mEssRMgr = nullptr;
        }
#endif

        if (_instance == this) {
            _instance = nullptr;
        }
    }

    /* virtual */ Core::hresult ResourceManagerImplementation::SetAVBlocked(const string& appId, const bool blocked, bool& success) /* override */
    {
        LOGINFO("SetAVBlocked: appId=%s, blocked=%s", appId.c_str(), blocked ? "true" : "false");
        
        Core::hresult result = Core::ERROR_GENERAL;
        success = false;

        _adminLock.Lock();

        try {
            // Check if ERM is available and blacklist is not disabled
            if ((nullptr != mEssRMgr) && (false == mDisableBlacklist)) {
                
                std::cout << "appid: " << appId << std::endl;
                std::cout << "blocked: " << std::boolalpha << blocked << std::endl;

#ifdef ENABLE_ERM
                bool status = blocked ? 
                    EssRMgrAddToBlackList(mEssRMgr, appId.c_str()) : 
                    EssRMgrRemoveFromBlackList(mEssRMgr, appId.c_str());
                
                std::cout << "setAVBlocked call returning " << std::boolalpha << status << std::endl;
                
                if (true == status) {
                    mAppsAVBlacklistStatus[appId] = blocked;
                    std::cout << "mAppsAVBlacklistStatus updated" << std::endl;
                    result = Core::ERROR_NONE;
                    success = true;
                } else {
                    LOGERR("ERM failed to %s application: %s", blocked ? "block" : "unblock", appId.c_str());
                    result = Core::ERROR_GENERAL;
                }
#else
                LOGERR("ENABLE_ERM not defined");
                result = Core::ERROR_UNAVAILABLE;
#endif
                
            } else {
                if (mDisableBlacklist) {
                    LOGWARN("Blacklist RFC is disabled");
                    result = Core::ERROR_UNAVAILABLE;
                } else {
                    LOGERR("ERM not enabled");
                    result = Core::ERROR_UNAVAILABLE;
                }
            }
        } catch (const std::exception& e) {
            LOGERR("Exception in SetAVBlocked: %s", e.what());
            result = Core::ERROR_GENERAL;
        }
        _adminLock.Unlock();

        return result;
    }

    /* virtual */ Core::hresult ResourceManagerImplementation::GetBlockedAVApplications(IClientIterator*& clients, bool& success) const /* override */
    {
        LOGINFO("GetBlockedAVApplications");
        
        Core::hresult result = Core::ERROR_GENERAL;
        clients = nullptr;
        success = false;

        _adminLock.Lock();

        try {
            if (nullptr != mEssRMgr) {
                std::list<string> blockedApps;
                
                std::cout << "iterating mAppsAVBlacklistStatus ..." << std::endl;
                
#ifdef ENABLE_ERM
                // Iterate through the blacklist status map
                std::map<std::string, bool>::const_iterator appsItr = mAppsAVBlacklistStatus.begin();
                for (; appsItr != mAppsAVBlacklistStatus.end(); appsItr++) {
                    std::cout << "app: " << appsItr->first << std::endl;
                    std::cout << "blocked: " << std::boolalpha << appsItr->second << std::endl;
                    
                    if (true == appsItr->second) {
                        blockedApps.push_back(appsItr->first);
                        LOGINFO("Found blocked app: %s", appsItr->first.c_str());
                    }
                }
                
                // Create iterator from the blocked apps list
                if (!blockedApps.empty()) {
                    clients = Core::Service<RPC::StringIterator>::Create<IClientIterator>(blockedApps);
                    result = Core::ERROR_NONE;
                    success = true;
                    LOGINFO("Successfully retrieved %zu blocked applications", blockedApps.size());
                } else {
                    // Return empty iterator for no blocked apps
                    clients = Core::Service<RPC::StringIterator>::Create<IClientIterator>(blockedApps);
                    result = Core::ERROR_NONE;
                    success = true;
                    LOGINFO("No blocked applications found");
                }
#else
                LOGERR("ENABLE_ERM not defined");
                result = Core::ERROR_UNAVAILABLE;
#endif
                
            } else {
                LOGERR("ERM not enabled");
                result = Core::ERROR_UNAVAILABLE;
            }
        } catch (const std::exception& e) {
            LOGERR("Exception in GetBlockedAVApplications: %s", e.what());
            result = Core::ERROR_GENERAL;
        }

        _adminLock.Unlock();

        return result;
    }

    // JSONRPCDirectLink helper class for service invocations 
    struct JSONRPCDirectLink
    {
    private:
      uint32_t mId { 0 };
      std::string mCallSign { };
#if ((THUNDER_VERSION >= 4) && (THUNDER_VERSION_MINOR == 4))
      PluginHost::ILocalDispatcher * dispatcher_ {nullptr};
#else
      PluginHost::IDispatcher * dispatcher_ {nullptr};
#endif

      Core::ProxyType<Core::JSONRPC::Message> Message() const
      {
        return (Core::ProxyType<Core::JSONRPC::Message>(PluginHost::IFactories::Instance().JSONRPC()));
      }

      template <typename PARAMETERS>
      bool ToMessage(PARAMETERS& parameters, Core::ProxyType<Core::JSONRPC::Message>& message) const
      {
        return ToMessage((Core::JSON::IElement*)(&parameters), message);
      }
      bool ToMessage(Core::JSON::IElement* parameters, Core::ProxyType<Core::JSONRPC::Message>& message) const
      {
        if (!parameters->IsSet())
          return true;
        string values;
        if (!parameters->ToString(values))
        {
          std::cout << "Failed to convert params to string\n";
          return false;
        }
        if (values.empty() != true)
        {
          message->Parameters = values;
        }
        return true;
      }
      template <typename RESPONSE>
      bool FromMessage(RESPONSE& response, const Core::ProxyType<Core::JSONRPC::Message>& message, bool isResponseString=false) const
      {
        return FromMessage((Core::JSON::IElement*)(&response), message, isResponseString);
      }
      bool FromMessage(Core::JSON::IElement* response, const Core::ProxyType<Core::JSONRPC::Message>& message, bool isResponseString=false) const
      {
        Core::OptionalType<Core::JSON::Error> error;
        if ( !isResponseString && !response->FromString(message->Result.Value(), error) )
        {
          std::cout << "Failed to parse response!!! Error: '" <<  error.Value().Message() << "'\n";
          return false;
        }
        return true;
      }

    public:
      JSONRPCDirectLink(PluginHost::IShell* service, std::string callsign)
        : mCallSign(callsign)
      {
        if (service)
#if ((THUNDER_VERSION >= 4) && (THUNDER_VERSION_MINOR == 4))
          dispatcher_ = service->QueryInterfaceByCallsign<PluginHost::ILocalDispatcher>(mCallSign);
#else
          dispatcher_ = service->QueryInterfaceByCallsign<PluginHost::IDispatcher>(mCallSign);
#endif
      }
  
      JSONRPCDirectLink(PluginHost::IShell* service)
        : JSONRPCDirectLink(service, "Controller")
      {
      }
      ~JSONRPCDirectLink()
      {
        if (dispatcher_)
          dispatcher_->Release();
      }

      template <typename PARAMETERS, typename RESPONSE>
      uint32_t Invoke(const uint32_t waitTime, const string& method, const PARAMETERS& parameters, RESPONSE& response, bool isResponseString=false)
      {
        if (dispatcher_ == nullptr) {
          std::cout << "No JSON RPC dispatcher for " << mCallSign << '\n';
          return Core::ERROR_GENERAL;
        }

        auto message = Message();

        message->JSONRPC = Core::JSONRPC::Message::DefaultVersion;
        message->Id = Core::JSON::DecUInt32(++mId);
        message->Designator = Core::JSON::String(mCallSign + ".1." + method);

        ToMessage(parameters, message);

        const uint32_t channelId = ~0;
#if ((THUNDER_VERSION >= 4) && (THUNDER_VERSION_MINOR == 4))
        string output = "";
        uint32_t result = Core::ERROR_BAD_REQUEST;

        if (dispatcher_  != nullptr) {
            PluginHost::ILocalDispatcher* localDispatcher = dispatcher_->Local();

            ASSERT(localDispatcher != nullptr);

            if (localDispatcher != nullptr)
                result =  dispatcher_->Invoke(channelId, message->Id.Value(), sThunderSecurityToken, message->Designator.Value(), message->Parameters.Value(),output);
        }

        if (message.IsValid() == true) {
            if (result == static_cast<uint32_t>(~0)) {
                message.Release();
            }
            else if (result == Core::ERROR_NONE)
            {
                if (output.empty() == true)
                    message->Result.Null(true);
                else
                    message->Result = output;
            }
            else
            {
                message->Error.SetError(result);
                if (output.empty() == false) {
                    message->Error.Text = output;
                }
            }
        }

        if (!FromMessage(response, message, isResponseString))
        {
            return Core::ERROR_GENERAL;
        }
#elif (THUNDER_VERSION == 2)
        auto resp =  dispatcher_->Invoke(sThunderSecurityToken, channelId, *message);
#else
        Core::JSONRPC::Context context(channelId, message->Id.Value(), sThunderSecurityToken) ;
        auto resp = dispatcher_->Invoke(context, *message);
#endif

#if ((THUNDER_VERSION == 2) || (THUNDER_VERSION >= 4) && (THUNDER_VERSION_MINOR == 2))

        if (resp->Error.IsSet()) {
          std::cout << "Call failed: " << message->Designator.Value() << " error: " <<  resp->Error.Text.Value() << "\n";
          return resp->Error.Code;
        }

        if (!FromMessage(response, resp, isResponseString))
          return Core::ERROR_GENERAL;
#endif
        return Core::ERROR_NONE;
      }
    };

    /* virtual */ Core::hresult ResourceManagerImplementation::ReserveTTSResource(const string& appId, bool& success) /* override */
    {
        LOGINFO("ReserveTTSResource: appId=%s", appId.c_str());
        
        Core::hresult result = Core::ERROR_GENERAL;
        success = false;

        _adminLock.Lock();

        try {
            // Check if ReserveTTS is disabled by RFC
            if (false == mDisableReserveTTS) {
                
                std::cout << "appid: " << appId << std::endl;
                
                // Prepare parameters for TTS setACL call
                JsonObject params;
                JsonObject ttsResult;
                JsonObject clientParam;
                JsonArray clientList;
                JsonArray accessList;
                
                // Build the access list structure for TTS
                clientList.Add(appId);
                clientParam.Set("method", "speak");
                clientParam["apps"] = clientList;
                accessList.Add(clientParam);
                params["accesslist"] = accessList;
                
                std::string jsonstr;
                params.ToString(jsonstr);
                std::cout << "Resourcemanager: about to call setACL: " << jsonstr << std::endl;
                
                // Call TTS service using JSONRPCDirectLink - exact same as original implementation
                uint32_t ret = Core::ERROR_GENERAL;
                
                if (_service != nullptr) {
                    ret = JSONRPCDirectLink(_service, "org.rdk.TextToSpeech").Invoke<JsonObject, JsonObject>(20000, "setACL", params, ttsResult);
                } else {
                    LOGERR("Service interface not available for TTS call");
                }
                
                bool status = ((Core::ERROR_NONE == ret) && 
                              (ttsResult.HasLabel("success")) && 
                              (ttsResult["success"].Boolean()));
                
                ttsResult.ToString(jsonstr);
                std::cout << "setACL response: " << jsonstr << std::endl;
                std::cout << "setACL status: " << std::boolalpha << status << std::endl;
                
                if (status) {
                    result = Core::ERROR_NONE;
                    success = true;
                    LOGINFO("Successfully reserved TTS resource for: %s", appId.c_str());
                } else {
                    result = Core::ERROR_GENERAL;
                    LOGERR("Failed to reserve TTS resource for: %s", appId.c_str());
                }
                
            } else {
                LOGWARN("ReserveTTS RFC is disabled");
                result = Core::ERROR_NONE; // Original code returns success when disabled
            }
            
        } catch (const std::exception& e) {
            LOGERR("Exception in ReserveTTSResource: %s", e.what());
            result = Core::ERROR_GENERAL;
        }

        _adminLock.Unlock();

        return result;
    }

    /* virtual */ Core::hresult ResourceManagerImplementation::ReserveTTSResourceForApps(IAppIdIterator* appids, bool& success) /* override */
    {
        LOGINFO("ReserveTTSResourceForApps");
        
        Core::hresult result = Core::ERROR_GENERAL;
        success = false;

        if (appids == nullptr) {
            LOGERR("AppIds iterator is null");
            return Core::ERROR_BAD_REQUEST;
        }

        _adminLock.Lock();

        try {
            // Check if ReserveTTS is disabled by RFC
            if (false == mDisableReserveTTS) {
                
                // Convert iterator to vector like in original wrapper
                std::vector<std::string> apps;
                string appId;
                while (appids->Next(appId) == true) {
                    apps.push_back(appId);
                }
                appids->Reset(0);
                
                // Debug output like original
                for (const auto& s : apps) {
                    std::cout << s << " ";
                }
                std::cout << std::endl;
                
                // Prepare parameters for TTS setACL call - exact same logic as original
                JsonObject params;
                JsonObject ttsResult;
                JsonArray accessList;
                JsonObject clientParam;
                JsonArray clientList;
                
                // Build client list from vector
                for (const auto& client : apps) {
                    clientList.Add(client);
                }
                
                clientParam.Set("method", "speak");
                clientParam["apps"] = clientList;
                accessList.Add(clientParam);
                params["accesslist"] = accessList;
                
                std::string jsonstr;
                params.ToString(jsonstr);
                std::cout << "Resourcemanager: about to call setACL: " << jsonstr << std::endl;
                
                // Call TTS service using JSONRPCDirectLink - exact same as original implementation
                uint32_t ret = Core::ERROR_GENERAL;
                
                if (_service != nullptr) {
                    ret = JSONRPCDirectLink(_service, "org.rdk.TextToSpeech").Invoke<JsonObject, JsonObject>(20000, "setACL", params, ttsResult);
                } else {
                    LOGERR("Service interface not available for TTS call");
                }
                
                bool status = ((Core::ERROR_NONE == ret) && 
                              (ttsResult.HasLabel("success")) && 
                              (ttsResult["success"].Boolean()));
                
                ttsResult.ToString(jsonstr);
                std::cout << "setACL response: " << jsonstr << std::endl;
                std::cout << "setACL status: " << std::boolalpha << status << std::endl;
                
                if (status) {
                    result = Core::ERROR_NONE;
                    success = true;
                    LOGINFO("Successfully reserved TTS resource for %zu apps", apps.size());
                } else {
                    result = Core::ERROR_GENERAL;
                    LOGERR("Failed to reserve TTS resource for multiple apps");
                }
                
            } else {
                LOGWARN("ReserveTTS RFC is disabled");
                result = Core::ERROR_NONE; // Original code returns success when disabled
                success = true; // Set success when disabled (as per original logic)
            }
            
        } catch (const std::exception& e) {
            LOGERR("Exception in ReserveTTSResourceForApps: %s", e.what());
            result = Core::ERROR_GENERAL;
        }

        _adminLock.Unlock();

        return result;
    }

    } // namespace Plugin
} // namespace WPEFramework
