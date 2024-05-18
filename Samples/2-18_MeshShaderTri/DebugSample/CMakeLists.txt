# Debug version
include(../../../CMakeFuncSupport/CMakeUtil.cmake)
set(MY_APP_NAME "MeshShaderTriangle")
cmake_minimum_required(VERSION 3.5)
project(DrawIndirectDumpTris VERSION 0.1 LANGUAGES CXX)
include_directories("$ENV{VULKAN_SDK}/Include"
                    "${CMAKE_CURRENT_SOURCE_DIR}/../../../ThirdPartyLibs/lodepng"
                    "$ENV{VULKAN_SDK}/Include"
                    "../../../ThirdPartyLibs/VMA"
                    "${CMAKE_CURRENT_SOURCE_DIR}/../../../ThirdPartyLibs/RenderDoc/renderdoc/api/app")

link_directories("$ENV{VULKAN_SDK}/lib")
add_definitions(-DSOURCE_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}\")
add_executable(${MY_APP_NAME} "main.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/../../../ThirdPartyLibs/lodepng/lodepng.cpp")
target_link_libraries(${MY_APP_NAME} vulkan-1)