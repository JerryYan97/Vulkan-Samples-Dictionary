//
// Created by Jerry on 11/28/2021.
//
#include "vulkan/vulkan.h"
#include <vector>
#include <iostream>

int main()
{
    // Initialize instance and application
    VkApplicationInfo appInfo{}; // TIPS: You can delete this bracket to see what happens.
    {
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "CreateInstance";
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

    // Destroy instance
    vkDestroyInstance(instance, nullptr);
}
