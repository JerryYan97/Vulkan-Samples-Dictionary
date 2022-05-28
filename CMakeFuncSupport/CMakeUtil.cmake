# This file contains support functions for building projects.

# Find Vulkan SDK or report an error
function(CheckVulkanSDK)
    IF(DEFINED ENV{VULKAN_SDK})
        message(STATUS "Vulkan SDK environment variable is found.")
    ELSE()
        # Cannot find Vulkan SDK
        message(FATAL_ERROR "Cannot find Vulkan SDK. Please set the VULKAN_SDK env variable.")
    ENDIF()
endfunction()

function(CheckQtEnv)
    IF(DEFINED ENV{QT_CMAKE_ENV_PATH})
        message(STATUS "Qt cmake environment variable is found.")
        IF(EXISTS ${QT_CMAKE_ENV_PATH})
            message(STATUS "QT_CMAKE_ENV_PATH path is valid.")
        ELSE
            message(FATAL_ERROR "The path set in QT_CMAKE_ENV_PATH doesn't exist.")
    ELSE()
        # Cannot find Qt cmake environment variable
        message(FATAL_ERROR "Cannot find Qt Cmake environment variable. Please set the QT_CMAKE_ENV_PATH env variable.")
    ENDIF()
endfunction()


# Copy Qt's core, Gui and Widgets to the project's folder so that the app can find these libraries.
# It needs the PROJECT_NAME and QT_CMAKE_PATH.
function(CopyQtLibs PROJECT_NAME QT_CMAKE_PATH)
    message(STATUS "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
    if (WIN32)
        set(QT_INSTALL_PATH "${QT_CMAKE_PATH}")
        if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
            set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
            if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
                set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
            endif ()
        endif ()
        if (EXISTS "${QT_INSTALL_PATH}/plugins/platforms/qwindows.dll")
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy
                    "${QT_INSTALL_PATH}/plugins/platforms/qwindows.dll"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
        endif ()
        foreach (QT_LIB Core Gui Widgets)
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy
                    "${QT_INSTALL_PATH}/bin/Qt6${QT_LIB}.dll"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
        endforeach (QT_LIB)
    endif ()
endfunction()