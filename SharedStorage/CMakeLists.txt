# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2022 RDK Management
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

cmake_minimum_required(VERSION 3.14)

set(PLUGIN_NAME SharedStorage)
find_package(WPEFramework NAMES WPEFramework Thunder)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})

set(CMAKE_CXX_STANDARD 11)

set(PLUGIN_SHAREDSTORAGE_MODE "Off" CACHE STRING "Controls if the plugin should run in its own process, in process or remote")
set(PLUGIN_SHAREDSTORAGE_STARTUPORDER "51" CACHE STRING "To configure startup order of the plugin")

add_library(${MODULE_NAME} SHARED
        SharedStorage.cpp
        Module.cpp
)

include_directories(
        ../helpers)

find_package(${NAMESPACE}Plugins REQUIRED)
find_package(${NAMESPACE}Definitions REQUIRED)
target_link_libraries(${MODULE_NAME} PRIVATE
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${NAMESPACE}Definitions::${NAMESPACE}Definitions
)

install(TARGETS ${MODULE_NAME}
        DESTINATION lib/${STORAGE_DIRECTORY}/plugins)

set(PLUGIN_IMPLEMENTATION ${MODULE_NAME}Implementation)
add_library(${PLUGIN_IMPLEMENTATION} SHARED
        Module.cpp
        SharedStorageImplementation.cpp
)

if (RDK_SERVICE_L2_TEST)
   find_library(TESTMOCKLIB_LIBRARIES NAMES TestMocklib)
   if (TESTMOCKLIB_LIBRARIES)
       message ("linking mock libraries ${TESTMOCKLIB_LIBRARIES} library")
       target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE ${TESTMOCKLIB_LIBRARIES})
   else (TESTMOCKLIB_LIBRARIES)
       message ("Require ${TESTMOCKLIB_LIBRARIES} library")
   endif (TESTMOCKLIB_LIBRARIES)
endif (RDK_SERVICES_L2_TEST)

target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${NAMESPACE}Definitions::${NAMESPACE}Definitions
)

install(TARGETS ${PLUGIN_IMPLEMENTATION}
        DESTINATION lib/${STORAGE_DIRECTORY}/plugins)

write_config(${PLUGIN_NAME})
