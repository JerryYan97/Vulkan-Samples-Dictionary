# Release version
get_directory_property(hasParent PARENT_DIRECTORY)
include(../../../CMakeFuncSupport/CMakeUtil.cmake)
set(MY_APP_NAME "ArrayPow2")
CheckVulkanSDK()
cmake_minimum_required(VERSION 3.5)
project(ArrayPow2 VERSION 0.1 LANGUAGES CXX)
include_directories("$ENV{VULKAN_SDK}/Include")
link_directories("$ENV{VULKAN_SDK}/lib")
add_definitions(-DSOURCE_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}\")
add_executable(${MY_APP_NAME} "main.cpp")
target_link_libraries(${MY_APP_NAME} vulkan-1)

execute_process(COMMAND glslc ArrayPow2.comp -o ArrayPow2.comp.spv
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})