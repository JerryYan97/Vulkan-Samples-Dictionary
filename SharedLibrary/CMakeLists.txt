# NOTES:
# * Shared library mode.
#
# INPUTS:
# Shared library modes: SHARED_LIB_APP, SHARED_LIB_GLFW, SHARED_LIB_IMGUI
# SHARED_LIB_APP only includes Application.h and Application.cpp. 
# SHARED_LIB_GLFW includes Application.h/cpp, GlfwApplication.h/cpp. 
# SHARED_LIB_IMGUI includes Application.h/cpp, GlfwApplication.h/cpp, DearImGuiApplication.h/cpp.
# SHARED_LIB_GLTF_GLM includes gltf and glm. 
# 

add_library(SharedLibrary STATIC)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    message(FATAL_ERROR "The SharedLibrary cannot be built alone.")
else()
    add_definitions(-DSHARED_LIB_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}\")

    # Load applications source files.
    if(DEFINED SHARED_LIB_APP)
        target_sources(
            SharedLibrary PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/Application/Application.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/Application/Application.h
        )
    elseif(DEFINED SHARED_LIB_GLFW)
        target_sources(
            SharedLibrary PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/Application/Application.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/Application/Application.h
            ${CMAKE_CURRENT_SOURCE_DIR}/Application/GlfwApplication.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/Application/GlfwApplication.h
        )

        target_include_directories(SharedLibrary PUBLIC ${GLFW_HEADER_DIR})
        target_link_libraries(SharedLibrary glfw)

    elseif(DEFINED SHARED_LIB_IMGUI)
        set(IMGUI_FILES_LIST ${CMAKE_CURRENT_SOURCE_DIR}/../ThirdPartyLibs/DearImGUI/imgui.cpp
                             ${CMAKE_CURRENT_SOURCE_DIR}/../ThirdPartyLibs/DearImGUI/imgui_draw.cpp
                             ${CMAKE_CURRENT_SOURCE_DIR}/../ThirdPartyLibs/DearImGUI/imgui_demo.cpp
                             ${CMAKE_CURRENT_SOURCE_DIR}/../ThirdPartyLibs/DearImGUI/imgui_tables.cpp
                             ${CMAKE_CURRENT_SOURCE_DIR}/../ThirdPartyLibs/DearImGUI/imgui_widgets.cpp
                             ${CMAKE_CURRENT_SOURCE_DIR}/../ThirdPartyLibs/DearImGUI/backends/imgui_impl_vulkan.cpp
                             ${CMAKE_CURRENT_SOURCE_DIR}/../ThirdPartyLibs/DearImGUI/backends/imgui_impl_glfw.cpp)

        target_sources(
            SharedLibrary PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/Application/Application.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/Application/Application.h
            ${CMAKE_CURRENT_SOURCE_DIR}/Application/GlfwApplication.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/Application/GlfwApplication.h
            ${CMAKE_CURRENT_SOURCE_DIR}/Application/DearImGuiApplication.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/Application/DearImGuiApplication.h
            ${IMGUI_FILES_LIST})

        include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../ThirdPartyLibs/DearImGUI
                            ${CMAKE_CURRENT_SOURCE_DIR}/../ThirdPartyLibs/DearImGUI/backends
                            ${GLFW_HEADER_DIR})

        # Category the source files
        source_group(ImGUI FILES ${IMGUI_FILES_LIST})

        target_include_directories(SharedLibrary PUBLIC ${IMGUI_HEADER_DIR})
        target_link_libraries(SharedLibrary glfw)

    else()
        message(FATAL_ERROR "Application must defines one of SHARED_LIB_APP, SHARED_LIB_GLFW and SHARED_LIB_IMGUI.")
    endif()


    if(DEFINED SHARED_LIB_GLTF_GLM)
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../ThirdPartyLibs/glm
                         ${CMAKE_CURRENT_BINARY_DIR}/SharedLibrary)
        target_link_libraries(SharedLibrary glm::glm)
        add_dependencies(SharedLibrary glm::glm)
        include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../ThirdPartyLibs/TinyGltf)
    endif()

    if(DEFINED SHARED_LIB_SCENE_ASSETS_UTILS)
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/AssetsLoader)
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Scene)
    endif()

    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../ThirdPartyLibs/stb)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Application)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Camera)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Event)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Pipeline)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Utils)
    
    if(NOT "../../ThirdPartyLibs/VMA" IN_LIST INCLUDE_DIRECTORIES)
        include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../ThirdPartyLibs/VMA)
    endif()

    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Camera)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Event)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Pipeline)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Utils)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/AnimLogger)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Actor)

    target_compile_features(SharedLibrary PRIVATE cxx_std_17)

    target_link_libraries(SharedLibrary vulkan-1)

    # AppUtils Shaders Compile
    if(NOT DEFINED SHARED_LIB_HLSL_DIR)
        set(SHARED_LIB_HLSL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/HLSL")
    endif()

    if(NOT DEFINED TOOL_DIR)
        set(TOOL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Tools")
    endif()

    # Deal with the building warning.
    set(DUMMY_FILE dummy)
    set_property(SOURCE DUMMY_FILE
                 PROPERTY SYMBOLIC True)

    add_custom_command(
            OUTPUT
                DUMMY_FILE
            COMMAND python
                ${TOOL_DIR}/GenerateShaderHeader.py ARGS --src ${SHARED_LIB_HLSL_DIR}/cubemapFormat_vert.hlsl --dstDir ${SHARED_LIB_HLSL_DIR}
            COMMAND python
                ${TOOL_DIR}/GenerateShaderHeader.py ARGS --src ${SHARED_LIB_HLSL_DIR}/cubemapFormat_frag.hlsl --dstDir ${SHARED_LIB_HLSL_DIR}
            COMMAND python
                ${TOOL_DIR}/GenerateShaderHeader.py ARGS --src ${SHARED_LIB_HLSL_DIR}/gammaCorrection_vert.hlsl --dstDir ${SHARED_LIB_HLSL_DIR}
            COMMAND python
                ${TOOL_DIR}/GenerateShaderHeader.py ARGS --src ${SHARED_LIB_HLSL_DIR}/gammaCorrection_frag.hlsl --dstDir ${SHARED_LIB_HLSL_DIR}
    )

    add_custom_target(SHARED_LIB_SHADER_COMPILE
                      DEPENDS DUMMY_FILE)

    add_dependencies(SharedLibrary SHARED_LIB_SHADER_COMPILE)
endif()