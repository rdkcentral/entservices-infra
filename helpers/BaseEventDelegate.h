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
#ifndef __BASEEVENTDELEGATE_H__
#define __BASEEVENTDELEGATE_H__
#include "StringUtils.h"
#include <interfaces/IAppNotifications.h>
#include "UtilsLogging.h"
#include <set>
#include "UtilsCallsign.h"
#include <mutex>


using namespace WPEFramework;

class BaseEventDelegate {
    public:

        class EXTERNAL EventDelegateDispatchJob : public Core::IDispatch 
        {
            public:
                EventDelegateDispatchJob(BaseEventDelegate* delegate, const string& event, const string& payload)
                    : mDelegate(*delegate), mEvent(event), mPayload(payload) {}

                EventDelegateDispatchJob() = delete;
                EventDelegateDispatchJob(const EventDelegateDispatchJob &) = delete;
                EventDelegateDispatchJob &operator=(const EventDelegateDispatchJob &) = delete;
                ~EventDelegateDispatchJob()
                {
                }

                static Core::ProxyType<Core::IDispatch> Create(BaseEventDelegate *parent,
                const string& event, const string& payload)
                {
                    return (Core::ProxyType<Core::IDispatch>(Core::ProxyType<EventDelegateDispatchJob>::Create(parent, event, payload)));
                }
                
                virtual void Dispatch()
                {
                    mDelegate.DispatchToAppNotifications(mEvent, mPayload);
                }

            private:
                BaseEventDelegate &mDelegate;
                string mEvent;
                string mPayload;
        };

        BaseEventDelegate() : mRegisteredNotifications(),
        mRegisterMutex() {
        }

        ~BaseEventDelegate() {
            // Cleanup registered notifications
            for (auto& entry : mRegisteredNotifications) {
                entry.second->Release();
            }

            mRegisteredNotifications.clear();
        }


        virtual bool HandleEvent(Exchange::IAppNotificationHandler::IEmitter *cb, const string &event, const bool listen, bool &registrationError) { return false; };

        bool Dispatch(const string &event, const string &payload) {
            LOGDBG("Dispatching %s with payload %s", event.c_str(), payload.c_str());
            
            // Check if Notification is registered if not return
            if (!IsNotificationRegistered(event)) {
                LOGDBG("Notification %s is not registered", event.c_str());
                return false;
            }

            Core::IWorkerPool::Instance().Submit(EventDelegateDispatchJob::Create(this, event, payload));

            return true;
        }

        bool DispatchToAppNotifications(const string& event, const string& payload) {

            auto emitter = GetEmitterForNotification(event);
            if (emitter != nullptr) {
                LOGDBG("Using registered emitter for event %s", event.c_str());
                emitter->Emit(event, payload, "");
                return true;
            }
            
            LOGERR("No emitter found for event %s", event.c_str());
            return false;
        }

        // new method to register notifications
        // which accepts a string and adds it to the mRegisteredNotifications vector
        void AddNotification(const string& event, Exchange::IAppNotificationHandler::IEmitter *cb) {
            string event_l = StringUtils::toLower(event);
            std::lock_guard<std::mutex> lock(mRegisterMutex);
            auto it = mRegisteredNotifications.find(event_l);
            if (it != mRegisteredNotifications.end()) {
                it->second->Release();
            }
            mRegisteredNotifications[event_l] = cb;
            cb->AddRef();
            LOGDBG("Notification registered = %s", event_l.c_str());
        }

        // new method to check if a notification is registered 
        bool IsNotificationRegistered(const string& event) {
            string event_l = StringUtils::toLower(event);
            std::lock_guard<std::mutex> lock(mRegisterMutex);
            bool result = mRegisteredNotifications.find(event_l) != mRegisteredNotifications.end();
            LOGDBG("Finding notification = %s result=%s", event_l.c_str(), result? "true":"false");
            return result;
        }

        Exchange::IAppNotificationHandler::IEmitter * GetEmitterForNotification(const string& event) {
            string event_l = StringUtils::toLower(event);
            std::lock_guard<std::mutex> lock(mRegisterMutex);
            auto it = mRegisteredNotifications.find(event_l);
            if (it != mRegisteredNotifications.end()) {
                return it->second;
            }
            return nullptr;
        }

        // new method remove the notification
        void RemoveNotification(const string& event) {
            string event_l = StringUtils::toLower(event);
            std::lock_guard<std::mutex> lock(mRegisterMutex);
            auto it = mRegisteredNotifications.find(event_l);
            if (it != mRegisteredNotifications.end()) {
                it->second->Release();
                mRegisteredNotifications.erase(event_l);
            }
        }

    private:
        std::map<string, Exchange::IAppNotificationHandler::IEmitter*> mRegisteredNotifications;
        std::mutex mRegisterMutex;


};
#endif
