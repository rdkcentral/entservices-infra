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
#include <interfaces/Ids.h>
#include <interfaces/IResourceManager.h>
#include <interfaces/IConfiguration.h>
#include <interfaces/json/JResourceManager.h>

#if defined(ENABLE_ERM) || defined(ENABLE_L1TEST)
#include <map>
#include "essos-resmgr.h"
#endif

#include <com/com.h>
#include <core/core.h>

using namespace WPEFramework;

namespace WPEFramework {
namespace Plugin {
    class ResourceManagerImplementation : public Exchange::IResourceManager, public Exchange::IConfiguration
    {
    public:
        // We do not allow this plugin to be copied !!
        ResourceManagerImplementation();
        ~ResourceManagerImplementation() override;

        ResourceManagerImplementation(const ResourceManagerImplementation&) = delete;
        ResourceManagerImplementation& operator=(const ResourceManagerImplementation&) = delete;

        BEGIN_INTERFACE_MAP(ResourceManagerImplementation)
        INTERFACE_ENTRY(Exchange::IResourceManager)
        INTERFACE_ENTRY(Exchange::IConfiguration)
        END_INTERFACE_MAP

        // IResourceManager interface methods
        Core::hresult SetAVBlocked(const string& appId, const bool blocked) override;
        Core::hresult GetBlockedAVApplications(IClientIterator*& clients) const override;
        Core::hresult ReserveTTSResource(const string& appId) override;
        Core::hresult ReserveTTSResourceForApps(IAppIdIterator* appids) override;

        // IConfiguration interface
        uint32_t Configure(PluginHost::IShell* service) override;

    private:
        mutable Core::CriticalSection _adminLock;
        PluginHost::IShell* _service;

#if defined(ENABLE_ERM) || defined(ENABLE_L1TEST) 
        EssRMgr* mEssRMgr;
#endif
        bool mDisableBlacklist;
        bool mDisableReserveTTS;
        std::map<std::string, bool> mAppsAVBlacklistStatus;

        // Internal helper methods
        bool setAVBlocked(const string& appId, const bool blocked);
        bool getBlockedAVApplications(std::vector<std::string> &appsList) const;
        bool reserveTTSResource(const string& appId);
        bool reserveTTSResourceForApps(const std::vector<std::string>& clients);

    public:
        static ResourceManagerImplementation* _instance;
    };
} // namespace Plugin
} // namespace WPEFramework
