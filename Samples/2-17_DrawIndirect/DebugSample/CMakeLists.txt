# Debug version
include(../../../CMakeFuncSupport/CMakeUtil.cmake)
set(MY_APP_NAME "DrawIndirectDumpTris")
CheckVulkanSDK()
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

set(SHARED_LIB_HLSL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../SharedLibrary/HLSL")

# Deal with the building warning.
set(DUMMY_FILE dummy)
set_property(SOURCE DUMMY_FILE
             PROPERTY SYMBOLIC True)

add_custom_command(
        OUTPUT
            DUMMY_FILE
        COMMAND python
            ${SHARED_LIB_HLSL_DIR}/HLSLCompile.py ARGS --src ${CMAKE_CURRENT_SOURCE_DIR}/hlsl/tri_vert.hlsl --dstDir ${CMAKE_CURRENT_SOURCE_DIR}/hlsl
        COMMAND python
            ${SHARED_LIB_HLSL_DIR}/HLSLCompile.py ARGS --src ${CMAKE_CURRENT_SOURCE_DIR}/hlsl/tri_frag.hlsl --dstDir ${CMAKE_CURRENT_SOURCE_DIR}/hlsl
)

add_custom_target(SHADER_COMPILE
    DEPENDS DUMMY_FILE)

add_dependencies(${MY_APP_NAME} SHADER_COMPILE)