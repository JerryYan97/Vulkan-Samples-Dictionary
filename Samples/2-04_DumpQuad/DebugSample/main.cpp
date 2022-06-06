//
// Created by Jerry on 11/28/2021.
//
#include <iostream>
#include <vector>
#include <fstream>
#include <cassert>
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include "vulkan/vulkan.h"
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

// pos1, pos2, pos3, col1, col2, col3
float verts[] = {
        -0.75f, -0.75f, 0.f, 1.f, 0.f, 0.f, // v0 - Top Left
        0.75f, -0.75f, 0.f, 0.f, 1.f, 0.f, // v1 - Top Right
        0.75f, 0.75f, 0.f, 0.f, 0.f, 1.f, // v2 - Bottom Right
        -0.75f, 0.75f, 0.f, 1.f, 1.f, 0.f // v3 - Bottom Left
};

// CCW
// v0 - v1 - v2; v2 - v3 - v0;
uint32_t vertIdx[] = {
    0, 1, 2, 2, 3, 0
};

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
        appInfo.pApplicationName = "DumpTri";
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

    // Initialize the VMA allocator
    VmaAllocator vmaAllocator;
    VmaVulkanFunctions vkFuncs = {};
    {
        vkFuncs.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
        vkFuncs.vkGetDeviceProcAddr   = &vkGetDeviceProcAddr;
    }

    VmaAllocatorCreateInfo allocCreateInfo = {};
    {
        allocCreateInfo.vulkanApiVersion = VK_API_VERSION_1_2;
        allocCreateInfo.physicalDevice   = physicalDevice;
        allocCreateInfo.device           = device;
        allocCreateInfo.instance         = instance;
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
        vertBufferInfo.size = sizeof(float) * 24;
        vertBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }
    VkBuffer vertBuf;
    VmaAllocation vertAlloc;
    VK_CHECK(vmaCreateBuffer(vmaAllocator, &vertBufferInfo, &mappableBufCreateInfo, &vertBuf, &vertAlloc, nullptr));
    VK_CHECK(vmaMapMemory(vmaAllocator, vertAlloc, &mappedData));
    memcpy(mappedData, verts, sizeof(float) * 24);
    vmaUnmapMemory(vmaAllocator, vertAlloc);

    // Create Index Buffer
    VkBufferCreateInfo idxBufferInfo = {};
    {
        idxBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        idxBufferInfo.size = sizeof(uint32_t) * 6;
        idxBufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }
    VkBuffer idxBuf;
    VmaAllocation idxAlloc;
    VK_CHECK(vmaCreateBuffer(vmaAllocator, &idxBufferInfo, &mappableBufCreateInfo, &idxBuf, &idxAlloc, nullptr));
    VK_CHECK(vmaMapMemory(vmaAllocator, idxAlloc, &mappedData));
    memcpy(mappedData, vertIdx, sizeof(uint32_t) * 6);
    vmaUnmapMemory(vmaAllocator, idxAlloc);

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

    // Create attachment descriptions for the color image and the depth image
    // 0: color image attachment;
    VkAttachmentDescription attachments[1];
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
    }

    // Create color reference
    VkAttachmentReference colorReference{};
    {
        colorReference.attachment = 0;
        colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
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
        subpass.preserveAttachmentCount = 0;
        subpass.pPreserveAttachments = nullptr;
    }

    // Create the render pass
    VkRenderPassCreateInfo renderPassCreateInfo{};
    {
        renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassCreateInfo.attachmentCount = 1;
        renderPassCreateInfo.pAttachments = attachments;
        renderPassCreateInfo.subpassCount = 1;
        renderPassCreateInfo.pSubpasses = &subpass;
    }
    VkRenderPass renderPass;
    VK_CHECK(vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass));

    // Create the frame buffer
    VkImageView attachmentsViews[1] = {colorImgView};
    VkFramebufferCreateInfo framebufferCreateInfo{};
    {
        framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.renderPass = renderPass;
        framebufferCreateInfo.attachmentCount = 1;
        framebufferCreateInfo.pAttachments = attachmentsViews;
        framebufferCreateInfo.width = 960;
        framebufferCreateInfo.height = 680;
        framebufferCreateInfo.layers = 1;
    }
    VkFramebuffer frameBuffer;
    VK_CHECK(vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &frameBuffer));

    // Create Vert Shader Module -- SOURCE_PATH is a MACRO definition passed in during compilation, which is specified in
    //                              the CMakeLists.txt file in the same level of repository.
    std::string shaderVertPath = std::string(SOURCE_PATH) + std::string("/DumpQuad.vert.spv");
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
    std::string shaderFragPath = std::string(SOURCE_PATH) + std::string("/DumpQuad.frag.spv");
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
        vertBindingDesc.stride = 6 * sizeof(float);
        vertBindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    }

    VkVertexInputAttributeDescription vertAttrDesc[2];
    {
        // Position
        vertAttrDesc[0].location = 0;
        vertAttrDesc[0].binding = 0;
        vertAttrDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        vertAttrDesc[0].offset = 0;
        // Color
        vertAttrDesc[1].location = 1;
        vertAttrDesc[1].binding = 0;
        vertAttrDesc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        vertAttrDesc[1].offset = 3 * sizeof(float);
    }
    VkPipelineVertexInputStateCreateInfo vertInputInfo{};
    {
        vertInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertInputInfo.pNext = nullptr;
        vertInputInfo.vertexBindingDescriptionCount = 1;
        vertInputInfo.pVertexBindingDescriptions = &vertBindingDesc;
        vertInputInfo.vertexAttributeDescriptionCount = 2;
        vertInputInfo.pVertexAttributeDescriptions = vertAttrDesc;
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

    // Create a pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    {
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0;
    }
    VkPipelineLayout pipelineLayout{};
    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout));

    // Create the graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    {
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
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
        pipelineInfo.renderPass = renderPass;
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
    VkRenderPassBeginInfo renderPassBeginInfo{};
    {
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.renderPass = renderPass;
        renderPassBeginInfo.framebuffer = frameBuffer;
        renderPassBeginInfo.renderArea.offset.x = 0;
        renderPassBeginInfo.renderArea.offset.y = 0;
        renderPassBeginInfo.renderArea.extent.width = 960;
        renderPassBeginInfo.renderArea.extent.height = 680;
        renderPassBeginInfo.clearValueCount = 1;
        renderPassBeginInfo.pClearValues = &clearVal;
    }
    vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdSetViewport(cmdBuffer, 0, 1, &vp);

    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // vkCmdDraw(cmdBuffer, 3, 1, 0, 0);
    VkDeviceSize vbOffset = 0;
    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vertBuf, &vbOffset);
    vkCmdBindIndexBuffer(cmdBuffer, idxBuf, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmdBuffer, 6, 1, 0, 0, 0);

    vkCmdEndRenderPass(cmdBuffer);

    VK_CHECK(vkEndCommandBuffer(cmdBuffer));

    // Command buffer submit info
    VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

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

    // Make rendered image visible to the host
    void* mapped = nullptr;
    vmaMapMemory(vmaAllocator, imgAlloc, &mapped);

    // Copy to RAM
    std::vector<unsigned char> imageRAM(imgAlloc->GetSize());
    memcpy(imageRAM.data(), mapped, imgAlloc->GetSize());
    vmaUnmapMemory(vmaAllocator, imgAlloc);

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

    // Destroy the pipeline layout
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

    // Destroy both of the shader modules
    vkDestroyShaderModule(device, shaderVertModule, nullptr);
    vkDestroyShaderModule(device, shaderFragModule, nullptr);

    // Destroy the frame buffer
    vkDestroyFramebuffer(device, frameBuffer, nullptr);

    // Destroy the render pass
    vkDestroyRenderPass(device, renderPass, nullptr);

    // Destroy the image view
    vkDestroyImageView(device, colorImgView, nullptr);

    // Destroy the image
    vmaDestroyImage(vmaAllocator, colorImage, imgAlloc);

    // Destroy the vertex buffer
    vmaDestroyBuffer(vmaAllocator, vertBuf, vertAlloc);

    // Destroy the index buffer
    vmaDestroyBuffer(vmaAllocator, idxBuf, idxAlloc);

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
