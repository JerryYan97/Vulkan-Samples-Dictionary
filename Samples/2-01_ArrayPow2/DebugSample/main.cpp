//
// Created by Jerry on 11/28/2021.
//
#include <iostream>
#include <fstream>
#include "vulkan/vulkan.hpp"


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
    else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    {
        std::cout << "Callback Info: " << callback_data->messageIdNumber << ":" << callback_data->pMessageIdName << ":" <<  callback_data->pMessage << std::endl << std::endl;
    }
    else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
    {
        std::cout << "Callback Verbose: " << callback_data->messageIdNumber << ":" << callback_data->pMessageIdName << ":" <<  callback_data->pMessage << std::endl << std::endl;
    }
    return VK_FALSE;
}

#define VK_CHECK(res) if(res){std::cout << "Error at line:" << __LINE__ << ", Error name:" << to_string(res) << ".\n"; exit(1);}

int main()
{
    // Verify that the debug extension for the callback messenger is supported.
    uint32_t propNum;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &propNum, nullptr));
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
        appInfo.pApplicationName = "ArrayPow2";
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

    // Initialize the device with compute queue
    uint32_t queueFamilyPropCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropCount, nullptr);
    assert(queueFamilyPropCount >= 1);
    std::vector<VkQueueFamilyProperties> queueFamilyProps(queueFamilyPropCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropCount, queueFamilyProps.data());

    bool found = false;
    unsigned int queueFamilyIdx = -1;
    for (int i = 0; i < queueFamilyPropCount; ++i)
    {
        if(queueFamilyProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            queueFamilyIdx = i;
            found = true;
            break;
        }
    }
    assert(found);

    float queuePriorities[1] = {0.f};
    VkDeviceQueueCreateInfo queueInfo{};
    {
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = queueFamilyIdx;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = queuePriorities;
    }

    VkDeviceCreateInfo deviceInfo{};
    {
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.pQueueCreateInfos = &queueInfo;
    }
    VkDevice device;
    VK_CHECK(vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device));

    // Get a compute queue
    VkQueue computeQueue;
    vkGetDeviceQueue(device, queueFamilyIdx, 0, &computeQueue);

    // Create a command pool supporting compute commands
    VkCommandPoolCreateInfo cmdPoolInfo{};
    {
        cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmdPoolInfo.queueFamilyIndex = queueFamilyIdx;
        cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    }
    VkCommandPool cmdPool;
    VK_CHECK(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &cmdPool));

    // Data for the input buffer
    uint32_t vals[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    VkDeviceSize valsSize = 9 * sizeof(uint32_t);

    // Create host buffer and device buffer as well as memory that they need
    VkBuffer ioBuffer;
    VkDeviceMemory ioMemory;

    // Create the input output buffer
    VkBufferCreateInfo ioBufInfo{};
    {
        ioBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        ioBufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        ioBufInfo.size = valsSize;
        ioBufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    VK_CHECK(vkCreateBuffer(device, &ioBufInfo, nullptr, &ioBuffer));

    // Allocate memory for the io buffer
    VkMemoryRequirements ioMemReqs;
    vkGetBufferMemoryRequirements(device, ioBuffer, &ioMemReqs);

    bool ioMemMatch = false;
    uint32_t ioMemTypeIdx = 0;
    VkMemoryPropertyFlagBits reqMemProp = static_cast<VkMemoryPropertyFlagBits>(
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    for (int i = 0; i < phyDeviceMemProps.memoryTypeCount; ++i)
    {
        if (ioMemReqs.memoryTypeBits & (1 << i))
        {
            if(phyDeviceMemProps.memoryTypes[i].propertyFlags & reqMemProp)
            {
                ioMemTypeIdx = i;
                ioMemMatch = true;
            }
        }
    }
    assert(ioMemMatch);

    VkMemoryAllocateInfo ioMemAllocInfo{};
    {
        ioMemAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ioMemAllocInfo.memoryTypeIndex = ioMemTypeIdx;
        ioMemAllocInfo.allocationSize = ioMemReqs.size;
    }
    VK_CHECK(vkAllocateMemory(device, &ioMemAllocInfo, nullptr, &ioMemory));

    // Copy data to the allocated io memory
    void *mapped;
    VK_CHECK(vkMapMemory(device, ioMemory, 0, valsSize, 0, &mapped));
    memcpy(mapped, vals, valsSize);
    vkUnmapMemory(device, ioMemory);

    // Bind io buffer and its memory
    VK_CHECK(vkBindBufferMemory(device, ioBuffer, ioMemory, 0));

    /*
     * Prepare compute pipeline
     * */
    VkDescriptorPoolSize desPoolSize{};
    {
        desPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        desPoolSize.descriptorCount = 1;
    }

    VkDescriptorPoolCreateInfo desPoolInfo{};
    {
        desPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        desPoolInfo.poolSizeCount = 1;
        desPoolInfo.pPoolSizes = &desPoolSize;
        desPoolInfo.maxSets = 1;
    }
    VkDescriptorPool desPool;
    VK_CHECK(vkCreateDescriptorPool(device, &desPoolInfo, nullptr, &desPool));

    VkDescriptorSetLayoutBinding desSetLayoutBinding{};
    {
        desSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        desSetLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        desSetLayoutBinding.binding = 0;
        desSetLayoutBinding.descriptorCount = 1;
    }

    VkDescriptorSetLayoutCreateInfo desSetLayoutInfo{};
    {
        desSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        desSetLayoutInfo.pBindings = &desSetLayoutBinding;
        desSetLayoutInfo.bindingCount = 1;
    }
    VkDescriptorSetLayout desSetLayout;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &desSetLayoutInfo, nullptr, &desSetLayout));

    VkPipelineLayoutCreateInfo pipeLayoutInfo{};
    {
        pipeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeLayoutInfo.setLayoutCount = 1;
        pipeLayoutInfo.pSetLayouts = &desSetLayout;
    }
    VkPipelineLayout pipelineLayout;
    VK_CHECK(vkCreatePipelineLayout(device, &pipeLayoutInfo, nullptr, &pipelineLayout));

    VkDescriptorSetAllocateInfo desSetAllocInfo{};
    {
        desSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        desSetAllocInfo.descriptorPool = desPool;
        desSetAllocInfo.pSetLayouts = &desSetLayout;
        desSetAllocInfo.descriptorSetCount = 1;
    }
    VkDescriptorSet descriptorSet;
    VK_CHECK(vkAllocateDescriptorSets(device, &desSetAllocInfo, &descriptorSet));

    VkDescriptorBufferInfo bufferDesInfo{};
    {
        // bufferDesInfo.buffer = deviceBuffer;
        bufferDesInfo.buffer = ioBuffer;
        bufferDesInfo.offset = 0;
        bufferDesInfo.range = VK_WHOLE_SIZE;
    }

    VkWriteDescriptorSet computeWriteDesSets{};
    {
        computeWriteDesSets.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        computeWriteDesSets.dstSet = descriptorSet;
        computeWriteDesSets.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        computeWriteDesSets.dstBinding = 0;
        computeWriteDesSets.pBufferInfo = &bufferDesInfo;
        computeWriteDesSets.descriptorCount = 1;
    }
    vkUpdateDescriptorSets(device, 1, &computeWriteDesSets, 0, nullptr);

    VkPipelineCacheCreateInfo pipelineCacheCreateInfo{};
    {
        pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    }
    VkPipelineCache pipelineCache;
    VK_CHECK(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &pipelineCache));

    // Pass SSBO size via specialization constant
    struct SpecializationData{
        uint32_t BUFFER_ELEMENT_COUNT = 9;
    } specializationData;
    VkSpecializationMapEntry specializationMapEntry{};
    {
        specializationMapEntry.constantID = 0;
        specializationMapEntry.offset = 0;
        specializationMapEntry.size = sizeof(uint32_t);
    }
    VkSpecializationInfo specializationInfo{};
    {
        specializationInfo.mapEntryCount = 1;
        specializationInfo.pMapEntries = &specializationMapEntry;
        specializationInfo.dataSize = sizeof(SpecializationData);
        specializationInfo.pData = &specializationData;
    }

    std::string shaderComputePath = std::string(SOURCE_PATH) + std::string("/ArrayPow2.comp.spv");
    std::ifstream inputCompShader(shaderComputePath.c_str(), std::ios::binary | std::ios::in);
    std::vector<unsigned char> inputCompShaderStr(std::istreambuf_iterator<char>(inputCompShader), {});
    inputCompShader.close();
    VkShaderModuleCreateInfo shaderCompModuleCreateInfo{};
    {
        shaderCompModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderCompModuleCreateInfo.codeSize = inputCompShaderStr.size();
        shaderCompModuleCreateInfo.pCode = (uint32_t*) inputCompShaderStr.data();
    }
    VkShaderModule shaderCompModule;
    VK_CHECK(vkCreateShaderModule(device, &shaderCompModuleCreateInfo, nullptr, &shaderCompModule));

    VkPipelineShaderStageCreateInfo compPipelineShaderStageInfo{};
    {
        compPipelineShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        compPipelineShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        compPipelineShaderStageInfo.module = shaderCompModule;
        compPipelineShaderStageInfo.pName = "main";
        compPipelineShaderStageInfo.pSpecializationInfo = &specializationInfo;
    }

    // Create pipeline
    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    {
        computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        computePipelineCreateInfo.layout = pipelineLayout;
        computePipelineCreateInfo.flags = 0;
        computePipelineCreateInfo.stage = compPipelineShaderStageInfo;
    }
    VkPipeline pipeline;
    VK_CHECK(vkCreateComputePipelines(device, pipelineCache, 1, &computePipelineCreateInfo, nullptr, &pipeline));

    /*
     * Command buffer creation (for compute work submission)
     */
    // Create a command buffer for compute operations
    VkCommandBufferAllocateInfo cmdBufAllocInfo{};
    {
        cmdBufAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdBufAllocInfo.commandPool = cmdPool;
        cmdBufAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdBufAllocInfo.commandBufferCount = 1;
    }
    VkCommandBuffer cmdBuf;
    VK_CHECK(vkAllocateCommandBuffers(device, &cmdBufAllocInfo, &cmdBuf));

    // Fence for compute CB sync
    VkFenceCreateInfo fenceCreateInfo{};
    {
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    }
    VkFence fence;
    VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &fence));

    VkCommandBufferBeginInfo cmdBufBeginInfo{};
    {
        cmdBufBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    }
    VK_CHECK(vkBeginCommandBuffer(cmdBuf, &cmdBufBeginInfo));

    // Barrier to ensure that input buffer transfer is finished before compute shader reads from it
    VkBufferMemoryBarrier bufBarrier{};
    {
        bufBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        // bufBarrier.buffer = deviceBuffer;
        bufBarrier.buffer = ioBuffer;
        bufBarrier.size = VK_WHOLE_SIZE;
        bufBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        bufBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        bufBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bufBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    }
    vkCmdPipelineBarrier(
            cmdBuf,
            VK_PIPELINE_STAGE_HOST_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0,
            0, nullptr,
            1, &bufBarrier,
            0, nullptr);
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, 0);
    vkCmdDispatch(cmdBuf, 9, 1, 1);

    // Barrier to ensure that shader writes are finished before buffer is read back from GPU
    bufBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    bufBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    bufBarrier.buffer = ioBuffer;
    bufBarrier.size = VK_WHOLE_SIZE;
    bufBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vkCmdPipelineBarrier(cmdBuf,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_HOST_BIT,
                         0,
                         0, nullptr,
                         1, &bufBarrier,
                         0, nullptr);

    VK_CHECK(vkEndCommandBuffer(cmdBuf));

    // Submit all recorded works
    VK_CHECK(vkResetFences(device, 1, &fence));
    VkSubmitInfo computeSubmitInfo{};
    {
        computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        computeSubmitInfo.commandBufferCount = 1;
        computeSubmitInfo.pCommandBuffers = &cmdBuf;
    }
    VK_CHECK(vkQueueSubmit(computeQueue, 1, &computeSubmitInfo, fence));
    VK_CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));

    // Make device writes visible to the host
    vkMapMemory(device, ioMemory, 0, VK_WHOLE_SIZE, 0, &mapped);

    // Copy to output
    uint32_t output[9] = {0};
    memcpy(output, mapped, 9 * sizeof(uint32_t));
    vkUnmapMemory(device, ioMemory);

    VK_CHECK(vkQueueWaitIdle(computeQueue));

    // Output
    std::cout << output[0] << ", " << output[1] << ", " << output[2] << std::endl;
    std::cout << output[3] << ", " << output[4] << ", " << output[5] << std::endl;
    std::cout << output[6] << ", " << output[7] << ", " << output[8] << std::endl;

    // Clean up
    vkDestroyBuffer(device, ioBuffer, nullptr);
    vkFreeMemory(device, ioMemory, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, desSetLayout, nullptr);
    vkDestroyDescriptorPool(device, desPool, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineCache(device, pipelineCache, nullptr);
    vkDestroyFence(device, fence, nullptr);
    vkDestroyCommandPool(device, cmdPool, nullptr);
    vkDestroyShaderModule(device, shaderCompModule, nullptr);
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