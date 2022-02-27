//
// Created by Jerry on 11/28/2021.
//
#include <iostream>
#include <vector>
#include <cassert>
#include "vulkan/vulkan.h"

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
    assert(queueFamilyPropCount >= 1);
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
    vkCreateImage(device, &depthImgInfo, nullptr, &depthImage);

    // Allocate memory the depth image object
    VkMemoryRequirements depthImgMemReqs;
    vkGetImageMemoryRequirements(device, depthImage, &depthImgMemReqs);

    uint32_t depthTypeIdx = 0;
    for (int i = 0; i < phyDeviceMemProps.memoryTypeCount; ++i)
    {
        if(depthImgMemReqs.memoryTypeBits & (1 << i))
        {
            if(phyDeviceMemProps.memoryTypes[i].propertyFlags & reqMemProp)
            {
                depthTypeIdx = i;
            }
        }
    }

    VkMemoryAllocateInfo depthMemAllocInfo{};
    {
        depthMemAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        depthMemAllocInfo.memoryTypeIndex = depthTypeIdx;
        depthMemAllocInfo.allocationSize = depthImgMemReqs.size;
    }
    VkDeviceMemory depthImgMem;
    vkAllocateMemory(device, &depthMemAllocInfo, nullptr, &depthImgMem);

    // Bind the depth image object and the memory together
    vkBindImageMemory(device, depthImage, depthImgMem, 0);

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
    vkCreateImageView(device, &depthImgViewInfo, nullptr, &depthImgView);

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
    vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass);

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
    vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &frameBuffer);

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

    // Destroy instance
    vkDestroyInstance(instance, nullptr);
}
