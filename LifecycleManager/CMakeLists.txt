# If not stated otherwise in this file or this component's license file the
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

set(PLUGIN_NAME LifecycleManager)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})

set(PLUGIN_LIFECYCLE_MANAGER_MODE "Local" CACHE STRING "Controls if the plugin should run in its own process, in process or remote")
set(PLUGIN_LIFECYCLE_MANAGER_AUTOSTART false CACHE STRING "Automatically start LifecycleManager plugin")
set(PLUGIN_LIFECYCLE_MANAGER_EXTRA_LIBRARIES "")
set(PLUGIN_LIFECYCLE_MANAGER_STARTUPORDER "" CACHE STRING "Automatically start LifecycleManager plugin")

add_definitions(-DLIFECYCLE_MANAGER_API_VERSION_NUMBER_MAJOR=1)
add_definitions(-DLIFECYCLE_MANAGER_API_VERSION_NUMBER_MINOR=0)
add_definitions(-DLIFECYCLE_MANAGER_API_VERSION_NUMBER_PATCH=0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(${NAMESPACE}Plugins REQUIRED)
find_package(${NAMESPACE}Definitions REQUIRED)
find_package(CompileSettingsDebug CONFIG REQUIRED)

add_library(${MODULE_NAME} SHARED
        LifecycleManager.cpp
	LifecycleManagerImplementation.cpp
        Module.cpp
        ApplicationContext.cpp
        RequestHandler.cpp
        RippleHandler.cpp
        RuntimeManagerHandler.cpp
        WindowManagerHandler.cpp
        State.cpp
	StateHandler.cpp
        StateTransitionHandler.cpp
        WebSocket.cpp
)

set_target_properties(${MODULE_NAME} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES)

set_source_files_properties(LifecycleManager.cpp PROPERTIES COMPILE_FLAGS "-fexceptions")

target_link_libraries(${MODULE_NAME}
    PRIVATE
        CompileSettingsDebug::CompileSettingsDebug
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${NAMESPACE}Definitions::${NAMESPACE}Definitions)

install(TARGETS ${MODULE_NAME}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${STORAGE_DIRECTORY}/plugins)

include_directories(../helpers)

set(LIFECYCLE_MANAGER_INCLUDES $ENV{LIFECYCLE_MANAGER_INCLUDES})
separate_arguments(LIFECYCLE_MANAGER_INCLUDES)
include_directories(BEFORE ${LIFECYCLE_MANAGER_INCLUDES})

write_config(${PLUGIN_NAME})
