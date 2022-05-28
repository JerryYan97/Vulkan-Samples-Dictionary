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

    // Data for the uniform buffer
    float mat[] = {1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f, 9.f};
    VkBufferCreateInfo bufCreateInfo{};
    {
        bufCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufCreateInfo.size = 9 * sizeof(float);
        bufCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    VkBuffer uniformBuffer;
    vkCreateBuffer(device, &bufCreateInfo, nullptr, &uniformBuffer);

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

    uint8_t *pData;
    vkMapMemory(device, deviceUniformBufMem, 0, memReqs.size, 0, (void**) &pData);
    memcpy(pData, mat, 9 * sizeof(float));
    vkUnmapMemory(device, deviceUniformBufMem);

    vkBindBufferMemory(device, uniformBuffer, deviceUniformBufMem, 0);

    // Destroy Uniform buffer
    vkDestroyBuffer(device, uniformBuffer, nullptr);

    // Free memory
    vkFreeMemory(device, deviceUniformBufMem, nullptr);

    // Destroy the device
    vkDestroyDevice(device, nullptr);

    // Destroy instance
    vkDestroyInstance(instance, nullptr);
}
