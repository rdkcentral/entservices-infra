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
#include "FbSettingsImplementation.h"
#include "UtilsLogging.h"
#include "delegate/SettingsDelegate.h"
#include "delegate/SystemDelegate.h"

namespace WPEFramework
{
    namespace Plugin
    {

        SERVICE_REGISTRATION(FbSettingsImplementation, 1, 0);

        FbSettingsImplementation::FbSettingsImplementation() : 
        mShell(nullptr)   
        {
            mDelegate = std::make_shared<SettingsDelegate>();
            mSystemDelegate = std::make_shared<SystemDelegate>();
        }

        FbSettingsImplementation::~FbSettingsImplementation()
        {
            // Cleanup resources if needed
            if (mShell != nullptr)
            {
                mShell->Release();
                mShell = nullptr;
            }
        }

        Core::hresult FbSettingsImplementation::HandleAppEventNotifier(const string& event /* @in */,
                                    const bool& listen /* @in */,
                                    bool &status /* @out */) {
            LOGINFO("HandleFireboltNotifier [event=%s listen=%s]",
                    event.c_str(), listen ? "true" : "false");
            status = true;
            Core::IWorkerPool::Instance().Submit(EventRegistrationJob::Create(this, event, listen));
            return Core::ERROR_NONE;
        }

        Core::hresult FbSettingsImplementation::SetName(const string& value  /* @in */, string& result) {
            result = "null"; // TBA
            return Core::ERROR_NONE;
        }

        Core::hresult FbSettingsImplementation::AddAdditionalInfo(const string& value  /* @in @opaque */, string& result) {
            result = "null"; // TBA
            return Core::ERROR_NONE;
        }
        // Delegated alias methods

        Core::hresult FbSettingsImplementation::GetDeviceMake(string& make) {
            LOGINFO("GetDeviceMake FbSettings");
            if (!mSystemDelegate) return Core::ERROR_UNAVAILABLE;
            return mSystemDelegate->GetDeviceMake(make);
        }

        Core::hresult FbSettingsImplementation::GetDeviceName(string& name) {
            if (!mSystemDelegate) return Core::ERROR_UNAVAILABLE;
            return mSystemDelegate->GetDeviceName(name);
        }

        Core::hresult FbSettingsImplementation::SetDeviceName(const string name) {
            if (!mSystemDelegate) return Core::ERROR_UNAVAILABLE;
            return mSystemDelegate->SetDeviceName(name);
        }

        Core::hresult FbSettingsImplementation::GetDeviceSku(string& sku) {
            if (!mSystemDelegate) return Core::ERROR_UNAVAILABLE;
            return mSystemDelegate->GetDeviceSku(sku);
        }

        Core::hresult FbSettingsImplementation::GetCountryCode(string& countryCode) {
            if (!mSystemDelegate) return Core::ERROR_UNAVAILABLE;
            return mSystemDelegate->GetCountryCode(countryCode);
        }

        Core::hresult FbSettingsImplementation::SetCountryCode(const string countryCode) {
            if (!mSystemDelegate) return Core::ERROR_UNAVAILABLE;
            return mSystemDelegate->SetCountryCode(countryCode);
        }

        Core::hresult FbSettingsImplementation::GetTimeZone(string& timeZone) {
            if (!mSystemDelegate) return Core::ERROR_UNAVAILABLE;
            return mSystemDelegate->GetTimeZone(timeZone);
        }

        Core::hresult FbSettingsImplementation::SetTimeZone(const string timeZone) {
            if (!mSystemDelegate) return Core::ERROR_UNAVAILABLE;
            return mSystemDelegate->SetTimeZone(timeZone);
        }

        Core::hresult FbSettingsImplementation::GetSecondScreenFriendlyName(string& name) {
            if (!mSystemDelegate) return Core::ERROR_UNAVAILABLE;
            return mSystemDelegate->GetSecondScreenFriendlyName(name);
        }

        uint32_t FbSettingsImplementation::Configure(PluginHost::IShell *shell)
        {
            LOGINFO("Configuring FbSettings");
            uint32_t result = Core::ERROR_NONE;
            ASSERT(shell != nullptr);
            mShell = shell;
            mShell->AddRef();
            mDelegate->setShell(mShell);
            if (mSystemDelegate) {
                mSystemDelegate->setShell(mShell);
            }
            return result;
        }
    }
}
