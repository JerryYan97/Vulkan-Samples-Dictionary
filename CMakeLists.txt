# Top-level CMake file
cmake_minimum_required(VERSION 3.5)
include(./CMakeFuncSupport/CMakeUtil.cmake)

project(OneFileVulkans VERSION 0.1 LANGUAGES CXX)

CheckVulkanSDK()

include_directories(./Compute)

include_directories(./Graphics)

