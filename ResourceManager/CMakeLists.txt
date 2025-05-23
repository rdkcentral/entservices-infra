# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2020 RDK Management
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

set(PLUGIN_NAME ResourceManager)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})
set(PLUGIN_RESOURCE_MANAGER_AUTOSTART true CACHE STRING "Automatically start ResourceManager plugin")
set(PLUGIN_RESOURCE_MANAGER_EXTRA_LIBRARIES "")
set(PLUGIN_RESOURCE_MANAGER_STARTUPORDER "" CACHE STRING "Automatically start ResourceManager plugin")

find_package(${NAMESPACE}Plugins REQUIRED)

add_library(${MODULE_NAME} SHARED
        ResourceManager.cpp
        Module.cpp
)

set_target_properties(${MODULE_NAME} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES)

target_compile_definitions(${MODULE_NAME} PRIVATE MODULE_NAME=Plugin_${PLUGIN_NAME})

target_include_directories(${MODULE_NAME} PRIVATE ../helpers )
set_source_files_properties(ResourceManager.cpp PROPERTIES COMPILE_FLAGS "-fexceptions")

set(RESOURCE_MANAGER_INCLUDES $ENV{RESOURCE_MANAGER_INCLUDES})
separate_arguments(RESOURCE_MANAGER_INCLUDES)
include_directories(BEFORE ${RESOURCE_MANAGER_INCLUDES})

if (RDK_SERVICES_L1_TEST)
    include_directories(${CMAKE_SOURCE_DIR}/Tests/L1Tests)
    add_definitions(-DENABLE_L1TEST)
endif(RDK_SERVICES_L1_TEST)

if (NOT RDK_SERVICES_L1_TEST)
        target_link_libraries(${MODULE_NAME} PRIVATE ${NAMESPACE}Plugins::${NAMESPACE}Plugins -lessosrmgr ${PLUGIN_RESOURCE_MANAGER_EXTRA_LIBRARIES})
else(RDK_SERVICES_L1_TEST)
        target_link_libraries(${MODULE_NAME} PRIVATE ${NAMESPACE}Plugins::${NAMESPACE}Plugins ${PLUGIN_RESOURCE_MANAGER_EXTRA_LIBRARIES})
endif(RDK_SERVICES_L1_TEST)

install(TARGETS ${MODULE_NAME}
        DESTINATION lib/${STORAGE_DIRECTORY}/plugins)

write_config(${PLUGIN_NAME})
