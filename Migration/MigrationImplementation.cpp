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

#include "MigrationImplementation.h"

#include <fstream>
#include <core/core.h>
#include <interfaces/entservices_errorcodes.h>
#include "UtilsLogging.h"
#include "UtilsgetFileContent.h"
#include "rfcapi.h"

#define MIGRATIONSTATUS "/opt/secure/persistent/MigrationStatus"
#define TR181_MIGRATIONSTATUS "Device.DeviceInfo.Migration.MigrationStatus"

namespace WPEFramework
{
    namespace Plugin
    {
        SERVICE_REGISTRATION(MigrationImplementation, 1, 0);
    
        MigrationImplementation::MigrationImplementation()
        {
            LOGINFO("MigrationImplementation Constructor called");
        }

        MigrationImplementation::~MigrationImplementation()
        {
            LOGINFO("MigrationImplementation Destructor called");
        }

        Core::hresult MigrationImplementation::GetBootTypeInfo(string& bootType)
        {
            bool status = false;
            const char* filename = "/tmp/bootType";
            string propertyName = "BOOT_TYPE";

            if (Utils::readPropertyFromFile(filename, propertyName, bootType))
            {
                LOGINFO("Boot type changed to: %s, current OS Class: rdke\n", bootType.c_str());
                status = true;
            }
            else
            {
                LOGERR("BootType is not present");
            }
	    return (status ? static_cast<uint32_t>(WPEFramework::Core::ERROR_NONE) : static_cast<uint32_t>(ERROR_FILE_IO));
        }

        Core::hresult MigrationImplementation::SetMigrationStatus(const string& status, bool& success)
        {
            std::unordered_set<std::string> Status_Set = {"NOT_STARTED","NOT_NEEDED","STARTED","PRIORITY_SETTINGS_MIGRATED","DEVICE_SETTINGS_MIGRATED","CLOUD_SETTINGS_MIGRATED","APP_DATA_MIGRATED","MIGRATION_COMPLETED"};

	    if(Status_Set.find(status) != Status_Set.end())
            {
                // if file exists, it will be truncated, otherwise it will be created
                std::ofstream file(MIGRATIONSTATUS, std::ios::trunc);
                if (file.is_open()) {
                    // Write the string status to the file
                    file << status;
                    LOGINFO("Current ENTOS Migration Status is %s\n", status.c_str());
                } else {
                    LOGERR("Failed to open or create file %s\n", MIGRATIONSTATUS);
		    return (ERROR_FILE_IO);
                }
                // Close the file
                file.close();
            }
            else {
		LOGERR("Invalid Migration Status\n");
		return (WPEFramework::Core::ERROR_INVALID_PARAMETER);
            }

            success = true;
            return WPEFramework::Core::ERROR_NONE;
        }

        Core::hresult MigrationImplementation::GetMigrationStatus(string& migrationStatus)
        {
            bool status = false;
            RFC_ParamData_t param = {0};
            WDMP_STATUS wdmpstatus = getRFCParameter((char*)"thunderapi", TR181_MIGRATIONSTATUS, &param);
            if (WDMP_SUCCESS == wdmpstatus) {
                migrationStatus = param.value;
                LOGINFO("Current ENTOS Migration Status is: %s\n", migrationStatus.c_str());
                status = true;
            }
            else {
                LOGINFO("Failed to get RFC parameter for Migration Status \n");
            }
            return (status ?  static_cast<uint32_t>(WPEFramework::Core::ERROR_NONE) :  static_cast<uint32_t>(ERROR_FILE_IO));
        }
    
    } // namespace Plugin
} // namespace WPEFramework
