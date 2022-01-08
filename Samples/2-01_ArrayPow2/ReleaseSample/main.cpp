//
// Created by Jerry on 11/28/2021.
//
#include <iostream>
#include <fstream>
#include "vulkan/vulkan.hpp"

int main()
{
    // Initialize instance and application
    VkApplicationInfo appInfo{};
    {
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "ArrayPow2";
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

    // Initialize the device with compute queue
    uint32_t queueFamilyPropCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropCount, nullptr);
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
    vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device);

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
    vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &cmdPool);

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
    vkCreateBuffer(device, &ioBufInfo, nullptr, &ioBuffer);

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
    vkAllocateMemory(device, &ioMemAllocInfo, nullptr, &ioMemory);

    // Copy data to the allocated io memory
    void *mapped;
    vkMapMemory(device, ioMemory, 0, valsSize, 0, &mapped);
    memcpy(mapped, vals, valsSize);
    vkUnmapMemory(device, ioMemory);

    // Bind io buffer and its memory
    vkBindBufferMemory(device, ioBuffer, ioMemory, 0);

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
    vkCreateDescriptorPool(device, &desPoolInfo, nullptr, &desPool);

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
    vkCreateDescriptorSetLayout(device, &desSetLayoutInfo, nullptr, &desSetLayout);

    VkPipelineLayoutCreateInfo pipeLayoutInfo{};
    {
        pipeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeLayoutInfo.setLayoutCount = 1;
        pipeLayoutInfo.pSetLayouts = &desSetLayout;
    }
    VkPipelineLayout pipelineLayout;
    vkCreatePipelineLayout(device, &pipeLayoutInfo, nullptr, &pipelineLayout);

    VkDescriptorSetAllocateInfo desSetAllocInfo{};
    {
        desSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        desSetAllocInfo.descriptorPool = desPool;
        desSetAllocInfo.pSetLayouts = &desSetLayout;
        desSetAllocInfo.descriptorSetCount = 1;
    }
    VkDescriptorSet descriptorSet;
    vkAllocateDescriptorSets(device, &desSetAllocInfo, &descriptorSet);

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
    vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &pipelineCache);

    // Pass SSBO size via specialization constant
    struct SpecializationData{
        uint32_t BUFFER_ELEMENT_COUNT = 9; // You can try to decrease this number to see how it affects output.
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
    vkCreateShaderModule(device, &shaderCompModuleCreateInfo, nullptr, &shaderCompModule);

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
    vkCreateComputePipelines(device, pipelineCache, 1, &computePipelineCreateInfo, nullptr, &pipeline);

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
    vkAllocateCommandBuffers(device, &cmdBufAllocInfo, &cmdBuf);

    // Fence for compute CB sync
    VkFenceCreateInfo fenceCreateInfo{};
    {
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    }
    VkFence fence;
    vkCreateFence(device, &fenceCreateInfo, nullptr, &fence);

    VkCommandBufferBeginInfo cmdBufBeginInfo{};
    {
        cmdBufBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    }
    vkBeginCommandBuffer(cmdBuf, &cmdBufBeginInfo);

    // Barrier to ensure that input buffer transfer is finished before compute shader reads from it
    VkBufferMemoryBarrier bufBarrier{};
    {
        bufBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
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

    vkEndCommandBuffer(cmdBuf);

    // Submit all recorded works
    vkResetFences(device, 1, &fence);
    VkSubmitInfo computeSubmitInfo{};
    {
        computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        computeSubmitInfo.commandBufferCount = 1;
        computeSubmitInfo.pCommandBuffers = &cmdBuf;
    }
    vkQueueSubmit(computeQueue, 1, &computeSubmitInfo, fence);
    vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);

    // Make device writes visible to the host
    vkMapMemory(device, ioMemory, 0, VK_WHOLE_SIZE, 0, &mapped);

    // Copy to output
    uint32_t output[9] = {0};
    memcpy(output, mapped, 9 * sizeof(uint32_t));
    vkUnmapMemory(device, ioMemory);

    vkQueueWaitIdle(computeQueue);

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
    // Destroy instance
    vkDestroyInstance(instance, nullptr);
}