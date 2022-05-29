# Release version
include(../../../CMakeFuncSupport/CMakeUtil.cmake)
CheckVulkanSDK()
CheckQtEnv()
cmake_minimum_required(VERSION 3.5)
project(QtHellowWindow VERSION 0.1 LANGUAGES CXX)
set(MY_APP_NAME "QtHellowWindow")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_PREFIX_PATH $ENV{QT_CMAKE_ENV_PATH})
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

add_definitions(-DSOURCE_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}\")

link_directories("$ENV{VULKAN_SDK}/lib")
add_executable(${MY_APP_NAME} "main.cpp")
target_link_libraries(${MY_APP_NAME}
                      vulkan-1
                      Qt::Core
                      Qt::Gui
                      Qt::Widgets)

CopyQtLibs(${MY_APP_NAME} ${CMAKE_PREFIX_PATH})