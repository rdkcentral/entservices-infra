cmake_minimum_required(VERSION 3.5)
project(AppcLifecycleManager)

set(CMAKE_CXX_STANDARD 14)

find_package(WPEFramework REQUIRED)


add_library(AppcLifecycleManager SHARED
    AppcLifecycleManager.cpp
)


target_include_directories(AppcLifecycleManager PRIVATE
    ${CMAKE_SOURCE_DIR}/interfaces
    ${CMAKE_SOURCE_DIR}/AppcLifecycleManager
)


target_link_libraries(AppcLifecycleManager
    WPEFrameworkCore
    WPEFrameworkPlugins
    WPEFrameworkInterfaces
)

install(TARGETS AppcLifecycleManager DESTINATION lib)
install(FILES AppcLifecycleManager.config DESTINATION share/WPEFramework/plugins)