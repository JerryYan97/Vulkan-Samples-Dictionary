include(../../CMakeFuncSupport/CMakeUtil.cmake)
set(MY_APP_NAME "SphericalToCubemap")
CheckVulkanSDK()
cmake_minimum_required(VERSION 3.5)
project(SphericalToCubemap VERSION 0.1 LANGUAGES CXX)

include_directories("$ENV{VULKAN_SDK}/Include" "../../ThirdPartyLibs/VMA")
include_directories(../../ThirdPartyLibs/args)
include_directories(../../ThirdPartyLibs/RenderDoc/renderdoc/api/app)

link_directories("$ENV{VULKAN_SDK}/lib")

add_definitions(-DSOURCE_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}\")
add_executable(${MY_APP_NAME} "main.cpp"
                              ${CMAKE_CURRENT_SOURCE_DIR}/SphericalToCubemap.h
                              ${CMAKE_CURRENT_SOURCE_DIR}/SphericalToCubemap.cpp)
# Load the shared library.
set(SHARED_LIB_APP TRUE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../SharedLibrary ${CMAKE_CURRENT_BINARY_DIR}/SharedLibrary)

get_target_property(APP_SRC_LIST ${MY_APP_NAME} SOURCES)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${APP_SRC_LIST})

target_compile_features(${MY_APP_NAME} PRIVATE cxx_std_17)

target_link_libraries(${MY_APP_NAME} vulkan-1)
target_link_libraries(${MY_APP_NAME} SharedLibrary)

add_dependencies(${MY_APP_NAME} SharedLibrary)

# Deal with the building warning.
set(DUMMY_FILE dummy)
set_property(SOURCE DUMMY_FILE
             PROPERTY SYMBOLIC True)

set(TOOL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")

add_custom_command(
        OUTPUT
            DUMMY_FILE
        COMMAND python
            ${TOOL_DIR}/GenerateShaderHeader.py ARGS --src ${CMAKE_CURRENT_SOURCE_DIR}/hlsl/toCubeMap_vert.hlsl --dstDir ${CMAKE_CURRENT_SOURCE_DIR}/hlsl
        COMMAND python
            ${TOOL_DIR}/GenerateShaderHeader.py ARGS --src ${CMAKE_CURRENT_SOURCE_DIR}/hlsl/toCubeMap_frag.hlsl --dstDir ${CMAKE_CURRENT_SOURCE_DIR}/hlsl
)

add_custom_target(SHADER_COMPILE
    DEPENDS DUMMY_FILE)

add_dependencies(${MY_APP_NAME} SHADER_COMPILE)