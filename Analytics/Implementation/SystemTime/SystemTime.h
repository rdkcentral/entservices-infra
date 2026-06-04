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

#pragma once

#include <thread>
#include <mutex>
#include <memory>
#include <map>
#include <condition_variable>
#include <queue>

#include "../../Module.h"
#include "ISystemTime.h"
#include <interfaces/ISystemServices.h>

namespace WPEFramework
{
    namespace Plugin
    {
        class SystemTime: public ISystemTime
        {
        public:

            SystemTime(PluginHost::IShell *shell);
            ~SystemTime();

            bool IsSystemTimeAvailable() override;
            TimeZoneAccuracy GetTimeZoneOffset(int32_t &offsetSec) override;

        private:
            class SystemServicesNotification : public Exchange::ISystemServices::INotification
            {
            private:
                SystemServicesNotification(const SystemServicesNotification&) = delete;
                SystemServicesNotification& operator=(const SystemServicesNotification&) = delete;

            public:
                explicit SystemServicesNotification(SystemTime& parent)
                    : _parent(parent)
                {
                }
                ~SystemServicesNotification() override = default;

            public:
                void OnTimeStatusChanged(const string& TimeQuality, const string& TimeSrc, const string& Time) override
                {
                    _parent.onTimeStatusChanged(TimeQuality, TimeSrc, Time);
                }

                void OnTimeZoneDSTChanged(const string& oldTimeZone, const string& newTimeZone, const string& oldAccuracy, const string& newAccuracy) override
                {
                    _parent.onTimeZoneDSTChanged(oldTimeZone, newTimeZone, oldAccuracy, newAccuracy);
                }

                BEGIN_INTERFACE_MAP(SystemServicesNotification)
                INTERFACE_ENTRY(Exchange::ISystemServices::INotification)
                END_INTERFACE_MAP

            private:
                SystemTime& _parent;
            };

            enum EventType
            {
                EVENT_UNDEF,
                EVENT_INITIALISE,
                EVENT_TIME_STATUS_CHANGED,
                EVENT_TIME_ZONE_CHANGED,
                EVENT_SHUTDOWN
            };

            struct Event
            {
                EventType type;
                std::string payload;
            };

            void onTimeStatusChanged(const string& TimeQuality, const string& TimeSrc, const string& Time);
            void onTimeZoneDSTChanged(const string& oldTimeZone, const string& newTimeZone, const string& oldAccuracy, const string& newAccuracy);

            void InitializeSystemServices();
            void UpdateTimeStatus();
            void UpdateTimeZone();
            std::pair<TimeZoneAccuracy, int32_t> ParseTimeZone();
            void PopulateTimeZoneTransitionMap();
            void EventLoop();


            std::mutex mQueueLock;
            std::condition_variable mQueueCondition;
            std::queue<Event> mQueue;

            std::thread mEventThread;
            std::mutex mLock;

            Core::Sink<SystemServicesNotification> _systemServicesNotification;
            bool _registeredSystemEventHandlers;

            std::string mTimeQuality;
            std::string mTimeZone;
            std::string mTimeZoneAccuracyString;
            std::map<time_t, int32_t> mTransitionMap;
            bool mIsSystemTimeAvailable;
            PluginHost::IShell *mShell;
        };

        typedef std::shared_ptr<SystemTime> SystemTimePtr;
    }
}
