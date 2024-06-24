# Debug version
include(../../../CMakeFuncSupport/CMakeUtil.cmake)
set(MY_APP_NAME "SSAO")
CheckVulkanSDK()
cmake_minimum_required(VERSION 3.5)
project(SSAO VERSION 0.1 LANGUAGES CXX)

include_directories("$ENV{VULKAN_SDK}/Include" "../../../ThirdPartyLibs/VMA")

set(GLFW_HEADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../ThirdPartyLibs/glfw/include/GLFW)
set(IMGUI_HEADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../ThirdPartyLibs/DearImGUI)
include_directories(${IMGUI_HEADER_DIR}
                    ${GLFW_HEADER_DIR})

set(TinyObjPath ../../../ThirdPartyLibs/TinyObj)
include_directories(${TinyObjPath})

link_directories("$ENV{VULKAN_SDK}/lib")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../ThirdPartyLibs/glfw
                 ${CMAKE_CURRENT_BINARY_DIR}/glfw)

add_definitions(-DSOURCE_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}\")
add_executable(${MY_APP_NAME} "main.cpp"
                              ${CMAKE_CURRENT_SOURCE_DIR}/SSAOApp.h
                              ${CMAKE_CURRENT_SOURCE_DIR}/SSAOApp.cpp)

# Load the shared library.
set(SHARED_LIB_IMGUI TRUE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../SharedLibrary
                 ${CMAKE_CURRENT_BINARY_DIR}/SharedLibrary)

get_target_property(APP_SRC_LIST ${MY_APP_NAME} SOURCES)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/../../../ FILES ${APP_SRC_LIST})

target_compile_features(${MY_APP_NAME} PRIVATE cxx_std_17)

target_link_libraries(${MY_APP_NAME} SharedLibrary vulkan-1 glfw)

add_dependencies(${MY_APP_NAME} glfw)
add_dependencies(${MY_APP_NAME} SharedLibrary)

set(SHARED_LIB_HLSL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../SharedLibrary/HLSL")

# Deal with the building warning.
set(DUMMY_FILE dummy)
set_property(SOURCE DUMMY_FILE
             PROPERTY SYMBOLIC True)

add_custom_command(
        OUTPUT
            DUMMY_FILE
        COMMAND python
            ${SHARED_LIB_HLSL_DIR}/HLSLCompile.py ARGS --src ${CMAKE_CURRENT_SOURCE_DIR}/hlsl/geo_vert.hlsl --dstDir ${CMAKE_CURRENT_SOURCE_DIR}/hlsl
        COMMAND python
            ${SHARED_LIB_HLSL_DIR}/HLSLCompile.py ARGS --src ${CMAKE_CURRENT_SOURCE_DIR}/hlsl/geo_frag.hlsl --dstDir ${CMAKE_CURRENT_SOURCE_DIR}/hlsl
        COMMAND python
            ${SHARED_LIB_HLSL_DIR}/HLSLCompile.py ARGS --src ${CMAKE_CURRENT_SOURCE_DIR}/hlsl/lighting_vert.hlsl --dstDir ${CMAKE_CURRENT_SOURCE_DIR}/hlsl
        COMMAND python
            ${SHARED_LIB_HLSL_DIR}/HLSLCompile.py ARGS --src ${CMAKE_CURRENT_SOURCE_DIR}/hlsl/lighting_frag.hlsl --dstDir ${CMAKE_CURRENT_SOURCE_DIR}/hlsl
)

add_custom_target(SHADER_COMPILE
    DEPENDS DUMMY_FILE)

add_dependencies(${MY_APP_NAME} SHADER_COMPILE)