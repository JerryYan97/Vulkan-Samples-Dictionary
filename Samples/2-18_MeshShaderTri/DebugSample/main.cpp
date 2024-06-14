#include <iostream>
#include <vector>
#include <fstream>
#include <cassert>
#include <vulkan/vulkan.h>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "lodepng.h"

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

float g_MeshShaderData[] = {
    -0.75f, -0.75f, 0.f, 1.f, 0.f, 0.f, // v0 - Top Left
     0.75f, -0.75f, 0.f, 0.f, 1.f, 0.f, // v1 - Top Right
     0.75f,  0.75f, 0.f, 0.f, 0.f, 1.f, // v2 - Bottom Right
    -0.75f,  0.75f, 0.f, 1.f, 1.f, 0.f  // v3 - Bottom Left
};

void main() 
{
    // Verify that the debug extension for the callback messenger is supported.
    uint32_t propNum;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &propNum, nullptr));
    assert(propNum >= 1);
    std::vector<VkExtensionProperties> props(propNum);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &propNum, props.data()));
    for (int i = 0; i < props.size(); ++i)
    {
        if (strcmp(props[i].extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
        {
            break;
        }
        if (i == propNum - 1)
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
        if (strcmp("VK_LAYER_KHRONOS_validation", layers[i].layerName) == 0)
        {
            break;
        }
        if (i == layerNum - 1)
        {
            std::cout << "Something went very wrong, cannot find VK_LAYER_KHRONOS_validation extension" << std::endl;
            exit(1);
        }
    }

    // Initialize instance and application
    VkApplicationInfo appInfo{}; // TIPS: You can delete this bracket to see what happens.
    {
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "DumpMeshShaderTri";
        appInfo.applicationVersion = 1;
        appInfo.pEngineName = "VulkanDict";
        appInfo.engineVersion = 1;
        appInfo.apiVersion = VK_API_VERSION_1_3;
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
    auto fpVkCreateDebugUtilsMessengerExt = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (fpVkCreateDebugUtilsMessengerExt == nullptr)
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
        if (queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            queueFamilyIdx = i;
            found = true;
            break;
        }
    }
    assert(found);

    float queue_priorities[1] = { 0.0 };
    VkDeviceQueueCreateInfo queueInfo{};
    {
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = queueFamilyIdx;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = queue_priorities;
    }

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeature{};
    {
        dynamicRenderingFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
        dynamicRenderingFeature.dynamicRendering = VK_TRUE;
    }

    std::vector<const char*> allDeviceExtensions;
    allDeviceExtensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
    // allDeviceExtensions.push_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);

    VkDeviceCreateInfo deviceInfo{};
    {
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.pNext = &dynamicRenderingFeature;
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        deviceInfo.enabledExtensionCount = allDeviceExtensions.size();
        deviceInfo.ppEnabledExtensionNames = allDeviceExtensions.data();
    }
    VkDevice device;
    VK_CHECK(vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device));

    // Initialize the VMA allocator
    VmaAllocator vmaAllocator;
    VmaVulkanFunctions vkFuncs = {};
    {
        vkFuncs.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
        vkFuncs.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;
    }

    VmaAllocatorCreateInfo allocCreateInfo = {};
    {
        allocCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
        allocCreateInfo.physicalDevice = physicalDevice;
        allocCreateInfo.device = device;
        allocCreateInfo.instance = instance;
        allocCreateInfo.pVulkanFunctions = &vkFuncs;
    }

    VK_CHECK(vmaCreateAllocator(&allocCreateInfo, &vmaAllocator));

    // Get a graphics queue
    VkQueue graphicsQueue;
    vkGetDeviceQueue(device, queueFamilyIdx, 0, &graphicsQueue);

    // Create Buffer and allocate memory for vertex buffer, index buffer and render target.
    VmaAllocationCreateInfo mappableBufCreateInfo = {};
    {
        mappableBufCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        mappableBufCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                      VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }

    void* mappedData = nullptr;

    // Create Mesh Shader Data Buffer
    VkBufferCreateInfo dataBufferInfo = {};
    {
        dataBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        dataBufferInfo.size  = sizeof(float) * 24;
        dataBufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }
    VkBuffer meshBuf;
    VmaAllocation meshBufAlloc;
    VK_CHECK(vmaCreateBuffer(vmaAllocator, &dataBufferInfo, &mappableBufCreateInfo, &meshBuf, &meshBufAlloc, nullptr));
    VK_CHECK(vmaMapMemory(vmaAllocator, meshBufAlloc, &mappedData));
    memcpy(mappedData, g_MeshShaderData, sizeof(float) * 24);
    vmaUnmapMemory(vmaAllocator, meshBufAlloc);

    // Create Image
    VkFormat colorImgFormat = VK_FORMAT_R8G8B8A8_UNORM;
    VkImageTiling colorBufTiling;
    {
        VkFormatProperties fProps;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, colorImgFormat, &fProps);
        if (fProps.linearTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)
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
    VmaAllocation imgAlloc;
    vmaCreateImage(vmaAllocator, &imageInfo, &mappableBufCreateInfo, &colorImage, &imgAlloc, nullptr);

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

    // Create Mesh Shader Module -- SOURCE_PATH is a MACRO definition passed in during compilation, which is specified in
    //                              the CMakeLists.txt file in the same level of repository.
    // std::string shaderVertPath = std::string(SOURCE_PATH) + std::string("/DumpQuad.vert.spv");
    // std::string shaderVertPath = std::string(SOURCE_PATH) + std::string("/DumpQuadVert.spv");
    std::string shaderMeshPath = std::string(SOURCE_PATH) + std::string("/tri.mesh.spv");
    std::ifstream inputMeshShader(shaderMeshPath.c_str(), std::ios::binary | std::ios::in);
    std::vector<unsigned char> inputMeshShaderStr(std::istreambuf_iterator<char>(inputMeshShader), {});
    inputMeshShader.close();
    VkShaderModuleCreateInfo shaderMeshModuleCreateInfo{};
    {
        shaderMeshModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderMeshModuleCreateInfo.codeSize = inputMeshShaderStr.size();
        shaderMeshModuleCreateInfo.pCode = (uint32_t*)inputMeshShaderStr.data();
    }
    VkShaderModule shaderMeshModule;
    VK_CHECK(vkCreateShaderModule(device, &shaderMeshModuleCreateInfo, nullptr, &shaderMeshModule));
    
    // Create Mesh Shader Stage create info
    VkPipelineShaderStageCreateInfo meshStgInfo{};
    {
        meshStgInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        meshStgInfo.stage = VK_SHADER_STAGE_MESH_BIT_EXT;
        meshStgInfo.pName = "main";
        meshStgInfo.module = shaderMeshModule;
    }

    // Prepare synchronization primitives
    VkFenceCreateInfo fenceCreateInfo{};
    {
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    }
    VkFence fence;
    VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &fence));

    // Create command buffers and command pool for subsequent rendering.
    // Create a command pool to allocate our command buffer from
    VkCommandPoolCreateInfo cmdPoolInfo{};
    {
        cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmdPoolInfo.queueFamilyIndex = queueFamilyIdx;
        cmdPoolInfo.flags = 0;
    }
    VkCommandPool cmdPool;
    VK_CHECK(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &cmdPool));

    // Allocate the command buffer from the command pool
    VkCommandBufferAllocateInfo cmdBufferAllocInfo{};
    {
        cmdBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdBufferAllocInfo.commandPool = cmdPool;
        cmdBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdBufferAllocInfo.commandBufferCount = 1;
    }
    VkCommandBuffer cmdBuffer;
    VK_CHECK(vkAllocateCommandBuffers(device, &cmdBufferAllocInfo, &cmdBuffer));

    // Destroy the fence
    vkDestroyFence(device, fence, nullptr);

    // Free Command Buffer
    vkFreeCommandBuffers(device, cmdPool, 1, &cmdBuffer);

    // Destroy the Command Pool
    vkDestroyCommandPool(device, cmdPool, nullptr);

    // Destroy the image view
    vkDestroyImageView(device, colorImgView, nullptr);

    // Destroy the image
    vmaDestroyImage(vmaAllocator, colorImage, imgAlloc);

    // Destroy the mesh buffer
    vmaDestroyBuffer(vmaAllocator, meshBuf, meshBufAlloc);

    // Destroy the vmaAllocator
    vmaDestroyAllocator(vmaAllocator);

    // Destroy the device
    vkDestroyDevice(device, nullptr);

    // Destroy debug messenger
    auto fpVkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (fpVkDestroyDebugUtilsMessengerEXT == nullptr)
    {
        exit(1);
    }
    fpVkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);

    // Destroy instance
    vkDestroyInstance(instance, nullptr);
}