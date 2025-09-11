cmake_minimum_required(VERSION 3.5)
project(AppCLifecycleManager)

set(CMAKE_CXX_STANDARD 14)

find_package(WPEFramework REQUIRED)


add_library(AppCLifecycleManager SHARED
    AppCLifecycleManager.cpp
)


target_include_directories(AppCLifecycleManager PRIVATE
    ${CMAKE_SOURCE_DIR}/interfaces
    ${CMAKE_SOURCE_DIR}/AppCLifecycleManager
)


target_link_libraries(AppCLifecycleManager
    WPEFrameworkCore
    WPEFrameworkPlugins
    WPEFrameworkInterfaces
)

install(TARGETS AppCLifecycleManager DESTINATION lib)
install(FILES AppCLifecycleManager.config DESTINATION share/WPEFramework/plugins)
