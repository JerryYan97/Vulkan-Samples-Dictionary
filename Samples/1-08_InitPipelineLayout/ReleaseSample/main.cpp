//
// Created by Jerry on 11/28/2021.
//
#include "vulkan/vulkan.h"
#include <vector>
#include <iostream>


int main()
{
    // Initialize instance and application
    VkApplicationInfo appInfo{};
    {
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "InitUniformBuffer";
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

    // Create a descriptor set layout binding
    VkDescriptorSetLayoutBinding layoutBinding{};
    {
        layoutBinding.binding = 0;
        layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        layoutBinding.descriptorCount = 1;
        layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    }

    // Create a descriptor set layout
    VkDescriptorSetLayoutCreateInfo desSetLayoutInfo{};
    {
        desSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        desSetLayoutInfo.bindingCount = 1;
        desSetLayoutInfo.pBindings = &layoutBinding;
    }
    VkDescriptorSetLayout desSetLayout{};
    vkCreateDescriptorSetLayout(device, &desSetLayoutInfo, nullptr, &desSetLayout);

    // Create a pipeline layout by using a descriptor set layout
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    {
        pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCreateInfo.setLayoutCount = 1;
        pipelineLayoutCreateInfo.pSetLayouts = &desSetLayout;
    }
    VkPipelineLayout pipelineLayout;
    vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);

    // Data for the uniform buffer
    float mat[] = {1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f, 9.f};

    // Create the uniform buffer
    VkBufferCreateInfo bufCreateInfo{};
    {
        bufCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufCreateInfo.size = 9 * sizeof(float);
        bufCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    VkBuffer uniformBuffer;
    vkCreateBuffer(device, &bufCreateInfo, nullptr, &uniformBuffer);

    // Allocate memory for the uniform buffer
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, uniformBuffer, &memReqs);

    uint32_t memTypeIdx = 0;
    for (int i = 0; i < phyDeviceMemProps.memoryTypeCount; ++i)
    {
        if(memReqs.memoryTypeBits & (1 << i))
        {
            if(phyDeviceMemProps.memoryTypes[i].propertyFlags &
               (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
            {
                memTypeIdx = i;
                break;
            }
        }
    }

    VkMemoryAllocateInfo allocInfo{};
    {
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.memoryTypeIndex = memTypeIdx;
        allocInfo.allocationSize = memReqs.size;
    }

    VkDeviceMemory deviceUniformBufMem;
    vkAllocateMemory(device, &allocInfo, nullptr, &deviceUniformBufMem);

    // Copy mat data to the allocated memory.
    uint8_t *pData;
    vkMapMemory(device, deviceUniformBufMem, 0, memReqs.size, 0, (void**) &pData);
    memcpy(pData, mat, 9 * sizeof(float));
    vkUnmapMemory(device, deviceUniformBufMem);

    // Bind the memory and the uniform buffer together.
    vkBindBufferMemory(device, uniformBuffer, deviceUniformBufMem, 0);

    // Create the descriptor pool
    VkDescriptorPoolSize desPoolSize{};
    {
        desPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        desPoolSize.descriptorCount = 1;
    }

    VkDescriptorPoolCreateInfo desPoolInfo{};
    {
        desPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        desPoolInfo.maxSets = 1;
        desPoolInfo.poolSizeCount = 1;
        desPoolInfo.pPoolSizes = &desPoolSize;
    }

    VkDescriptorPool descriptorPool;
    vkCreateDescriptorPool(device, &desPoolInfo, nullptr, &descriptorPool);

    // Allocate the descriptor set
    VkDescriptorSetAllocateInfo desSetAllocInfo{};
    {
        desSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        desSetAllocInfo.descriptorPool = descriptorPool;
        desSetAllocInfo.descriptorSetCount = 1;
        desSetAllocInfo.pSetLayouts = &desSetLayout;
    }
    VkDescriptorSet descriptorSet;
    vkAllocateDescriptorSets(device, &desSetAllocInfo, &descriptorSet);

    // Write Info to the descriptor
    VkDescriptorBufferInfo desBufInfo{};
    {
        desBufInfo.buffer = uniformBuffer;
        desBufInfo.offset = 0;
        desBufInfo.range = 9 * sizeof(float);
    }
    VkWriteDescriptorSet writeDesSet{};
    {
        writeDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDesSet.dstSet = descriptorSet;
        writeDesSet.descriptorCount = 1;
        writeDesSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeDesSet.pBufferInfo = &desBufInfo;
    }
    vkUpdateDescriptorSets(device, 1, &writeDesSet, 0, nullptr);

    // Destroy the descriptor pool
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);

    // Destroy Uniform buffer
    vkDestroyBuffer(device, uniformBuffer, nullptr);

    // Free memory
    vkFreeMemory(device, deviceUniformBufMem, nullptr);

    // Destroy the descriptor set layout
    vkDestroyDescriptorSetLayout(device, desSetLayout, nullptr);

    // Destroy the pipeline layout
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

    // Destroy the device
    vkDestroyDevice(device, nullptr);

    // Destroy instance
    vkDestroyInstance(instance, nullptr);
}
