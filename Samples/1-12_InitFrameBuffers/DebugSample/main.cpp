//
// Created by Jerry on 11/28/2021.
//
#include <iostream>
#include <vector>
#include <cassert>
#include "vulkan/vulkan.h"

#define STR(r)    \
	case r:       \
		return #r \

const std::string to_string(VkResult result)
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

VKAPI_ATTR VkBool32 VKAPI_CALL debug_utils_messenger_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
        VkDebugUtilsMessageTypeFlagsEXT message_type,
        const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
        void *user_data)
{
    if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        std::cout << "Callback Warning: " << callback_data->messageIdNumber << ":" << callback_data->pMessageIdName << ":" <<  callback_data->pMessage << std::endl;
    }
    else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        std::cerr << "Callback Error: " << callback_data->messageIdNumber << ":" << callback_data->pMessageIdName << ":" <<  callback_data->pMessage << std::endl;
    }
    return VK_FALSE;
}

#define VK_CHECK(res) if(res){std::cout << "Error at line:" << __LINE__ << ", Error name:" << to_string(res) << ".\n"; exit(1);}

int main()
{
    // Verify that the debug extension for the callback messenger is supported.
    uint32_t propNum;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &propNum, nullptr));
    assert(propNum >= 1);
    std::vector<VkExtensionProperties> props(propNum);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &propNum, props.data()));
    for (int i = 0; i < props.size(); ++i)
    {
        if(strcmp(props[i].extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
        {
            break;
        }
        if(i == propNum - 1)
        {
            std::cout << "Something went very wrong, cannot find " << VK_EXT_DEBUG_UTILS_EXTENSION_NAME << " extension"
                      << std::endl;
            exit(1);
        }
    }

    // Create the debug callback messenger info for instance create and destroy check.
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    {
        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        debugCreateInfo.pfnUserCallback = debug_utils_messenger_callback;
    }

    // Verify that the validation layer for Khronos validation is supported
    uint32_t layerNum;
    VK_CHECK(vkEnumerateInstanceLayerProperties(&layerNum, nullptr));
    assert(layerNum >= 1);
    std::vector<VkLayerProperties> layers(layerNum);
    VK_CHECK(vkEnumerateInstanceLayerProperties(&layerNum, layers.data()));
    for (int i = 0; i < layerNum; ++i)
    {
        if(strcmp("VK_LAYER_KHRONOS_validation", layers[i].layerName) == 0)
        {
            break;
        }
        if(i == layerNum - 1)
        {
            std::cout << "Something went very wrong, cannot find VK_LAYER_KHRONOS_validation extension" << std::endl;
            exit(1);
        }
    }

    // Initialize instance and application
    VkApplicationInfo appInfo{}; // TIPS: You can delete this bracket to see what happens.
    {
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "InitDevice";
        appInfo.applicationVersion = 1;
        appInfo.pEngineName = "VulkanDict";
        appInfo.engineVersion = 1;
        appInfo.apiVersion = VK_API_VERSION_1_1;
    }

    const char* extensionName = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
    VkInstanceCreateInfo instanceCreateInfo{};
    {
        instanceCreateInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceCreateInfo.pNext = &debugCreateInfo;
        instanceCreateInfo.pApplicationInfo = &appInfo;
        instanceCreateInfo.enabledExtensionCount = 1;
        instanceCreateInfo.ppEnabledExtensionNames = &extensionName;
        instanceCreateInfo.enabledLayerCount = 1;
        instanceCreateInfo.ppEnabledLayerNames = &validationLayerName;
    }
    VkInstance instance;
    VK_CHECK(vkCreateInstance(&instanceCreateInfo, nullptr, &instance));

    // Create debug messenger
    VkDebugUtilsMessengerEXT debugMessenger;
    auto fpVkCreateDebugUtilsMessengerExt = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if(fpVkCreateDebugUtilsMessengerExt == nullptr)
    {
        exit(1);
    }
    VK_CHECK(fpVkCreateDebugUtilsMessengerExt(instance, &debugCreateInfo, nullptr, &debugMessenger));

    // Enumerate the physicalDevices, select the first one and display the name of it.
    uint32_t phyDeviceCount;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &phyDeviceCount, nullptr));
    assert(phyDeviceCount >= 1);
    std::vector<VkPhysicalDevice> phyDeviceVec(phyDeviceCount);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &phyDeviceCount, phyDeviceVec.data()));
    VkPhysicalDevice physicalDevice = phyDeviceVec[0];
    VkPhysicalDeviceProperties physicalDevProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &physicalDevProperties);
    std::cout << "Device name:" << physicalDevProperties.deviceName << std::endl;

    // Enumerate this physical device's memory properties
    VkPhysicalDeviceMemoryProperties phyDeviceMemProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &phyDeviceMemProps);

    // Initialize the device with graphics queue
    uint32_t queueFamilyPropCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropCount, nullptr);
    assert(queueFamilyPropCount >= 1);
    std::vector<VkQueueFamilyProperties> queueFamilyProps(queueFamilyPropCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropCount, queueFamilyProps.data());

    bool found = false;
    unsigned int queueFamilyIdx = -1;
    for (unsigned int i = 0; i < queueFamilyPropCount; ++i)
    {
        if(queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            queueFamilyIdx = i;
            found = true;
            break;
        }
    }
    assert(found);

    float queue_priorities[1] = {0.0};
    VkDeviceQueueCreateInfo queueInfo{};
    {
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = queueFamilyIdx;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = queue_priorities;
    }

    VkDeviceCreateInfo deviceInfo{};
    {
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.pQueueCreateInfos = &queueInfo;
    }
    VkDevice device;
    VK_CHECK(vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device));

    // Create Image
    VkFormat colorImgFormat = VK_FORMAT_R8G8B8A8_UNORM;
    VkImageTiling colorBufTiling;
    {
        VkFormatProperties fProps;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, colorImgFormat, &fProps);
        if(fProps.linearTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT )
        {
            colorBufTiling = VK_IMAGE_TILING_LINEAR;
        }
        else
        {
            std::cout << "VK_FORMAT_R8G8B8A8_UNORM Unsupported." << std::endl;
            exit(1);
        }
    }
    VkImageCreateInfo imageInfo{};
    {
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = colorImgFormat;
        imageInfo.extent.width = 960;
        imageInfo.extent.height = 680;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.tiling = colorBufTiling;
    }
    VkImage colorImage;
    VK_CHECK(vkCreateImage(device, &imageInfo, nullptr, &colorImage));

    // Allocate memory for the image object
    VkMemoryRequirements imageMemReqs;
    vkGetImageMemoryRequirements(device, colorImage, &imageMemReqs);

    bool memMatch = false;
    uint32_t memTypeIdx = 0;
    VkMemoryPropertyFlagBits reqMemProp = static_cast<VkMemoryPropertyFlagBits>(
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    for (int i = 0; i < phyDeviceMemProps.memoryTypeCount; ++i)
    {
        if(imageMemReqs.memoryTypeBits & (1 << i))
        {
            if(phyDeviceMemProps.memoryTypes[i].propertyFlags & reqMemProp)
            {
                memTypeIdx = i;
                memMatch = true;
                break;
            }
        }
    }
    assert(memMatch);

    VkMemoryAllocateInfo memAlloc{};
    {
        memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memAlloc.allocationSize = imageMemReqs.size;
        memAlloc.memoryTypeIndex = memTypeIdx;
    }
    VkDeviceMemory imageMem;
    VK_CHECK(vkAllocateMemory(device, &memAlloc, nullptr, &imageMem));

    // Bind the image object and the memory together
    VK_CHECK(vkBindImageMemory(device, colorImage, imageMem, 0));

    // Create the image view
    VkImageViewCreateInfo imageViewInfo{};
    {
        imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewInfo.image = colorImage;
        imageViewInfo.format = colorImgFormat;
        imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewInfo.subresourceRange.baseMipLevel = 0;
        imageViewInfo.subresourceRange.levelCount = 1;
        imageViewInfo.subresourceRange.baseArrayLayer = 0;
        imageViewInfo.subresourceRange.layerCount = 1;
    }
    VkImageView colorImgView;
    VK_CHECK(vkCreateImageView(device, &imageViewInfo, nullptr, &colorImgView));

    // Create depth buffer
    VkFormat depthImgFormat = VK_FORMAT_D32_SFLOAT;
    VkImageTiling depthBufTiling;
    {
        VkFormatProperties fProps;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, depthImgFormat, &fProps);
        if(fProps.linearTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            depthBufTiling = VK_IMAGE_TILING_LINEAR;
        }
        else if (fProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            depthBufTiling = VK_IMAGE_TILING_OPTIMAL;
        }
        else
        {
            std::cout << "VK_FORMAT_D32_SFLOAT Unsupported." << std::endl;
            exit(1);
        }
    }
    VkImageCreateInfo depthImgInfo{};
    {
        depthImgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        depthImgInfo.imageType = VK_IMAGE_TYPE_2D;
        depthImgInfo.format = depthImgFormat;
        depthImgInfo.extent.width = 960;
        depthImgInfo.extent.height = 680;
        depthImgInfo.extent.depth = 1;
        depthImgInfo.mipLevels = 1;
        depthImgInfo.arrayLayers = 1;
        depthImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        depthImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthImgInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        depthImgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        depthImgInfo.tiling = depthBufTiling;
    }
    VkImage depthImage;
    VK_CHECK(vkCreateImage(device, &depthImgInfo, nullptr, &depthImage));

    // Allocate memory the depth image object
    VkMemoryRequirements depthImgMemReqs;
    vkGetImageMemoryRequirements(device, depthImage, &depthImgMemReqs);

    bool depthMemMatch = false;
    uint32_t depthTypeIdx = 0;
    for (int i = 0; i < phyDeviceMemProps.memoryTypeCount; ++i)
    {
        if(depthImgMemReqs.memoryTypeBits & (1 << i))
        {
            if(phyDeviceMemProps.memoryTypes[i].propertyFlags & reqMemProp)
            {
                depthMemMatch = true;
                depthTypeIdx = i;
            }
        }
    }
    assert(depthMemMatch);

    VkMemoryAllocateInfo depthMemAllocInfo{};
    {
        depthMemAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        depthMemAllocInfo.memoryTypeIndex = depthTypeIdx;
        depthMemAllocInfo.allocationSize = depthImgMemReqs.size;
    }
    VkDeviceMemory depthImgMem;
    VK_CHECK(vkAllocateMemory(device, &depthMemAllocInfo, nullptr, &depthImgMem));

    // Bind the depth image object and the memory together
    VK_CHECK(vkBindImageMemory(device, depthImage, depthImgMem, 0));

    // Create the depth image view
    VkImageViewCreateInfo depthImgViewInfo{};
    {
        depthImgViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        depthImgViewInfo.image = depthImage;
        depthImgViewInfo.format = depthImgFormat;
        depthImgViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthImgViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        depthImgViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        depthImgViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        depthImgViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        depthImgViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthImgViewInfo.subresourceRange.baseMipLevel = 0;
        depthImgViewInfo.subresourceRange.levelCount = 1;
        depthImgViewInfo.subresourceRange.baseArrayLayer = 0;
        depthImgViewInfo.subresourceRange.layerCount = 1;
    }
    VkImageView depthImgView;
    VK_CHECK(vkCreateImageView(device, &depthImgViewInfo, nullptr, &depthImgView));

    // Create attachment descriptions for the color image and the depth image
    // 0: color image attachment; 1: depth buffer attachment.
    VkAttachmentDescription attachments[2];
    {
        attachments[0].format = colorImgFormat;
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
        attachments[0].flags = 0;

        attachments[1].format = depthImgFormat;
        attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
        attachments[1].flags = 0;
    }

    // Create color reference
    VkAttachmentReference colorReference{};
    {
        colorReference.attachment = 0;
        colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    // Create depth reference
    VkAttachmentReference depthReference{};
    {
        depthReference.attachment = 1;
        depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    // Create a subpass for color and depth rendering
    VkSubpassDescription subpass{};
    {
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.flags = 0;
        subpass.inputAttachmentCount = 0;
        subpass.pInputAttachments = nullptr;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorReference;
        subpass.pResolveAttachments = nullptr;
        subpass.pDepthStencilAttachment = &depthReference;
        subpass.preserveAttachmentCount = 0;
        subpass.pPreserveAttachments = nullptr;
    }

    // Create the render pass
    VkRenderPassCreateInfo renderPassCreateInfo{};
    {
        renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassCreateInfo.attachmentCount = 2;
        renderPassCreateInfo.pAttachments = attachments;
        renderPassCreateInfo.subpassCount = 1;
        renderPassCreateInfo.pSubpasses = &subpass;
    }
    VkRenderPass renderPass;
    VK_CHECK(vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass));

    // Create the frame buffer
    VkImageView attachmentsViews[2] = {colorImgView, depthImgView};
    VkFramebufferCreateInfo framebufferCreateInfo{};
    {
        framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.renderPass = renderPass;
        framebufferCreateInfo.attachmentCount = 2;
        framebufferCreateInfo.pAttachments = attachmentsViews;
        framebufferCreateInfo.width = 960;
        framebufferCreateInfo.height = 680;
        framebufferCreateInfo.layers = 1;
    }
    VkFramebuffer frameBuffer;
    VK_CHECK(vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &frameBuffer));

    // Destroy the frame buffer
    vkDestroyFramebuffer(device, frameBuffer, nullptr);

    // Destroy the render pass
    vkDestroyRenderPass(device, renderPass, nullptr);

    // Destroy the depth image view
    vkDestroyImageView(device, depthImgView, nullptr);

    // Destroy the depth image
    vkDestroyImage(device, depthImage, nullptr);

    // Free the memory backing the depth image object
    vkFreeMemory(device, depthImgMem, nullptr);

    // Destroy the image view
    vkDestroyImageView(device, colorImgView, nullptr);

    // Destroy the image
    vkDestroyImage(device, colorImage, nullptr);

    // Free the memory backing the image object
    vkFreeMemory(device, imageMem, nullptr);

    // Destroy the device
    vkDestroyDevice(device, nullptr);

    // Destroy debug messenger
    auto fpVkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if(fpVkDestroyDebugUtilsMessengerEXT == nullptr)
    {
        exit(1);
    }
    fpVkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);

    // Destroy instance
    vkDestroyInstance(instance, nullptr);
}
