#include <iostream>
#include <vector>
#include <fstream>
#include <cassert>
#include "vulkan/vulkan.h"
#include "lodepng.h"
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

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
        appInfo.pApplicationName = "DumpTri";
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

    // Extension for dynamic rendering
    const std::vector<const char*> deviceExtensions = { VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME };

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_feature{};
    {
        dynamic_rendering_feature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
        dynamic_rendering_feature.dynamicRendering = VK_TRUE;
    }

    VkDeviceCreateInfo deviceInfo{};
    {
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.pNext = &dynamic_rendering_feature;
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        deviceInfo.enabledExtensionCount = uint32_t(deviceExtensions.size());
        deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
    }
    VkDevice device;
    VK_CHECK(vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device));

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

    VmaAllocator allocator;
    vmaCreateAllocator(&allocCreateInfo, &allocator);

    // Get a graphics queue
    VkQueue graphicsQueue;
    vkGetDeviceQueue(device, queueFamilyIdx, 0, &graphicsQueue);

    // Create the image
    VkImage colorImage;
    VmaAllocation imgAllocation;
    VkFormat colorImgFormat = VK_FORMAT_R8G8B8A8_UNORM;

    VmaAllocationCreateInfo imgAllocInfo{};
    {
        imgAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        imgAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
                             VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
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
        imageInfo.tiling = VK_IMAGE_TILING_LINEAR;
    }

    vmaCreateImage(allocator, &imageInfo, &imgAllocInfo, &colorImage, &imgAllocation, nullptr);

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

    // Create the buffer and transfer data to it -- vec3 alignment needs to be 4 floats. So, we use 12 floats buffer
    // for 9 floats ubo input.
    VkBuffer uboBuffer;
    VmaAllocation uboBufferAllocation;

    VkBufferCreateInfo bufferInfo{};
    {
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = 12 * sizeof(float);
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo bufferAllocInfo{};
    {
        bufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        bufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | 
                                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    vmaCreateBuffer(allocator, &bufferInfo, &bufferAllocInfo, &uboBuffer, &uboBufferAllocation, nullptr);

    // Create the ubo buffer and copy data to it.
    void* mapped = nullptr;
    vkMapMemory(device, uboBufferAllocation->GetMemory(), 0, VK_WHOLE_SIZE, 0, &mapped);

    // Copy from RAM
    float uboData[12] = {
        1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
    };

    memcpy(mapped, uboData, 12 * sizeof(float));

    vkUnmapMemory(device, uboBufferAllocation->GetMemory());

    // Create the descriptor set of ubo buffer for pipeline binding.
    VkDescriptorPool descriptorPool;
    VkDescriptorSet uboDescriptorSet;

    VkDescriptorPoolSize poolSize{};
    {
        poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = 1;
    }

    VkDescriptorPoolCreateInfo poolInfo{};
    {
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = 1;
    }
    
    VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool));

    // Create a descriptor set layout
    VkDescriptorSetLayout descriptorSetLayout;

    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    {
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.pImmutableSamplers = nullptr;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    {
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &uboLayoutBinding;
    }

    VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout));

    VkDescriptorSetAllocateInfo descriptorSetAllocInfo{};
    {
        descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorSetAllocInfo.descriptorPool = descriptorPool;
        descriptorSetAllocInfo.descriptorSetCount = 1;
        descriptorSetAllocInfo.pSetLayouts = &descriptorSetLayout;
    }
    
    vkAllocateDescriptorSets(device, &descriptorSetAllocInfo, &uboDescriptorSet);

    // Update the descriptor set to let it point to the ubo buffer
    VkDescriptorBufferInfo descriptorBufferInfo{};
    {
        descriptorBufferInfo.buffer = uboBuffer;
        descriptorBufferInfo.offset = 0;
        descriptorBufferInfo.range = sizeof(uboData);
    }

    VkWriteDescriptorSet descriptorWrite{};
    {
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = uboDescriptorSet;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &descriptorBufferInfo;
    }

    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

    // Create Vert Shader Module -- SOURCE_PATH is a MACRO definition passed in during compilation, which is specified in
    //                              the CMakeLists.txt file in the same level of repository.
    std::string shaderVertPath = std::string(SOURCE_PATH) + std::string("/UniformBufTri.vert.spv");
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
    std::string shaderFragPath = std::string(SOURCE_PATH) + std::string("/UniformBufTri.frag.spv");
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
    VkPipelineVertexInputStateCreateInfo vertInputInfo{};
    {
        vertInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
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
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    }
    VkPipelineLayout pipelineLayout{};
    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout));

    // Create the graphics pipeline -- The graphics pipeline is used for scene rendering
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
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
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

    // Wait for the ubo data transfer compeletion.
    VkMemoryBarrier uboDataTransBarrier{};
    {
        uboDataTransBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        uboDataTransBarrier.pNext = nullptr;
        uboDataTransBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        uboDataTransBarrier.dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT;
    }

    vkCmdPipelineBarrier(cmdBuffer, 
                         VK_PIPELINE_STAGE_HOST_BIT,
                         VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 
                         0,
                         1, &uboDataTransBarrier,
                         0, nullptr,
                         0, nullptr);

    VkClearValue clearVal{};
    {
        clearVal.color = {{0.f, 0.f, 0.f, 1.f}};
    }

    VkRenderingAttachmentInfoKHR renderAttachmentInfo{};
    {
        renderAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
        renderAttachmentInfo.imageView = colorImgView;
        renderAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
        renderAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        renderAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        renderAttachmentInfo.clearValue = clearVal;
    }

    VkRenderingInfoKHR renderInfo{};
    {
        renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
        renderInfo.renderArea.offset = { 0, 0 };
        renderInfo.renderArea.extent = { 960, 680 };
        renderInfo.layerCount = 1;
        renderInfo.colorAttachmentCount = 1;
        renderInfo.pColorAttachments = &renderAttachmentInfo;
    }

    vkCmdBeginRendering(cmdBuffer, &renderInfo);

    vkCmdSetViewport(cmdBuffer, 0, 1, &vp);

    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    vkCmdBindDescriptorSets(cmdBuffer, 
                            VK_PIPELINE_BIND_POINT_GRAPHICS, 
                            pipelineLayout, 
                            0, 
                            1, 
                            &uboDescriptorSet, 
                            0, 
                            nullptr);

    vkCmdDraw(cmdBuffer, 3, 1, 0, 0);

    vkCmdEndRendering(cmdBuffer);

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
    vkMapMemory(device, imgAllocation->GetMemory(), 0, VK_WHOLE_SIZE, 0, &mapped);

    // Copy to RAM
    std::vector<unsigned char> imageRAM(imgAllocation->GetSize());
    memcpy(imageRAM.data(), mapped, imgAllocation->GetSize());
    vkUnmapMemory(device, imgAllocation->GetMemory());

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

    // Destroy the descriptor set layout
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

    // Destroy the descriptor pool
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);

    // Destroy Pipeline
    vkDestroyPipeline(device, pipeline, nullptr);

    // Destroy the pipeline layout
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

    // Destroy both of the shader modules
    vkDestroyShaderModule(device, shaderVertModule, nullptr);
    vkDestroyShaderModule(device, shaderFragModule, nullptr);

    // Destroy buffer
    vmaDestroyBuffer(allocator, uboBuffer, uboBufferAllocation);

    // Destroy the image view
    vkDestroyImageView(device, colorImgView, nullptr);

    // Destroy the image
    vmaDestroyImage(allocator, colorImage, imgAllocation);

    // Destroy the allocator
    vmaDestroyAllocator(allocator);

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
