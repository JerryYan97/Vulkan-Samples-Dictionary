# Debug version
include(../../../CMakeFuncSupport/CMakeUtil.cmake)
set(MY_APP_NAME "DockImgTemplate")
CheckVulkanSDK()
cmake_minimum_required(VERSION 3.5)
project(DockImgTemplate VERSION 0.1 LANGUAGES CXX)

include_directories("$ENV{VULKAN_SDK}/Include" "../../../ThirdPartyLibs/VMA")
include_directories(../../../ThirdPartyLibs/glfw/include/GLFW)
include_directories(../../../ThirdPartyLibs/glfw/include)

set(DearImGUIPath ../../../ThirdPartyLibs/DearImGUI)
include_directories(${DearImGUIPath})
include_directories(${DearImGUIPath}/backends)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../../ThirdPartyLibs/lodepng")

link_directories("$ENV{VULKAN_SDK}/lib")
link_directories("../../../ThirdPartyLibs/glfw/build/src/Debug/")

add_definitions(-DSOURCE_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}\")
add_executable(${MY_APP_NAME} "main.cpp" 
                              ${DearImGUIPath}/imgui.cpp 
                              ${DearImGUIPath}/imgui_draw.cpp
                              ${DearImGUIPath}/imgui_demo.cpp
                              ${DearImGUIPath}/imgui_tables.cpp
                              ${DearImGUIPath}/imgui_widgets.cpp
                              ${DearImGUIPath}/backends/imgui_impl_vulkan.cpp 
                              ${DearImGUIPath}/backends/imgui_impl_glfw.cpp)

target_compile_features(${MY_APP_NAME} PRIVATE cxx_std_17)

target_link_libraries(${MY_APP_NAME} vulkan-1)
target_link_libraries(${MY_APP_NAME} glfw3)

execute_process(COMMAND glslc ./glsl/glsl.vert -o vert.spv
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
execute_process(COMMAND glslc ./glsl/glsl.frag -o frag.spv
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})