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

#include "ResourceManagerImplementation.h"
#include "UtilsgetRFCConfig.h"
#include "UtilsJsonRpc.h"
#include <iostream>

using namespace WPEFramework;

namespace WPEFramework {
namespace Plugin {

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

          template <typename PARAMETERS>
          uint32_t Get(const uint32_t waitTime, const string& method, PARAMETERS& respObject)
          {
            JsonObject empty;
            return Invoke(waitTime, method, empty, respObject);
          }

          template <typename PARAMETERS>
          uint32_t Set(const uint32_t waitTime, const string& method, const PARAMETERS& sendObject)
          {
            JsonObject empty;
            return Invoke(waitTime, method, sendObject, empty);
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
            string sThunderSecurityToken = ""; // TODO: Get from service

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

    SERVICE_REGISTRATION(ResourceManagerImplementation, 1, 0);
    ResourceManagerImplementation* ResourceManagerImplementation::_instance = nullptr;

    ResourceManagerImplementation::ResourceManagerImplementation()
    : _adminLock()
    , _service(nullptr)
#if defined(ENABLE_ERM) || defined(ENABLE_L1TEST) 
    , mEssRMgr(nullptr)
#endif
    , mDisableBlacklist(true)
    , mDisableReserveTTS(true)
    {
        LOGINFO("Create ResourceManagerImplementation Instance");
        ResourceManagerImplementation::_instance = this;
        
#ifdef ENABLE_ERM
        mEssRMgr = EssRMgrCreate();
        std::cout<<"EssRMgrCreate "<<((mEssRMgr != nullptr)?"succeeded":"failed")<<std::endl;

        RFC_ParamData_t param;
        mDisableBlacklist = true;

        if (true == Utils::getRFCConfig("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Resourcemanager.Blacklist.Enable", param))
        {
            mDisableBlacklist = ((param.type == WDMP_BOOLEAN) && (strncasecmp(param.value, "false", 5) == 0));
        }
        
        if (true == Utils::getRFCConfig("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Resourcemanager.ReserveTTS.Enable", param))
        {
            mDisableReserveTTS = ((param.type == WDMP_BOOLEAN) && (strncasecmp(param.value, "false", 5) == 0));
        }
#else
        std::cout<<"ENABLE_ERM not defined"<<std::endl;
#endif
    }

    ResourceManagerImplementation::~ResourceManagerImplementation()
    {
        LOGINFO("Call ResourceManagerImplementation destructor");
        
        // Release service reference
        if (_service != nullptr) {
            _service->Release();
        }
        
#ifdef ENABLE_ERM
        if (mEssRMgr) {
            EssRMgrDestroy(mEssRMgr);
            mEssRMgr = nullptr;
        }
#endif

        ResourceManagerImplementation::_instance = nullptr;
        _service = nullptr;
    }

    uint32_t ResourceManagerImplementation::Configure(PluginHost::IShell* service)
    {
        LOGINFO("Configuring ResourceManagerImplementation");
        uint32_t result = Core::ERROR_NONE;
        ASSERT(service != nullptr);
        
        _adminLock.Lock();
        _service = service;
        _service->AddRef();
        
        // JSON-RPC registration is handled in main ResourceManager plugin
        
        _adminLock.Unlock();

        return result;
    }

    Core::hresult ResourceManagerImplementation::SetAVBlocked(const string& appId, const bool blocked)
    {
        LOGINFO("SetAVBlocked: appId=%s, blocked=%s", appId.c_str(), blocked ? "true" : "false");
        
        _adminLock.Lock();
        
        bool result = false;
        
        if ((nullptr != mEssRMgr) && (false == mDisableBlacklist))
        {
            if (!appId.empty()) 
            {
                std::cout<<"appId : "<< appId << std::endl;
                std::cout<<"blocked  : "<<std::boolalpha << blocked << std::endl;

                result = setAVBlocked(appId, blocked);
                std::cout<< "ResourceManagerImplementation : setAVBlocked returned : "<<std::boolalpha << result << std::endl;
            }
            else
            {
                std::cout<<"ResourceManagerImplementation : ERROR: appId is required and cannot be empty" << std::endl;
            }
        }
        else
        {
            std::cout << "ResourceManagerImplementation : " << (mDisableBlacklist ? "Blacklist RFC is disabled" : "ERM not enabled") << std::endl;
        }
        
        _adminLock.Unlock();
        
        return result ? Core::ERROR_NONE : Core::ERROR_GENERAL;
    }

    bool ResourceManagerImplementation::setAVBlocked(const string& appId, const bool blocked)
    {
        bool status = true;
        
#ifdef ENABLE_ERM
        if (mEssRMgr) {
            status = blocked ? EssRMgrAddToBlackList(mEssRMgr, appId.c_str()) 
                            : EssRMgrRemoveFromBlackList(mEssRMgr, appId.c_str());
            
            std::cout<<"setAVBlocked call returning "<<std::boolalpha << status << std::endl;
            
            if (status) {
                mAppsAVBlacklistStatus[appId] = blocked;
                std::cout<<"mAppsAVBlacklistStatus updated"<<std::endl;
            }
        } else {
            std::cout<<"EssRMgr is null"<<std::endl;
            status = false;
        }
#else
        std::cout<<"ENABLE_ERM not defined, simulating success"<<std::endl;
        // For testing without ERM, just update the map
        mAppsAVBlacklistStatus[appId] = blocked;
#endif
        
        return status;
    }

    Core::hresult ResourceManagerImplementation::GetBlockedAVApplications(IClientIterator*& clients) const
    {
        LOGINFO("GetBlockedAVApplications called");
        
        _adminLock.Lock();
        
        std::vector<std::string> appsList;
        bool result = false;
        
        if (nullptr != mEssRMgr)
        {
            result = getBlockedAVApplications(appsList);
            if (result)
            {
                std::cout << "Found " << appsList.size() << " blocked applications" << std::endl;
                
                // Create iterator for COM-RPC
                clients = (Core::Service<RPC::IteratorType<Exchange::IResourceManager::IClientIterator>>::Create<Exchange::IResourceManager::IClientIterator>(appsList));
                if (clients != nullptr) {
                    clients->AddRef();
                }
            }
        }
        else
        {
            std::cout << "ResourceManagerImplementation : ERM not enabled" << std::endl;
        }
        
        _adminLock.Unlock();
        
        return result ? Core::ERROR_NONE : Core::ERROR_GENERAL;
    }

    bool ResourceManagerImplementation::getBlockedAVApplications(std::vector<std::string> &appsList) const
    {
        bool status = false;
        
#ifdef ENABLE_ERM
        std::cout<<"iterating mAppsAVBlacklistStatus ..." << std::endl;
        std::map<std::string, bool>::const_iterator appsItr = mAppsAVBlacklistStatus.begin();
        for (; appsItr != mAppsAVBlacklistStatus.end(); appsItr++)
        {
            std::cout<<"app : " <<appsItr->first<< std::endl;
            std::cout<<"blocked  : "<<std::boolalpha << appsItr->second << std::endl;
            if (true == appsItr->second)
            {
                appsList.push_back(appsItr->first);
            }
        }
        status = true;
#else
        std::cout<<"ENABLE_ERM not defined, returning empty list"<<std::endl;
        status = true;  // Still return success, just empty list
#endif
        
        return status;
    }

    Core::hresult ResourceManagerImplementation::ReserveTTSResource(const string& appId)
    {
        LOGINFO("ReserveTTSResource: appId=%s", appId.c_str());
        
        _adminLock.Lock();
        
        bool result = false;
        
        if (!appId.empty() && (false == mDisableReserveTTS))
        {
            std::cout<<"appId : "<< appId << std::endl;
            
            result = reserveTTSResource(appId);
            std::cout<< "ResourceManagerImplementation : reserveTTSResource returned : "<<std::boolalpha << result << std::endl;
        }
        else
        {
            if (mDisableReserveTTS)
            {
                std::cout << "ResourceManagerImplementation : ReserveTTS RFC is disabled" << std::endl;
                result = true; // Return success when disabled
            }
            else
            {
                std::cout<<"ResourceManagerImplementation : ERROR: appId is required and cannot be empty" << std::endl;
            }
        }
        
        _adminLock.Unlock();
        
        return result ? Core::ERROR_NONE : Core::ERROR_GENERAL;
    }

    bool ResourceManagerImplementation::reserveTTSResource(const string& appId)
    {
        uint32_t ret = Core::ERROR_NONE;
        bool status = false;

        JsonObject params;
        JsonObject result;
        JsonObject clientParam;
        JsonArray clientList = JsonArray(); 
        JsonArray accessList = JsonArray();

        clientList.Add(appId);
        clientParam.Set("method", "speak");
        clientParam["apps"] = clientList;
        accessList.Add(clientParam);
        params["accesslist"] = accessList;

        std::string jsonstr;
        params.ToString(jsonstr);
        std::cout<<"ResourceManagerImplementation : about to call setACL : "<< jsonstr << std::endl;

        ret = JSONRPCDirectLink(_service, "org.rdk.TextToSpeech").Invoke<JsonObject, JsonObject>(20000, "setACL", params, result);

        status = ((Core::ERROR_NONE == ret) && (result.HasLabel("success")) && (result["success"].Boolean()));

        result.ToString(jsonstr);
        std::cout<<"setACL response : "<< jsonstr << std::endl;
        std::cout<<"setACL status  : "<<std::boolalpha << status << std::endl;
        
        return status;
    }

    Core::hresult ResourceManagerImplementation::ReserveTTSResourceForApps(IAppIdIterator* appids)
    {
        LOGINFO("ReserveTTSResourceForApps called");
        
        _adminLock.Lock();
        
        bool result = false;
        
        if ((appids != nullptr) && (false == mDisableReserveTTS))
        {
            // Convert COM-RPC iterator to vector
            std::vector<std::string> apps;
            string appId;
            
            while (appids->Next(appId) == true) {
                apps.push_back(appId);
                std::cout << "AppId from iterator: " << appId << std::endl;
            }
            
            if (!apps.empty()) {
                result = reserveTTSResourceForApps(apps);
                std::cout<< "ResourceManagerImplementation : reserveTTSResourceForApps returned : "<<std::boolalpha << result << std::endl;
            } else {
                std::cout<<"ResourceManagerImplementation : ERROR: appids iterator is empty" << std::endl;
            }
        }
        else
        {
            if (mDisableReserveTTS)
            {
                std::cout << "ResourceManagerImplementation : ReserveTTS RFC is disabled" << std::endl;
                result = true; // Return success when disabled
            }
            else
            {
                std::cout<<"ResourceManagerImplementation : ERROR: appids iterator is null" << std::endl;
            }
        }
        
        _adminLock.Unlock();
        
        return result ? Core::ERROR_NONE : Core::ERROR_GENERAL;
    }

    bool ResourceManagerImplementation::reserveTTSResourceForApps(const std::vector<std::string>& clients)
    {
        uint32_t ret = Core::ERROR_NONE;
        bool status = false;

        JsonObject params;
        JsonObject result;
        JsonArray accessList;
        JsonObject clientParam;
        JsonArray clientList;
        
        // Add all apps to the client list
        for (const auto& client : clients) {
             clientList.Add(client);
        }
        
        clientParam.Set("method", "speak");
        clientParam["apps"] = clientList;
        accessList.Add(clientParam);
        params["accesslist"] = accessList;

        std::string jsonstr;
        params.ToString(jsonstr);
        std::cout<<"ResourceManagerImplementation : about to call setACL for multiple apps : "<< jsonstr << std::endl;

        ret = JSONRPCDirectLink(_service, "org.rdk.TextToSpeech").Invoke<JsonObject, JsonObject>(20000, "setACL", params, result);

        status = ((Core::ERROR_NONE == ret) && (result.HasLabel("success")) && (result["success"].Boolean()));

        result.ToString(jsonstr);
        std::cout<<"setACL response : "<< jsonstr << std::endl;
        std::cout<<"setACL status  : "<<std::boolalpha << status << std::endl;

        return status;
    }

} // namespace Plugin
} // namespace WPEFramework
