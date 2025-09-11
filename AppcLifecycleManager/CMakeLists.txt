cmake_minimum_required(VERSION 3.5)
<<<<<<< HEAD
project(AppCLifecycleManager)
=======
project(AppCLifecycleManagerTest)
>>>>>>> c015b04996a01a19d6ea7596f8c15b59514812a4

set(CMAKE_CXX_STANDARD 14)

find_package(WPEFramework REQUIRED)


<<<<<<< HEAD
add_library(AppCLifecycleManager SHARED
    AppCLifecycleManager.cpp
)


target_include_directories(AppCLifecycleManager PRIVATE
    ${CMAKE_SOURCE_DIR}/interfaces
    ${CMAKE_SOURCE_DIR}/AppCLifecycleManager
)


target_link_libraries(AppCLifecycleManager
=======
add_library(AppCLifecycleManagerTest SHARED
    AppCLifecycleManagerTest.cpp
)


target_include_directories(AppCLifecycleManagerTest PRIVATE
    ${CMAKE_SOURCE_DIR}/interfaces
    ${CMAKE_SOURCE_DIR}/AppCLifecycleManagerTest
)


target_link_libraries(AppCLifecycleManagerTest
>>>>>>> c015b04996a01a19d6ea7596f8c15b59514812a4
    WPEFrameworkCore
    WPEFrameworkPlugins
    WPEFrameworkInterfaces
)

<<<<<<< HEAD
install(TARGETS AppCLifecycleManager DESTINATION lib)
install(FILES AppCLifecycleManager.config DESTINATION share/WPEFramework/plugins)
=======
install(TARGETS AppCLifecycleManagerTest DESTINATION lib)
install(FILES AppCLifecycleManagerTest.config DESTINATION share/WPEFramework/plugins)
>>>>>>> c015b04996a01a19d6ea7596f8c15b59514812a4
