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
