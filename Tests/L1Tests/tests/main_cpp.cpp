/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
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
distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/


#include "UserSettingsImplementation.h"
#include <WPEFramework/interfaces/IUserSettings.h>
#include <WPEFramework/interfaces/Ids.h>
#include <gtest/gtest.h>

WPEFramework::Exchange::IUserSettings *InterfacePointer = nullptr;
WPEFramework::Exchange::IUserSettingsInspector *IUserSettingsInspectorPointer = nullptr;

int main(int argc, char **argv) {
    using namespace WPEFramework::Plugin;
    // Instantiate UserSettingsImplementation and assign to global pointers
    UserSettingsImplementation* userSettingsImpl = new UserSettingsImplementation();
    InterfacePointer = userSettingsImpl;
    IUserSettingsInspectorPointer = userSettingsImpl;

    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();
    delete userSettingsImpl;
    return result;
}
