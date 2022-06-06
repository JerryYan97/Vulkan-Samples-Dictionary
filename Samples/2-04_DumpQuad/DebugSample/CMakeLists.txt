# Debug version
include(../../../CMakeFuncSupport/CMakeUtil.cmake)
set(MY_APP_NAME "DumpQuad")
CheckVulkanSDK()
cmake_minimum_required(VERSION 3.5)
project(DumpTri VERSION 0.1 LANGUAGES CXX)
include_directories("$ENV{VULKAN_SDK}/Include"
                    "${CMAKE_CURRENT_SOURCE_DIR}/../../../ThirdPartyLibs/lodepng"
                    "$ENV{VULKAN_SDK}/Include" "../../../ThirdPartyLibs/VMA")
link_directories("$ENV{VULKAN_SDK}/lib")
add_definitions(-DSOURCE_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}\")
add_executable(${MY_APP_NAME} "main.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/../../../ThirdPartyLibs/lodepng/lodepng.cpp")
target_link_libraries(${MY_APP_NAME} vulkan-1)

execute_process(COMMAND glslc DumpQuad.vert -o DumpQuad.vert.spv
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
execute_process(COMMAND glslc DumpQuad.frag -o DumpQuad.frag.spv
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})