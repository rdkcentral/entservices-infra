cmake_minimum_required(VERSION 3.5)
project(AppCLifecycleManagerTest)

set(CMAKE_CXX_STANDARD 14)

find_package(WPEFramework REQUIRED)


add_library(AppCLifecycleManagerTest SHARED
    AppCLifecycleManagerTest.cpp
)


target_include_directories(AppCLifecycleManagerTest PRIVATE
    ${CMAKE_SOURCE_DIR}/interfaces
    ${CMAKE_SOURCE_DIR}/AppCLifecycleManagerTest
)


target_link_libraries(AppCLifecycleManagerTest
    WPEFrameworkCore
    WPEFrameworkPlugins
    WPEFrameworkInterfaces
)

install(TARGETS AppCLifecycleManagerTest DESTINATION lib)
install(FILES AppCLifecycleManagerTest.config DESTINATION share/WPEFramework/plugins)
