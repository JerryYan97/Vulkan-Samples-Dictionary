# Debug version
include(../../../CMakeFuncSupport/CMakeUtil.cmake)
set(MY_APP_NAME "PBRIBL")
CheckVulkanSDK()
cmake_minimum_required(VERSION 3.5)
project(PBRIBL VERSION 0.1 LANGUAGES CXX)

include_directories("$ENV{VULKAN_SDK}/Include" "../../../ThirdPartyLibs/VMA")
include_directories(../../../ThirdPartyLibs/glfw/include/GLFW)
include_directories(../../../ThirdPartyLibs/glfw/include)

set(StbImagePath ../../../ThirdPartyLibs/stb)
include_directories(${StbImagePath})

set(TinyObjPath ../../../ThirdPartyLibs/TinyObj)
include_directories(${TinyObjPath})

include_directories(../../../ThirdPartyLibs/RenderDoc/renderdoc/api/app)

link_directories("$ENV{VULKAN_SDK}/lib")
link_directories("../../../ThirdPartyLibs/glfw/build/src/Debug/")

add_definitions(-DSOURCE_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}\")
add_executable(${MY_APP_NAME} "main.cpp"
                              ${CMAKE_CURRENT_SOURCE_DIR}/PBRIBLApp.h
                              ${CMAKE_CURRENT_SOURCE_DIR}/PBRIBLApp.cpp)

# Load the shared library.
set(SHARED_LIB_GLFW TRUE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../SharedLibrary ${CMAKE_CURRENT_BINARY_DIR}/SharedLibrary)

get_target_property(APP_SRC_LIST ${MY_APP_NAME} SOURCES)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/../../../ FILES ${APP_SRC_LIST})

target_compile_features(${MY_APP_NAME} PRIVATE cxx_std_17)

target_link_libraries(${MY_APP_NAME} vulkan-1)
target_link_libraries(${MY_APP_NAME} glfw3)
target_link_libraries(${MY_APP_NAME} SharedLibrary)

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
            ${SHARED_LIB_HLSL_DIR}/HLSLCompile.py ARGS --src ${CMAKE_CURRENT_SOURCE_DIR}/hlsl/skybox_vert.hlsl --dstDir ${CMAKE_CURRENT_SOURCE_DIR}/hlsl
        COMMAND python
            ${SHARED_LIB_HLSL_DIR}/HLSLCompile.py ARGS --src ${CMAKE_CURRENT_SOURCE_DIR}/hlsl/skybox_frag.hlsl --dstDir ${CMAKE_CURRENT_SOURCE_DIR}/hlsl
        COMMAND python
            ${SHARED_LIB_HLSL_DIR}/HLSLCompile.py ARGS --src ${CMAKE_CURRENT_SOURCE_DIR}/hlsl/ibl_vert.hlsl --dstDir ${CMAKE_CURRENT_SOURCE_DIR}/hlsl
        COMMAND python
            ${SHARED_LIB_HLSL_DIR}/HLSLCompile.py ARGS --src ${CMAKE_CURRENT_SOURCE_DIR}/hlsl/ibl_frag.hlsl --dstDir ${CMAKE_CURRENT_SOURCE_DIR}/hlsl
)

add_custom_target(SHADER_COMPILE
    DEPENDS DUMMY_FILE)

add_dependencies(${MY_APP_NAME} SHADER_COMPILE)

# execute_process(COMMAND glslc ./glsl/skybox.vert -o skybox_vert.spv
#         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
# execute_process(COMMAND glslc ./glsl/skybox.frag -o skybox_frag.spv
#         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
# execute_process(COMMAND glslc ./glsl/sphere.vert -o sphere_vert.spv
#         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
# execute_process(COMMAND glslc ./glsl/sphere.frag -o sphere_frag.spv
#         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})