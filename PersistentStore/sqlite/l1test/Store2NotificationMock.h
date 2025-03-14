/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
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
**/

#pragma once

#include <gmock/gmock.h>
#include <interfaces/IStore2.h>

class Store2NotificationMock : public WPEFramework::Exchange::IStore2::INotification {
public:
    ~Store2NotificationMock() override = default;
    MOCK_METHOD(void, ValueChanged, (const WPEFramework::Exchange::IStore2::ScopeType scope, const string& ns, const string& key, const string& value), (override));
    BEGIN_INTERFACE_MAP(Store2NotificationMock)
    INTERFACE_ENTRY(INotification)
    END_INTERFACE_MAP
};
