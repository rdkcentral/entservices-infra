# Gateway CMakeLists.txt
# Builds Gateway as a static library with NetFilter support

# Gateway needs WPEFramework dependencies
find_package(${NAMESPACE}Plugins REQUIRED)
find_package(CompileSettingsDebug CONFIG REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../helpers)

# Find iptables libraries for NetFilter support
find_path(LIBIPTC_INCLUDE_DIR NAMES libiptc/libiptc.h)
find_library(LIBIPTC_LIBRARY NAMES iptc libiptc)
find_path(LIBIP4TC_INCLUDE_DIR NAMES linux/netfilter_ipv4/ip_tables.h)
find_library(LIBIP4TC_LIBRARY NAMES ip4tc libip4tc)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(LIBIPTC
    DEFAULT_MSG LIBIPTC_LIBRARY LIBIPTC_INCLUDE_DIR)

mark_as_advanced(LIBIPTC_LIBRARY LIBIPTC_INCLUDE_DIR)

if(LIBIPTC_FOUND)
    set(LIBIPTC_INCLUDE_DIRS ${LIBIPTC_INCLUDE_DIR})
    set(LIBIPTC_LIBRARIES ${LIBIPTC_LIBRARY})
endif()

if(LIBIPTC_FOUND AND NOT TARGET LibIptc::LibIptc)
    add_library(LibIptc::LibIptc SHARED IMPORTED)
    set_target_properties(LibIptc::LibIptc PROPERTIES
        IMPORTED_LOCATION "${LIBIPTC_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBIPTC_INCLUDE_DIR}")
endif()

# IPv4 tables
find_package_handle_standard_args(LIBIP4TC
    DEFAULT_MSG LIBIP4TC_LIBRARY LIBIP4TC_INCLUDE_DIR)

mark_as_advanced(LIBIP4TC_LIBRARY LIBIP4TC_INCLUDE_DIR)

if(LIBIP4TC_FOUND)
    set(LIBIP4TC_INCLUDE_DIRS ${LIBIP4TC_INCLUDE_DIR})
    set(LIBIP4TC_LIBRARIES ${LIBIP4TC_LIBRARY})
endif()

if(LIBIP4TC_FOUND AND NOT TARGET LibIptc::LibIp4tc)
    add_library(LibIptc::LibIp4tc SHARED IMPORTED)
    set_target_properties(LibIptc::LibIp4tc PROPERTIES
        IMPORTED_LOCATION "${LIBIP4TC_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBIP4TC_INCLUDE_DIR}")
endif()

# Gateway library sources
set(GATEWAY_SOURCES
    WebInspector.cpp
    NetFilter.cpp
    NetFilterLock.cpp
    NetFilterUtils.c
    ContainerUtils.cpp
)

# Create Gateway static library
add_library(Gateway STATIC ${GATEWAY_SOURCES})

# Set C++ standard
set_target_properties(Gateway PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES)

# Link WPEFramework dependencies
target_link_libraries(Gateway PUBLIC
    CompileSettingsDebug::CompileSettingsDebug
    ${NAMESPACE}Plugins::${NAMESPACE}Plugins)

# Link iptables libraries if found
if(LIBIPTC_FOUND AND LIBIP4TC_FOUND)
    target_link_libraries(Gateway PUBLIC
        LibIptc::LibIp4tc
        LibIptc::LibIptc
    )
target_include_directories(Gateway PRIVATE
        ${LIBIPTC_INCLUDE_DIR}
        ${LIBIP4TC_INCLUDE_DIR}
    )
endif()

# Export include directory to parent
target_include_directories(Gateway PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR})

