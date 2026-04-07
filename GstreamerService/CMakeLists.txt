# ------------------------------------------------------------------------------
# Plugin metadata
# ------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.8)

if(NOT NAMESPACE)
    set(NAMESPACE "WPEFramework")
endif()

set(PLUGIN_NAME GstreamerService)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})
set(PLUGIN_IMPLEMENTATION ${MODULE_NAME}Implementation)

set(VERSION_MAJOR 1)
set(VERSION_MINOR 0)
set(VERSION_PATCH 0)

set(MODULE_VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})

add_compile_definitions(GSTREAMERSERVICE_MAJOR_VERSION=${VERSION_MAJOR})
add_compile_definitions(GSTREAMERSERVICE_MINOR_VERSION=${VERSION_MINOR})
add_compile_definitions(GSTREAMERSERVICE_PATCH_VERSION=${VERSION_PATCH})

message(STATUS "Building ${MODULE_NAME} v${MODULE_VERSION}")

# ------------------------------------------------------------------------------
# Build options
# ------------------------------------------------------------------------------

set(PLUGIN_GSTREAMERSERVICE_STARTUPORDER "" CACHE STRING "Startup order")
set(PLUGIN_GSTREAMERSERVICE_AUTOSTART "false" CACHE STRING "Autostart plugin")
set(PLUGIN_GSTREAMERSERVICE_MODE "Local" CACHE STRING "Controls if the plugin should run in its own process, in process or remote")

# ------------------------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------------------------

find_package(${NAMESPACE}Plugins     REQUIRED)
find_package(${NAMESPACE}Definitions REQUIRED)
find_package(CompileSettingsDebug CONFIG REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)

if(USE_THUNDER_R4)
    find_package(${NAMESPACE}COM       REQUIRED)
else()
    find_package(${NAMESPACE}Protocols REQUIRED)
endif()

# ------------------------------------------------------------------------------
# Plugin shared library (Thunder host side - stub)
# ------------------------------------------------------------------------------

add_library(${MODULE_NAME} SHARED
    GstreamerService.cpp
    Module.cpp
)

set_target_properties(${MODULE_NAME} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
)

target_compile_definitions(${MODULE_NAME} PRIVATE MODULE_NAME=Plugin_${PLUGIN_NAME})

target_include_directories(${MODULE_NAME} PRIVATE ../helpers)

target_link_libraries(${MODULE_NAME}
    PRIVATE
        CompileSettingsDebug::CompileSettingsDebug
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${NAMESPACE}Definitions::${NAMESPACE}Definitions
)

install(TARGETS ${MODULE_NAME}
    DESTINATION lib/${STORAGE_DIRECTORY}/plugins
)

# ------------------------------------------------------------------------------
# Implementation shared library (runs out-of-process)
# ------------------------------------------------------------------------------

add_library(${PLUGIN_IMPLEMENTATION} SHARED
    GstreamerServiceImplementation.cpp
    Module.cpp
)

set_target_properties(${PLUGIN_IMPLEMENTATION} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
)

target_compile_definitions(${PLUGIN_IMPLEMENTATION} PRIVATE MODULE_NAME=Plugin_${PLUGIN_NAME}Implementation)

target_include_directories(${PLUGIN_IMPLEMENTATION} PRIVATE
    ${GSTREAMER_INCLUDE_DIRS}
    ../helpers
)

target_link_libraries(${PLUGIN_IMPLEMENTATION}
    PRIVATE
        CompileSettingsDebug::CompileSettingsDebug
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${NAMESPACE}Definitions::${NAMESPACE}Definitions
        ${GSTREAMER_LIBRARIES}
)

target_link_directories(${PLUGIN_IMPLEMENTATION} PRIVATE
    ${GSTREAMER_LIBRARY_DIRS}
)

if(USE_THUNDER_R4)
    target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE ${NAMESPACE}COM::${NAMESPACE}COM)
else()
    target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE ${NAMESPACE}Protocols::${NAMESPACE}Protocols)
endif()

install(TARGETS ${PLUGIN_IMPLEMENTATION}
    DESTINATION lib/${STORAGE_DIRECTORY}/plugins
)

# ------------------------------------------------------------------------------
# Generate plugin configuration
# ------------------------------------------------------------------------------

write_config(${PLUGIN_NAME})
