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

#pragma once
#include <interfaces/json/JPackageDownloader.h>
#include <core/JSON.h>

namespace WPEFramework
{
    namespace Plugin
    {
        class PackageInfoJson : public Core::JSON::Container
        {
        public:
            PackageInfoJson()
                : Core::JSON::Container()
            {
                _Init();
            }

            PackageInfoJson(const PackageInfoJson& _other)
                : Core::JSON::Container()
                , DownloadId(_other.DownloadId)
                , FileLocator(_other.FileLocator)
                , Reason(_other.Reason)
            {
                _Init();
            }

            PackageInfoJson& operator=(const PackageInfoJson& _rhs)
            {
                DownloadId = _rhs.DownloadId;
                FileLocator = _rhs.FileLocator;
                Reason = _rhs.Reason;
                return (*this);
            }

            PackageInfoJson(const Exchange::IPackageDownloader::PackageInfo& _other)
                : Core::JSON::Container()
            {
                DownloadId = _other.downloadId;
                FileLocator = _other.fileLocator;
                Reason = _other.reason;
                _Init();
            }

            PackageInfoJson &operator=(const Exchange::IPackageDownloader::PackageInfo &_rhs)
            {
                DownloadId = _rhs.downloadId;
                FileLocator = _rhs.fileLocator;
                Reason = _rhs.reason;
                return (*this);
            }

            operator WPEFramework::Exchange::IPackageDownloader::PackageInfo() const
            {
                Exchange::IPackageDownloader::PackageInfo _value{};
                _value.downloadId = DownloadId;
                _value.fileLocator = FileLocator;
                _value.reason = Reason;
                return (_value);
            }

        private:
            void _Init()
            {
                Add(_T("downloadId"), &DownloadId);
                Add(_T("fileLocator"), &FileLocator);
                Add(_T("reason"), &Reason);
            }

        public:
            Core::JSON::String DownloadId;
            Core::JSON::String FileLocator;                                    // Download ID
            Core::JSON::EnumType<Exchange::IPackageDownloader::Reason> Reason; // File Locator
        }; // class PackageInfoJson

    } // namespace Plugin

    ENUM_CONVERSION_BEGIN(Exchange::IPackageDownloader::Reason) // defined in <core/Enumerate.h>
        { Exchange::IPackageDownloader::Reason::NONE,                   _T("NONE"), 4 },
        { Exchange::IPackageDownloader::Reason::DOWNLOAD_FAILURE,       _T("DOWNLOAD_FAILURE"), 16 },
        { Exchange::IPackageDownloader::Reason::DISK_PERSISTENCE_FAILURE,_T("DISK_PERSISTENCE_FAILURE"), 25 },
    ENUM_CONVERSION_END(Exchange::IPackageDownloader::Reason)
} // namespace WPEFramework