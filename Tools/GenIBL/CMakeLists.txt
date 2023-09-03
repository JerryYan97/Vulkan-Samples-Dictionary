# Debug version
include(../../../CMakeFuncSupport/CMakeUtil.cmake)
set(MY_APP_NAME "GenIBL")
CheckVulkanSDK()
cmake_minimum_required(VERSION 3.5)
project(GenIBL VERSION 0.1 LANGUAGES CXX)

include_directories("$ENV{VULKAN_SDK}/Include" "../../../ThirdPartyLibs/VMA")
include_directories(../../ThirdPartyLibs/args)
include_directories(../../ThirdPartyLibs/stb)
include_directories(../../ThirdPartyLibs/RenderDoc/renderdoc/api/app)

link_directories("$ENV{VULKAN_SDK}/lib")

add_definitions(-DSOURCE_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}\")
add_executable(${MY_APP_NAME} "main.cpp"
                              ${CMAKE_CURRENT_SOURCE_DIR}/GenIBL.h
                              ${CMAKE_CURRENT_SOURCE_DIR}/GenIBL.cpp)

# Load the shared library.
set(SHARED_LIB_APP TRUE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../SharedLibrary ${CMAKE_CURRENT_BINARY_DIR}/SharedLibrary)

get_target_property(APP_SRC_LIST ${MY_APP_NAME} SOURCES)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${APP_SRC_LIST})

target_compile_features(${MY_APP_NAME} PRIVATE cxx_std_17)

target_link_libraries(${MY_APP_NAME} vulkan-1)
target_link_libraries(${MY_APP_NAME} SharedLibrary)

add_dependencies(${MY_APP_NAME} SharedLibrary)

execute_process(COMMAND glslc ./glsl/skybox.vert -o skybox_vert.spv
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
execute_process(COMMAND glslc ./glsl/skybox.frag -o skybox_frag.spv
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
execute_process(COMMAND glslc ./glsl/sphere.vert -o sphere_vert.spv
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
execute_process(COMMAND glslc ./glsl/sphere.frag -o sphere_frag.spv
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})