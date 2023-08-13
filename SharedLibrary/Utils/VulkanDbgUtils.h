#pragma once

#include <iostream>
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#define STR(r)    \
	case r:       \
		return #r \

static const std::string to_string(VkResult result)
{
    switch (result) {
        STR(VK_NOT_READY);
        STR(VK_TIMEOUT);
        STR(VK_EVENT_SET);
        STR(VK_EVENT_RESET);
        STR(VK_INCOMPLETE);
        STR(VK_ERROR_OUT_OF_HOST_MEMORY);
        STR(VK_ERROR_OUT_OF_DEVICE_MEMORY);
        STR(VK_ERROR_INITIALIZATION_FAILED);
        STR(VK_ERROR_DEVICE_LOST);
        STR(VK_ERROR_MEMORY_MAP_FAILED);
        STR(VK_ERROR_LAYER_NOT_PRESENT);
        STR(VK_ERROR_EXTENSION_NOT_PRESENT);
        STR(VK_ERROR_FEATURE_NOT_PRESENT);
        STR(VK_ERROR_INCOMPATIBLE_DRIVER);
        STR(VK_ERROR_TOO_MANY_OBJECTS);
        STR(VK_ERROR_FORMAT_NOT_SUPPORTED);
        STR(VK_ERROR_FRAGMENTED_POOL);
        STR(VK_ERROR_UNKNOWN);
        STR(VK_ERROR_OUT_OF_POOL_MEMORY);
        STR(VK_ERROR_INVALID_EXTERNAL_HANDLE);
        STR(VK_ERROR_FRAGMENTATION);
        STR(VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS);
        STR(VK_ERROR_SURFACE_LOST_KHR);
        STR(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
        STR(VK_SUBOPTIMAL_KHR);
        STR(VK_ERROR_OUT_OF_DATE_KHR);
        STR(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
        STR(VK_ERROR_VALIDATION_FAILED_EXT);
        STR(VK_ERROR_INVALID_SHADER_NV);
        STR(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT);
        STR(VK_ERROR_NOT_PERMITTED_EXT);
        STR(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT);
        STR(VK_THREAD_IDLE_KHR);
        STR(VK_THREAD_DONE_KHR);
        STR(VK_OPERATION_DEFERRED_KHR);
        STR(VK_OPERATION_NOT_DEFERRED_KHR);
        STR(VK_PIPELINE_COMPILE_REQUIRED_EXT);
    default:
        return "UNKNOWN_ERROR";
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_utils_messenger_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data)
{
    if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        std::cout << "Callback Warning: " << callback_data->messageIdNumber << ":" << callback_data->pMessageIdName << ":" << callback_data->pMessage << std::endl;
    }
    else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        std::cerr << "Callback Error: " << callback_data->messageIdNumber << ":" << callback_data->pMessageIdName << ":" << callback_data->pMessage << std::endl;
    }
    return VK_FALSE;
}

#define VK_CHECK(res) if(res){std::cout << "Error at line:" << __LINE__ << ", Error name:" << to_string(res) << ".\n"; exit(1);}

static void CheckVkResult(VkResult err)
{
    VK_CHECK(err);
}

static void PrintAllVkFormatFeatureFlagsBits(VkFormatFeatureFlags printFlags)
{
    uint32_t checkEnum = 0;
    do {
        if (checkEnum & printFlags)
        {
            std::cout << string_VkFormatFeatureFlagBits((VkFormatFeatureFlagBits)checkEnum) << std::endl;
        }

        if (checkEnum == 0)
        {
            checkEnum = 1;
        }
        else
        {
            checkEnum = checkEnum << 1;
        }
    } while (checkEnum <= (uint32_t)printFlags);
}

static void CheckVkImageSupport(VkPhysicalDevice device, VkImageCreateInfo dbgImgInfo)
{
    VkFormatProperties properties;
    vkGetPhysicalDeviceFormatProperties(device, dbgImgInfo.format, &properties);
    std::cout << "linearTilingFeatures:  " << properties.linearTilingFeatures << std::endl;
    PrintAllVkFormatFeatureFlagsBits(properties.linearTilingFeatures);
    std::cout << std::endl;

    std::cout << "optimalTilingFeatures: " << properties.optimalTilingFeatures << std::endl;
    PrintAllVkFormatFeatureFlagsBits(properties.optimalTilingFeatures);
    std::cout << std::endl;

    std::cout << "bufferFeatures:        " << properties.bufferFeatures << std::endl;
    PrintAllVkFormatFeatureFlagsBits(properties.bufferFeatures);
    std::cout << std::endl << std::endl;
}

static void PrintFormatForColorRenderTarget(VkPhysicalDevice device)
{
    uint32_t checkEnum = 0;
    do {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(device, (VkFormat)checkEnum, &properties);

        if (properties.linearTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)
        {
            std::cout << string_VkFormat((VkFormat)checkEnum) << " supports linear tiling color attachment." << std::endl;
        }

        if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)
        {
            std::cout << string_VkFormat((VkFormat)checkEnum) << " supports optimal tiling color attachment." << std::endl;
        }

        checkEnum++;
    } while (checkEnum <= VK_FORMAT_ASTC_12x12_SRGB_BLOCK);
}
