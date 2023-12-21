#include <iostream>
#include <vector>
#include <fstream>
#include <cassert>
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include "vulkan/vulkan.h"
#include "lodepng.h"
#include "renderdoc_app.h"
#include <Windows.h>
#include <cassert>

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

// pos1, pos2, pos3
float verts[] = {
    -0.25f,  0.25f, 0.f, 1.f, // v0 - Bottom Left
     0.25f,  0.25f, 0.f, 1.f, // v1 - Bottom Right
     0.0f,  -0.25f, 0.f, 1.f  // v2 - Top
};

// CCW
// v0 - v1 - v2; v2 - v3 - v0;
uint32_t vertIdx[] = {
    0, 1, 2
};

// color - r, color - g, color - b, color - a (padding), offset - x, offset - y.
float colorStorageData[] = {
    1.f, 0.f, 0.f, 1.f, // Upper-Left red triangle.
    0.f, 1.f, 0.f, 1.f, // Upper-Right green triangle.
    0.f, 0.f, 1.f, 1.f, // Bottom-Left blue triangle.
    1.f, 1.f, 0.f, 1.f  // Bottom-Right yellow triangle.
};

float offsetsStorageData[] = {
    -0.25f, -0.25f, // Upper-Left red triangle (offset).
     0.25f, -0.25f, // Upper-Right green triangle (offset).
    -0.25f,  0.25f, // Bottom-Left blue triangle (offset).
     0.25f,  0.25f  // Bottom-Right yellow triangle (offset).
};


VkDrawIndexedIndirectCommand indirectDrawIdxCmd = {
    3, 4, 0, 0, 0
};

