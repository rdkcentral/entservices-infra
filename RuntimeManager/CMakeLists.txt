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

set(PLUGIN_NAME RuntimeManager)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})
set(PLUGIN_IMPLEMENTATION ${MODULE_NAME}Implementation)

set(PLUGIN_RUNTIME_MANAGER_MODE "Off" CACHE STRING "Controls if the plugin should run in its own process, in process or remote")
set(PLUGIN_RUNTIME_MANAGER_AUTOSTART false CACHE STRING "Automatically start RuntimeManager plugin")
set(PLUGIN_RUNTIME_MANAGER_EXTRA_LIBRARIES "")
set(PLUGIN_RUNTIME_MANAGER_STARTUPORDER "" CACHE STRING "Automatically start RuntimeManager plugin")

add_definitions(-DRUNTIME_MANAGER_API_VERSION_NUMBER_MAJOR=1)
add_definitions(-DRUNTIME_MANAGER_API_VERSION_NUMBER_MINOR=0)
add_definitions(-DRUNTIME_MANAGER_API_VERSION_NUMBER_PATCH=0)

set(RUNTIME_MANAGER_IMPLEMENTATION ${PLUGIN_NAME}Implementation)
add_definitions(-DPLUGIN_RUNTIME_MANAGER_IMPLEMENTATION_NAME="${RUNTIME_MANAGER_IMPLEMENTATION}")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(${NAMESPACE}Plugins REQUIRED)
find_package(${NAMESPACE}Definitions REQUIRED)
find_package(CompileSettingsDebug CONFIG REQUIRED)

add_library(${MODULE_NAME} SHARED
        RuntimeManager.cpp
        Module.cpp
)

set_target_properties(${MODULE_NAME} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES)

set_source_files_properties(RuntimeManager.cpp PROPERTIES COMPILE_FLAGS "-fexceptions")

target_link_libraries(${MODULE_NAME}
    PRIVATE
        CompileSettingsDebug::CompileSettingsDebug
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${NAMESPACE}Definitions::${NAMESPACE}Definitions)

install(TARGETS ${MODULE_NAME}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${STORAGE_DIRECTORY}/plugins)

add_library(${PLUGIN_IMPLEMENTATION} SHARED
    RuntimeManagerImplementation.cpp
    DobbySpecGenerator.cpp
    WindowManagerConnector.cpp
    DobbyEventListener.cpp
    UserIdManager.cpp
    AIConfiguration.cpp
    Module.cpp)

include_directories(../helpers)

set_target_properties(${PLUGIN_IMPLEMENTATION} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES)

set( AI_BUILD_TYPE "DEBUG" )
add_definitions( -DAI_DEBUG=1 -DAI_RELEASE=2 )
if( AI_BUILD_TYPE STREQUAL "RELEASE" )
    add_definitions( -DAI_BUILD_TYPE=AI_RELEASE )
elseif( AI_BUILD_TYPE STREQUAL "DEBUG" )
    add_definitions( -DAI_BUILD_TYPE=AI_DEBUG )
else()
    message( FATAL_ERROR "AI_BUILD_TYPE '${AI_BUILD_TYPE}' not supported" )
endif()

set(RUNTIME_MANAGER_INCLUDES $ENV{RUNTIME_MANAGER_INCLUDES})
separate_arguments(RUNTIME_MANAGER_INCLUDES)
include_directories(BEFORE ${RUNTIME_MANAGER_INCLUDES})

target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE
    CompileSettingsDebug::CompileSettingsDebug ${NAMESPACE}Plugins::${NAMESPACE}Plugins  ${PLUGIN_RUNTIME_MANAGER_EXTRA_LIBRARIES})

install(TARGETS ${PLUGIN_IMPLEMENTATION}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${STORAGE_DIRECTORY}/plugins)

write_config(${PLUGIN_NAME})
