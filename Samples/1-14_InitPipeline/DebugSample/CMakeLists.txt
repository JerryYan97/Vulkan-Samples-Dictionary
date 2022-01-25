# Debug version
include(../../../CMakeFuncSupport/CMakeUtil.cmake)
set(MY_APP_NAME "InitPipeline")
CheckVulkanSDK()
cmake_minimum_required(VERSION 3.5)
project(InitPipeline VERSION 0.1 LANGUAGES CXX)
include_directories("$ENV{VULKAN_SDK}/Include")
link_directories("$ENV{VULKAN_SDK}/lib")
add_definitions(-DSOURCE_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}\")
add_executable(${MY_APP_NAME} "main.cpp")
target_link_libraries(${MY_APP_NAME} vulkan-1)

execute_process(COMMAND glslc init_pipeline.vert -o init_pipeline.vert.spv
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
execute_process(COMMAND glslc init_pipeline.frag -o init_pipeline.frag.spv
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})