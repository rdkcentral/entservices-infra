/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2020 RDK Management
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

#ifndef __CONTEXTUTILS_H__
#define __CONTEXTUTILS_H__
#include <interfaces/IAppGateway.h>
#include <interfaces/IAppNotifications.h>
#include <interfaces/IApp2AppProvider.h>

#include "UtilsCallsign.h"
using namespace WPEFramework;
using namespace std;
class ContextUtils {
    public:
        // Implement a static method which accepts a Exchange::IAppNotifications::Context object and
        // converts it into Exchange::IAppGateway::Context object
        static Exchange::Context ConvertNotificationToAppGatewayContext(const Exchange::IAppNotifications::Context& notificationsContext){
            Exchange::Context appGatewayContext;
            // Perform the conversion logic here
            appGatewayContext.requestId = notificationsContext.requestId;
            appGatewayContext.connectionId = notificationsContext.connectionId;
            appGatewayContext.appId = notificationsContext.appId;
            return appGatewayContext;
        }

        // Implement a static method which accepts a Exchange::IAppGateway::Context object and
        // converts it into Exchange::IAppNotifications::Context object
        static Exchange::IAppNotifications::Context ConvertAppGatewayToNotificationContext(const Exchange::Context& appGatewayContext, const string& origin){
            Exchange::IAppNotifications::Context notificationsContext;
            // Perform the conversion logic here
            notificationsContext.requestId = appGatewayContext.requestId;
            notificationsContext.connectionId = appGatewayContext.connectionId;
            notificationsContext.appId = appGatewayContext.appId;
            notificationsContext.origin = origin;
            return notificationsContext;
        }

        // Implement a static method which accepts a Exchange::IAppGateway::Context object and
        // converts it into Exchange::IApp2AppProvider::Context object
        static Exchange::IApp2AppProvider::Context ConvertAppGatewayToProviderContext(const Exchange::Context& appGatewayContext, const string& origin){
            Exchange::IApp2AppProvider::Context providerContext;
            // Perform the conversion logic here
            providerContext.requestId = appGatewayContext.requestId;
            providerContext.connectionId = appGatewayContext.connectionId;
            providerContext.appId = appGatewayContext.appId;
            providerContext.origin = origin;
            return providerContext;
        }

        static Exchange::Context ConvertProviderToAppGatewayContext(const Exchange::IApp2AppProvider::Context& context){
            Exchange::Context appGatewayContext;
            // Perform the conversion logic here
            appGatewayContext.requestId = context.requestId;
            appGatewayContext.connectionId = context.connectionId;
            appGatewayContext.appId = context.appId;
            return appGatewayContext;
        }

        static Exchange::Context ConvertProviderToLaunchDelegateContext(const Exchange::IApp2AppProvider::Context& context){
            Exchange::Context launchDelegateContext;
            // Perform the conversion logic here
            launchDelegateContext.requestId = context.requestId;
            launchDelegateContext.connectionId = context.connectionId;
            launchDelegateContext.appId = context.appId;
            return launchDelegateContext;
        }

        static bool IsOriginGateway(const string& origin) {
            return origin == APP_GATEWAY_CALLSIGN;
        }
};
#endif
