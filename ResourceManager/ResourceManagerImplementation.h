/**
 * If not stated otherwise in this file or this component's LICENSE
 * file the following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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
#include <interfaces/IResourceManager.h>

#ifdef ENABLE_ERM
#include <essos-resmgr.h>
#endif

namespace WPEFramework {
namespace Plugin {

    class ResourceManagerImplementation : public Exchange::IResourceManager
    {
    public:
        ResourceManagerImplementation(const ResourceManagerImplementation&) = delete;
        ResourceManagerImplementation& operator=(const ResourceManagerImplementation&) = delete;

        ResourceManagerImplementation();
        ~ResourceManagerImplementation() override;

        BEGIN_INTERFACE_MAP(ResourceManagerImplementation)
            INTERFACE_ENTRY(Exchange::IResourceManager)
        END_INTERFACE_MAP

        // IResourceManager interface implementation
        Core::hresult SetAVBlocked(const string& appId, const bool blocked, Success& result) override;
        Core::hresult GetBlockedAVApplications(IStringIterator*& clients, bool& success) const override;
        Core::hresult ReserveTTSResource(const string& appId, Success& result) override;
        Core::hresult ReserveTTSResourceForApps(IStringIterator* const appids, Success& result) override;

    public:
        static ResourceManagerImplementation* _instance;

    private:
        mutable Core::CriticalSection _adminLock;
        PluginHost::IShell* _service;

#if defined(ENABLE_ERM) || defined(ENABLE_L1TEST)
        EssRMgr* mEssRMgr;
#endif
        bool mDisableBlacklist;
        bool mDisableReserveTTS;
        mutable std::map<std::string, bool> mAppsAVBlacklistStatus;
    };

} // namespace Plugin
} // namespace WPEFramework
