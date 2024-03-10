# Debug version
include(../../../CMakeFuncSupport/CMakeUtil.cmake)
set(MY_APP_NAME "GlfwTri")
CheckVulkanSDK()
cmake_minimum_required(VERSION 3.5)
project(RealTimeTriangle VERSION 0.1 LANGUAGES CXX)

include_directories("$ENV{VULKAN_SDK}/Include")
# include_directories(../../../ThirdPartyLibs/glfw/include/GLFW)

set(GLFW_HEADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../ThirdPartyLibs/glfw/include/GLFW)
include_directories(${GLFW_HEADER_DIR})

link_directories("$ENV{VULKAN_SDK}/lib")
# link_directories("../../../ThirdPartyLibs/glfw/build/src/Debug/")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../ThirdPartyLibs/glfw
                 ${CMAKE_CURRENT_BINARY_DIR}/glfw)

add_definitions(-DSOURCE_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}\")
add_executable(${MY_APP_NAME} "main.cpp")

target_compile_features(${MY_APP_NAME} PRIVATE cxx_std_17)

target_link_libraries(${MY_APP_NAME} vulkan-1 glfw)

add_dependencies(${MY_APP_NAME} glfw)

execute_process(COMMAND glslc ./glsl/glsl.vert -o vert.spv
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
execute_process(COMMAND glslc ./glsl/glsl.frag -o frag.spv
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})