set(PLUGIN_NAME AppcLifecycleManager)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})

set(PLUGIN_APPC_LIFECYCLE_MANAGER_MODE "Local" CACHE STRING "Controls if the plugin should run in its own process, in process or remote")
set(PLUGIN_APPC_LIFECYCLE_MANAGER_AUTOSTART "true" CACHE STRING "Automatically start AppcLifecycleManager plugin")
set(PLUGIN_APPC_LIFECYCLE_MANAGER_STARTUPORDER "" CACHE STRING "Automatically start AppcLifecycleManager plugin")


list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(${NAMESPACE}Plugins REQUIRED)
find_package(${NAMESPACE}Definitions REQUIRED)
find_package(CompileSettingsDebug CONFIG REQUIRED)

add_library(${MODULE_NAME} SHARED
	AppcLifecycleManager.cpp
    Module.cpp
)

set_target_properties(${MODULE_NAME} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES)

set_source_files_properties(AppcLifecycleManager.cpp PROPERTIES COMPILE_FLAGS "-fexceptions")

target_link_libraries(${MODULE_NAME}
    PRIVATE
        CompileSettingsDebug::CompileSettingsDebug
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${NAMESPACE}Definitions::${NAMESPACE}Definitions)

install(TARGETS ${MODULE_NAME}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${STORAGE_DIRECTORY}/plugins)


include_directories(../helpers)


set(APPC_LIFECYCLEMANAGER_INCLUDES $ENV{APPC_LIFECYCLEMANAGER_INCLUDES})
separate_arguments(APPC_LIFECYCLEMANAGER_INCLUDES)
include_directories(BEFORE ${APPC_LIFECYCLEMANAGER_INCLUDES})

write_config(${PLUGIN_NAME})

