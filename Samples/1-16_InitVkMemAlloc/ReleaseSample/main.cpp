#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include <vector>
#include <iostream>

int main() {
    // Initialize instance and application
    VkApplicationInfo appInfo{};
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

    // Initialize the device with graphics queue
    uint32_t queueFamilyPropCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilyProps(queueFamilyPropCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropCount, queueFamilyProps.data());

    unsigned int queueFamilyIdx = -1;
    for (unsigned int i = 0; i < queueFamilyPropCount; ++i) {
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

    VmaAllocator allocator;
    vmaCreateAllocator(&allocCreateInfo, &allocator);

    VkBufferCreateInfo bufferInfo = {};
    {
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size  = 65536;
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    VmaAllocationCreateInfo allocInfo = {};
    {
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    }

    VkBuffer buffer;
    VmaAllocation allocation;
    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);

    // Destroy the buffer
    vmaDestroyBuffer(allocator, buffer, allocation);

    // Destroy the allocator
    vmaDestroyAllocator(allocator);

    // Destroy the device
    vkDestroyDevice(device, nullptr);

    // Destroy instance
    vkDestroyInstance(instance, nullptr);
}
