get_directory_property(hasParent PARENT_DIRECTORY)
include(../../CMakeFuncSupport/CMakeUtil.cmake)
set(MY_APP_NAME "DebugMessenger")
CheckVulkanSDK()

if(hasParent)
    # Has a parent scope
    
else()
    # Doesn't have a parent scope. We only build project at top-level or bottom-level for all samples.
    cmake_minimum_required(VERSION 3.5)
    project(DebugMessenger VERSION 0.1 LANGUAGES CXX)
    include_directories("$ENV{VULKAN_SDK}/Include")
    link_directories("$ENV{VULKAN_SDK}/lib")
    add_definitions(-DSOURCE_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}\")
    add_executable(${MY_APP_NAME} "main.cpp")
    target_link_libraries(${MY_APP_NAME} vulkan-1)
endif()