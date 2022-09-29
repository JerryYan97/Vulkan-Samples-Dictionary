# Debug version
include(../../../CMakeFuncSupport/CMakeUtil.cmake)
set(MY_APP_NAME "RealTimeTri")
CheckVulkanSDK()
cmake_minimum_required(VERSION 3.5)
project(RealTimeTriangle VERSION 0.1 LANGUAGES CXX)

include_directories("$ENV{VULKAN_SDK}/Include")
include_directories(../../../ThirdPartyLibs/glfw/include/GLFW)

link_directories("$ENV{VULKAN_SDK}/lib")
link_directories("../../../ThirdPartyLibs/glfw/build/src/Debug/")

add_definitions(-DSOURCE_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}\")
add_executable(${MY_APP_NAME} "main.cpp")

target_link_libraries(${MY_APP_NAME} vulkan-1)
target_link_libraries(${MY_APP_NAME} glfw3)