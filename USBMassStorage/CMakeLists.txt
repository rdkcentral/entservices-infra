# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2024 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set(PLUGIN_NAME USBMassStorage)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})
set(PLUGIN_IMPLEMENTATION ${MODULE_NAME}Implementation)

set(PLUGIN_USB_MASS_STORAGE_AUTOSTART "false" CACHE STRING "Automatically start USBMassStorage plugin")
set(PLUGIN_USB_MASS_STORAGE_STARTUPORDER "45" CACHE STRING "To configure startup order of USBMassStorage plugin")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(${NAMESPACE}Plugins REQUIRED)
find_package(${NAMESPACE}Definitions REQUIRED)
find_package(CompileSettingsDebug CONFIG REQUIRED)

if (NOT RDK_SERVICE_L2_TEST AND NOT RDK_SERVICES_L1_TEST)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
endif()

add_library(${MODULE_NAME} SHARED
    USBMassStorage.cpp
    Module.cpp)

set_target_properties(${MODULE_NAME} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES)

target_link_libraries(${MODULE_NAME}
    PRIVATE
        CompileSettingsDebug::CompileSettingsDebug
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${NAMESPACE}Definitions::${NAMESPACE}Definitions)

install(TARGETS ${MODULE_NAME}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${STORAGE_DIRECTORY}/plugins)

add_library(${PLUGIN_IMPLEMENTATION} SHARED
    USBMassStorageImplementation.cpp
    Module.cpp)

include_directories(../helpers)

if (NOT RDK_SERVICE_L2_TEST)
    include_directories(${LIBUSB_INCLUDE_DIRS})
endif()

set_target_properties(${PLUGIN_IMPLEMENTATION} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES)

if (RDK_SERVICE_L2_TEST)
   find_library(TESTMOCKLIB_LIBRARIES NAMES TestMocklib)
   if (TESTMOCKLIB_LIBRARIES)
       message ("linking mock libraries ${TESTMOCKLIB_LIBRARIES} library")
       target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE ${TESTMOCKLIB_LIBRARIES})
       target_link_options(${PLUGIN_IMPLEMENTATION} PRIVATE -Wl,--wrap,open -Wl,--wrap,mkdir -Wl,--wrap,mount -Wl,--wrap,umount -Wl,--wrap,ioctl -Wl,--wrap,statvfs -Wl,--wrap,statfs -Wl,--wrap,stat  -Wl,--wrap,rmdir)
   else (TESTMOCKLIB_LIBRARIES)
       message ("Require ${TESTMOCKLIB_LIBRARIES} library")
   endif (TESTMOCKLIB_LIBRARIES)
endif (RDK_SERVICES_L2_TEST)

if (RDK_SERVICES_L1_TEST)
    target_link_options(${PLUGIN_IMPLEMENTATION} PRIVATE -Wl,--wrap,open -Wl,--wrap,mkdir -Wl,--wrap,mount -Wl,--wrap,umount -Wl,--wrap,ioctl -Wl,--wrap,statvfs -Wl,--wrap,statfs -Wl,--wrap,stat  -Wl,--wrap,rmdir)
endif (RDK_SERVICES_L1_TEST)
target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE ${NAMESPACE}Plugins::${NAMESPACE}Plugins)

target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE CompileSettingsDebug::CompileSettingsDebug ${NAMESPACE}Plugins::${NAMESPACE}Plugins)

install(TARGETS ${PLUGIN_IMPLEMENTATION}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${STORAGE_DIRECTORY}/plugins)

write_config(${PLUGIN_NAME})