int main()
{
    // RenderDoc debug starts
    RENDERDOC_API_1_6_0* rdoc_api = NULL;
    if (HMODULE mod = GetModuleHandleA("renderdoc.dll"))
    {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
        int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0, (void**)&rdoc_api);
        assert(ret == 1);
    }

    if (rdoc_api)
    {
        rdoc_api->StartFrameCapture(NULL, NULL);
    }

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
        appInfo.pApplicationName = "DrawIndirectDump4Tris";
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
    allDeviceExtensions.push_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);

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

    // Get the push descriptor function pointer
    PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR = (PFN_vkCmdPushDescriptorSetKHR)vkGetDeviceProcAddr(device, "vkCmdPushDescriptorSetKHR");
    if (!vkCmdPushDescriptorSetKHR) {
        exit(1);
    }

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

    // Create Vertex Buffer
    VkBufferCreateInfo vertBufferInfo = {};
    {
        vertBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        vertBufferInfo.size = sizeof(verts);
        vertBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }
    VkBuffer vertBuf;
    VmaAllocation vertAlloc;
    VK_CHECK(vmaCreateBuffer(vmaAllocator, &vertBufferInfo, &mappableBufCreateInfo, &vertBuf, &vertAlloc, nullptr));
    VK_CHECK(vmaMapMemory(vmaAllocator, vertAlloc, &mappedData));
    memcpy(mappedData, verts, sizeof(verts));
    vmaUnmapMemory(vmaAllocator, vertAlloc);

    // Create Index Buffer
    VkBufferCreateInfo idxBufferInfo = {};
    {
        idxBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        idxBufferInfo.size = sizeof(vertIdx);
        idxBufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }
    VkBuffer idxBuf;
    VmaAllocation idxAlloc;
    VK_CHECK(vmaCreateBuffer(vmaAllocator, &idxBufferInfo, &mappableBufCreateInfo, &idxBuf, &idxAlloc, nullptr));
    VK_CHECK(vmaMapMemory(vmaAllocator, idxAlloc, &mappedData));
    memcpy(mappedData, vertIdx, sizeof(vertIdx));
    vmaUnmapMemory(vmaAllocator, idxAlloc);

    // Create the storage buffers.
    VkBufferCreateInfo offsetStorageBufferInfo = {};
    {
        offsetStorageBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        offsetStorageBufferInfo.size = sizeof(offsetsStorageData);
        offsetStorageBufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }
    VkBuffer offsetStorageBuffer;
    VmaAllocation offsetStorageBufferAlloc;
    VK_CHECK(vmaCreateBuffer(vmaAllocator, &offsetStorageBufferInfo, &mappableBufCreateInfo, &offsetStorageBuffer, &offsetStorageBufferAlloc, nullptr));
    VK_CHECK(vmaMapMemory(vmaAllocator, offsetStorageBufferAlloc, &mappedData));
    memcpy(mappedData, offsetsStorageData, sizeof(offsetsStorageData));
    vmaUnmapMemory(vmaAllocator, offsetStorageBufferAlloc);

    VkDescriptorBufferInfo offsetSSBODescInfo{};
    {
        offsetSSBODescInfo.buffer = offsetStorageBuffer;
        offsetSSBODescInfo.offset = 0;
        offsetSSBODescInfo.range = sizeof(offsetsStorageData);
    }

    VkBufferCreateInfo colorStorageBufferInfo = {};
    {
        colorStorageBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        colorStorageBufferInfo.size = sizeof(colorStorageData);
        colorStorageBufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }
    VkBuffer colorStorageBuffer;
    VmaAllocation colorStorageBufferAlloc;
    VK_CHECK(vmaCreateBuffer(vmaAllocator, &colorStorageBufferInfo, &mappableBufCreateInfo, &colorStorageBuffer, &colorStorageBufferAlloc, nullptr));
    VK_CHECK(vmaMapMemory(vmaAllocator, colorStorageBufferAlloc, &mappedData));
    memcpy(mappedData, colorStorageData, sizeof(colorStorageData));
    vmaUnmapMemory(vmaAllocator, colorStorageBufferAlloc);

    VkDescriptorBufferInfo colorSSBODescInfo{};
    {
        colorSSBODescInfo.buffer = colorStorageBuffer;
        colorSSBODescInfo.offset = 0;
        colorSSBODescInfo.range = sizeof(colorStorageData);
    }

    // Create the indirect draw command buffer.
    VkBufferCreateInfo indirectDrawIdxCmdBufferInfo = {};
    {
        indirectDrawIdxCmdBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        indirectDrawIdxCmdBufferInfo.size = sizeof(indirectDrawIdxCmd);
        indirectDrawIdxCmdBufferInfo.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    }
    VkBuffer indirectDrawCmdBuffer;
    VmaAllocation indirectDrawCmdBufferAlloc;
    VK_CHECK(vmaCreateBuffer(vmaAllocator, &indirectDrawIdxCmdBufferInfo, &mappableBufCreateInfo, &indirectDrawCmdBuffer, &indirectDrawCmdBufferAlloc, nullptr));
    VK_CHECK(vmaMapMemory(vmaAllocator, indirectDrawCmdBufferAlloc, &mappedData));
    memcpy(mappedData, &indirectDrawIdxCmd, sizeof(indirectDrawIdxCmd));
    vmaUnmapMemory(vmaAllocator, indirectDrawCmdBufferAlloc);

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
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.tiling = colorBufTiling;
    }

    VkImage colorImage;
    VmaAllocation imgAlloc;
    vmaCreateImage(vmaAllocator, &imageInfo, &mappableBufCreateInfo, &colorImage, &imgAlloc, nullptr);

    // Create the image view
    VkImageSubresourceRange colorTargetSubRsrcRange{};
    {
        colorTargetSubRsrcRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorTargetSubRsrcRange.baseMipLevel = 0;
        colorTargetSubRsrcRange.levelCount = 1;
        colorTargetSubRsrcRange.baseArrayLayer = 0;
        colorTargetSubRsrcRange.layerCount = 1;
    }

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
        imageViewInfo.subresourceRange = colorTargetSubRsrcRange;
    }
    VkImageView colorImgView;
    VK_CHECK(vkCreateImageView(device, &imageViewInfo, nullptr, &colorImgView));

    VkBufferCreateInfo colorImgStagingBufferInfo = {};
    {
        colorImgStagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        colorImgStagingBufferInfo.size = imgAlloc->GetSize();
        colorImgStagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }
    VkBuffer colorImgStagingBuffer;
    VmaAllocation colorImgStagingBufferAlloc;
    VK_CHECK(vmaCreateBuffer(vmaAllocator, &colorImgStagingBufferInfo, &mappableBufCreateInfo, &colorImgStagingBuffer, &colorImgStagingBufferAlloc, nullptr));

    // Create Vert Shader Module -- SOURCE_PATH is a MACRO definition passed in during compilation, which is specified in
    //                              the CMakeLists.txt file in the same level of repository.
    std::string shaderVertPath = std::string(SOURCE_PATH) + std::string("/hlsl/tri_vert.spv");
    std::ifstream inputVertShader(shaderVertPath.c_str(), std::ios::binary | std::ios::in);
    std::vector<unsigned char> inputVertShaderStr(std::istreambuf_iterator<char>(inputVertShader), {});
    inputVertShader.close();
    VkShaderModuleCreateInfo shaderVertModuleCreateInfo{};
    {
        shaderVertModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderVertModuleCreateInfo.codeSize = inputVertShaderStr.size();
        shaderVertModuleCreateInfo.pCode = (uint32_t*) inputVertShaderStr.data();
    }
    VkShaderModule shaderVertModule;
    VK_CHECK(vkCreateShaderModule(device, &shaderVertModuleCreateInfo, nullptr, &shaderVertModule));

    // Create Vert Shader Stage create info
    VkPipelineShaderStageCreateInfo vertStgInfo{};
    {
        vertStgInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertStgInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertStgInfo.pName = "main";
        vertStgInfo.module = shaderVertModule;
    }

    // Create Frag Shader Module -- SOURCE_PATH is a MACRO definition passed in during compilation, which is specified in
    //                              the CMakeLists.txt file in the same level of repository.
    std::string shaderFragPath = std::string(SOURCE_PATH) + std::string("/hlsl/tri_frag.spv");
    std::ifstream inputFragShader(shaderFragPath.c_str(), std::ios::binary | std::ios::in);
    std::vector<unsigned char> inputFragShaderStr(std::istreambuf_iterator<char>(inputFragShader), {});
    inputFragShader.close();
    VkShaderModuleCreateInfo shaderFragModuleCreateInfo{};
    {
        shaderFragModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderFragModuleCreateInfo.codeSize = inputFragShaderStr.size();
        shaderFragModuleCreateInfo.pCode = (uint32_t*) inputFragShaderStr.data();
    }
    VkShaderModule shaderFragModule;
    VK_CHECK(vkCreateShaderModule(device, &shaderFragModuleCreateInfo, nullptr, &shaderFragModule));

    // Create frag shader stage create info
    VkPipelineShaderStageCreateInfo fragStgInfo{};
    {
        fragStgInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStgInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStgInfo.pName = "main";
        fragStgInfo.module = shaderFragModule;
    }

    // Combine shader stages info into an array
    VkPipelineShaderStageCreateInfo stgArray[] = {vertStgInfo, fragStgInfo};

    // Specifying all kinds of pipeline states
    // Vertex input state
    VkVertexInputBindingDescription vertBindingDesc = {};
    {
        vertBindingDesc.binding = 0;
        // vertBindingDesc.stride = 3 * sizeof(float);
        vertBindingDesc.stride = 4 * sizeof(float);
        vertBindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    }

    VkVertexInputAttributeDescription vertAttrDesc;
    {
        // Position
        vertAttrDesc.location = 0;
        vertAttrDesc.binding = 0;
        // vertAttrDesc.format = VK_FORMAT_R32G32B32_SFLOAT;
        vertAttrDesc.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        vertAttrDesc.offset = 0;
    }
    VkPipelineVertexInputStateCreateInfo vertInputInfo{};
    {
        vertInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertInputInfo.pNext = nullptr;
        vertInputInfo.vertexBindingDescriptionCount = 1;
        vertInputInfo.pVertexBindingDescriptions = &vertBindingDesc;
        vertInputInfo.vertexAttributeDescriptionCount = 1;
        vertInputInfo.pVertexAttributeDescriptions = &vertAttrDesc;
    }

    // Vertex assembly state
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
    {
        inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;
        inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }

    // Rasterization state
    VkPipelineRasterizationStateCreateInfo rasterizationInfo{};
    {
        rasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizationInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizationInfo.depthClampEnable = VK_FALSE;
        rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
        rasterizationInfo.depthBiasEnable = VK_FALSE;
        rasterizationInfo.depthBiasClamp = 0;
        rasterizationInfo.depthBiasSlopeFactor = 0;
        rasterizationInfo.lineWidth = 1.f;
    }

    // Color blend state
    VkPipelineColorBlendAttachmentState cbAttState{};
    {
        cbAttState.colorWriteMask = 0xf;
        cbAttState.blendEnable = VK_FALSE;
        cbAttState.alphaBlendOp = VK_BLEND_OP_ADD;
        cbAttState.colorBlendOp = VK_BLEND_OP_ADD;
        cbAttState.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        cbAttState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        cbAttState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        cbAttState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    }
    VkPipelineColorBlendStateCreateInfo colorBlendInfo{};
    {
        colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendInfo.attachmentCount = 1;
        colorBlendInfo.pAttachments = &cbAttState;
        colorBlendInfo.logicOpEnable = VK_FALSE;
        colorBlendInfo.blendConstants[0] = 1.f;
        colorBlendInfo.blendConstants[1] = 1.f;
        colorBlendInfo.blendConstants[2] = 1.f;
        colorBlendInfo.blendConstants[3] = 1.f;
    }

    // Viewport state
    VkViewport vp{};
    {
        vp.width  = 960;
        vp.height = 680;
        vp.maxDepth = 1.f;
        vp.minDepth = 0.f;
        vp.x = 0;
        vp.y = 0;
    }
    VkRect2D scissor{};
    {
        scissor.extent.width = 960;
        scissor.extent.height = 680;
        scissor.offset.x = 0;
        scissor.offset.y = 0;
    }
    VkPipelineViewportStateCreateInfo vpStateInfo{};
    {
        vpStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vpStateInfo.viewportCount = 1;
        vpStateInfo.pViewports = &vp;
        vpStateInfo.scissorCount = 1;
        vpStateInfo.pScissors = &scissor;
    }

    // Depth stencil state
    VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};
    {
        depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencilInfo.depthTestEnable = VK_TRUE;
        depthStencilInfo.depthWriteEnable = VK_TRUE;
        depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
        depthStencilInfo.stencilTestEnable = VK_FALSE;
    }

    // Multisample state
    VkPipelineMultisampleStateCreateInfo multiSampleInfo{};
    {
        multiSampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multiSampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multiSampleInfo.sampleShadingEnable = VK_FALSE;
        multiSampleInfo.alphaToCoverageEnable = VK_FALSE;
        multiSampleInfo.alphaToOneEnable = VK_FALSE;
    }

    VkDescriptorSetLayoutBinding offsetsSSBOBinding{};
    {
        offsetsSSBOBinding.binding = 0;
        offsetsSSBOBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        offsetsSSBOBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        offsetsSSBOBinding.descriptorCount = 1;
    }

    VkDescriptorSetLayoutBinding colorSSBOBinding{};
    {
        colorSSBOBinding.binding = 1;
        colorSSBOBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        colorSSBOBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        colorSSBOBinding.descriptorCount = 1;
    }

    VkDescriptorSetLayoutBinding pipelineDesSetLayoutBindings[2] = { offsetsSSBOBinding, colorSSBOBinding };
    VkDescriptorSetLayoutCreateInfo pipelineDesSetLayoutInfo{};
    {
        pipelineDesSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        // Setting this flag tells the descriptor set layouts that no actual descriptor sets are allocated but instead pushed at command buffer creation time
        pipelineDesSetLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
        pipelineDesSetLayoutInfo.bindingCount = 2;
        pipelineDesSetLayoutInfo.pBindings = pipelineDesSetLayoutBindings;
    }

    VkDescriptorSetLayout descSetLayout;
    VK_CHECK(vkCreateDescriptorSetLayout(device,
                                         &pipelineDesSetLayoutInfo,
                                         nullptr,
                                         &descSetLayout));

    // Create a pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    {
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descSetLayout;
    }
    VkPipelineLayout pipelineLayout{};
    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout));

    VkPipelineRenderingCreateInfoKHR pipelineRenderCreateInfo{};
    {
        pipelineRenderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        pipelineRenderCreateInfo.colorAttachmentCount = 1;
        pipelineRenderCreateInfo.pColorAttachmentFormats = &colorImgFormat;
    }

    // Create the graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    {
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.pNext = &pipelineRenderCreateInfo;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.pVertexInputState = &vertInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
        pipelineInfo.pRasterizationState = &rasterizationInfo;
        pipelineInfo.pColorBlendState = &colorBlendInfo;
        pipelineInfo.pTessellationState = nullptr;
        pipelineInfo.pMultisampleState = &multiSampleInfo;
        pipelineInfo.pViewportState = &vpStateInfo;
        pipelineInfo.pDepthStencilState = &depthStencilInfo;
        pipelineInfo.pStages = stgArray;
        pipelineInfo.stageCount = 2;
        pipelineInfo.renderPass = nullptr;
        pipelineInfo.subpass = 0;
    }
    VkPipeline pipeline;
    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));

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

    // Fill command buffers for rendering and submit them to a queue.
    VkCommandBufferBeginInfo cmdBufInfo{};
    {
        cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    }

    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));

    VkClearValue clearVal{};
    {
        clearVal.color = {{0.f, 0.f, 0.f, 1.f}};
    }

    VkRenderingAttachmentInfoKHR colorAttachmentInfo{};
    {
        colorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
        colorAttachmentInfo.imageView = colorImgView;
        colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
        colorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachmentInfo.clearValue = clearVal;
    }

    VkRenderingInfoKHR renderInfo{};
    {
        renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
        renderInfo.renderArea.offset = { 0, 0 };
        renderInfo.renderArea.extent = { 960, 680 };
        renderInfo.layerCount = 1;
        renderInfo.colorAttachmentCount = 1;
        renderInfo.pColorAttachments = &colorAttachmentInfo;
    }

    // Transfer the color image to color attachment
    VkImageMemoryBarrier undefToColorAttachmentBarrier{};
    {
        undefToColorAttachmentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        undefToColorAttachmentBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        undefToColorAttachmentBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        undefToColorAttachmentBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        undefToColorAttachmentBarrier.image = colorImage;
        undefToColorAttachmentBarrier.subresourceRange = colorTargetSubRsrcRange;
    }

    vkCmdPipelineBarrier(
        cmdBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &undefToColorAttachmentBarrier);

    vkCmdBeginRendering(cmdBuffer, &renderInfo);

    vkCmdSetViewport(cmdBuffer, 0, 1, &vp);

    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

    std::vector<VkWriteDescriptorSet> writeDescriptorSet0;
    VkWriteDescriptorSet writeOffsetSSBODescSet{};
    {
        writeOffsetSSBODescSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeOffsetSSBODescSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writeOffsetSSBODescSet.dstBinding = 0;
        writeOffsetSSBODescSet.descriptorCount = 1;
        writeOffsetSSBODescSet.pBufferInfo = &offsetSSBODescInfo;
    }
    writeDescriptorSet0.push_back(writeOffsetSSBODescSet);

    VkWriteDescriptorSet writeColorSSBODescSet{};
    {
        writeColorSSBODescSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeColorSSBODescSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writeColorSSBODescSet.dstBinding = 1;
        writeColorSSBODescSet.descriptorCount = 1;
        writeColorSSBODescSet.pBufferInfo = &colorSSBODescInfo;
    }
    writeDescriptorSet0.push_back(writeColorSSBODescSet);

    vkCmdPushDescriptorSetKHR(cmdBuffer,
                              VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipelineLayout,
                              0, writeDescriptorSet0.size(), writeDescriptorSet0.data());

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    VkDeviceSize vbOffset = 0;
    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vertBuf, &vbOffset);
    vkCmdBindIndexBuffer(cmdBuffer, idxBuf, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexedIndirect(cmdBuffer, indirectDrawCmdBuffer, 0, 1, sizeof(indirectDrawIdxCmd));

    vkCmdEndRendering(cmdBuffer);

    // Transfer the color image to general
    VkImageMemoryBarrier colorAttachmentToGeneralBarrier{};
    {
        colorAttachmentToGeneralBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        colorAttachmentToGeneralBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        colorAttachmentToGeneralBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        colorAttachmentToGeneralBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachmentToGeneralBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        colorAttachmentToGeneralBarrier.image = colorImage;
        colorAttachmentToGeneralBarrier.subresourceRange = colorTargetSubRsrcRange;
    }

    vkCmdPipelineBarrier(
        cmdBuffer,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &colorAttachmentToGeneralBarrier);

    VkBufferImageCopy imgToBufferCopyInfo{};
    {
        imgToBufferCopyInfo.bufferRowLength = 960;
        imgToBufferCopyInfo.imageSubresource.aspectMask = colorTargetSubRsrcRange.aspectMask;
        imgToBufferCopyInfo.imageSubresource.baseArrayLayer = colorTargetSubRsrcRange.baseArrayLayer;
        imgToBufferCopyInfo.imageSubresource.layerCount = colorTargetSubRsrcRange.layerCount;
        imgToBufferCopyInfo.imageSubresource.mipLevel = colorTargetSubRsrcRange.baseMipLevel;
        imgToBufferCopyInfo.imageExtent = { 960, 680, 1 };
    }

    vkCmdCopyImageToBuffer(cmdBuffer,
                           colorImage,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           colorImgStagingBuffer,
                           1, &imgToBufferCopyInfo);

    VK_CHECK(vkEndCommandBuffer(cmdBuffer));

    // Command buffer submit info
    VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

    VkSubmitInfo submitInfo{};
    {
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pWaitDstStageMask = &waitStageMask;
        submitInfo.pCommandBuffers = &cmdBuffer;
        submitInfo.commandBufferCount = 1;
    }
    VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, fence));

    // Wait for the fence to signal that command buffer has finished executing
    VK_CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));
    VK_CHECK(vkQueueWaitIdle(graphicsQueue));

    if (rdoc_api)
    {
        rdoc_api->EndFrameCapture(NULL, NULL);
    }

    // Make rendered image visible to the host
    void* mapped = nullptr;
    vmaMapMemory(vmaAllocator, colorImgStagingBufferAlloc, &mapped);

    // Copy to RAM
    std::vector<unsigned char> imageRAM(imgAlloc->GetSize());
    memcpy(imageRAM.data(), mapped, imgAlloc->GetSize());
    vmaUnmapMemory(vmaAllocator, colorImgStagingBufferAlloc);

    std::string pathName = std::string(SOURCE_PATH) + std::string("/test.png");
    std::cout << pathName << std::endl;
    unsigned int error = lodepng::encode(pathName, imageRAM, 960, 680);
    if(error){std::cout << "encoder error " << error << ": "<< lodepng_error_text(error) << std::endl;}

    // Destroy the fence
    vkDestroyFence(device, fence, nullptr);

    // Free Command Buffer
    vkFreeCommandBuffers(device, cmdPool, 1, &cmdBuffer);

    // Destroy the Command Pool
    vkDestroyCommandPool(device, cmdPool, nullptr);

    // Destroy Pipeline
    vkDestroyPipeline(device, pipeline, nullptr);

    // Destroy descriptor set layout
    vkDestroyDescriptorSetLayout(device, descSetLayout, nullptr);

    // Destroy the pipeline layout
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

    // Destroy both of the shader modules
    vkDestroyShaderModule(device, shaderVertModule, nullptr);
    vkDestroyShaderModule(device, shaderFragModule, nullptr);

    // Destroy the image view
    vkDestroyImageView(device, colorImgView, nullptr);

    // Destroy the image
    vmaDestroyImage(vmaAllocator, colorImage, imgAlloc);
    vmaDestroyBuffer(vmaAllocator, colorImgStagingBuffer, colorImgStagingBufferAlloc);

    // Destroy the vertex buffer
    vmaDestroyBuffer(vmaAllocator, vertBuf, vertAlloc);

    // Destroy the index buffer
    vmaDestroyBuffer(vmaAllocator, idxBuf, idxAlloc);

    // Destroy other GPU buffers.
    vmaDestroyBuffer(vmaAllocator, offsetStorageBuffer, offsetStorageBufferAlloc);
    vmaDestroyBuffer(vmaAllocator, colorStorageBuffer, colorStorageBufferAlloc);
    vmaDestroyBuffer(vmaAllocator, indirectDrawCmdBuffer, indirectDrawCmdBufferAlloc);

    // Destroy the vmaAllocator
    vmaDestroyAllocator(vmaAllocator);

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
