//
// Created by Jerry on 11/28/2021.
//
#include "vulkan/vulkan.h"
#include <vector>
#include <iostream>

int main()
{
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
        instanceCreateInfo.pApplicationInfo = &appInfo;
    }
    VkInstance instance;
    vkCreateInstance(&instanceCreateInfo, nullptr, &instance);

    // Enumerate the physicalDevices, select the first one and display the name of it.
    uint32_t phyDeviceCount;
    vkEnumeratePhysicalDevices(instance, &phyDeviceCount, nullptr);
    std::vector<VkPhysicalDevice> phyDeviceVec(phyDeviceCount);
    vkEnumeratePhysicalDevices(instance, &phyDeviceCount, phyDeviceVec.data());
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
    std::vector<VkQueueFamilyProperties> queueFamilyProps(queueFamilyPropCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropCount, queueFamilyProps.data());

    unsigned int queueFamilyIdx = -1;
    for (unsigned int i = 0; i < queueFamilyPropCount; ++i)
    {
        if(queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            queueFamilyIdx = i;
            break;
        }
    }

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
    vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device);

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
        else if(fProps.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT )
        {
            colorBufTiling = VK_IMAGE_TILING_OPTIMAL;
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
    vkCreateImage(device, &imageInfo, nullptr, &colorImage);

    // Allocate memory for the image object
    VkMemoryRequirements imageMemReqs;
    vkGetImageMemoryRequirements(device, colorImage, &imageMemReqs);

    uint32_t memTypeIdx = 0;
    for (int i = 0; i < phyDeviceMemProps.memoryTypeCount; ++i)
    {
        if(imageMemReqs.memoryTypeBits & (1 << i))
        {
            if(phyDeviceMemProps.memoryTypes[i].propertyFlags &
               (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
            {
                memTypeIdx = i;
                break;
            }
        }
    }

    VkMemoryAllocateInfo memAlloc{};
    {
        memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memAlloc.allocationSize = imageMemReqs.size;
        memAlloc.memoryTypeIndex = memTypeIdx;
    }
    VkDeviceMemory imageMem;
    vkAllocateMemory(device, &memAlloc, nullptr, &imageMem);

    // Bind the image object and the memory together
    vkBindImageMemory(device, colorImage, imageMem, 0);

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
    vkCreateImageView(device, &imageViewInfo, nullptr, &colorImgView);

    // Destroy the image view
    vkDestroyImageView(device, colorImgView, nullptr);

    // Destroy the image
    vkDestroyImage(device, colorImage, nullptr);

    // Free the memory backing the image object
    vkFreeMemory(device, imageMem, nullptr);

    // Destroy the device
    vkDestroyDevice(device, nullptr);

    // Destroy instance
    vkDestroyInstance(instance, nullptr);
}
