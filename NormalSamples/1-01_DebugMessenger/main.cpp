//
// Created by Jerry on 11/28/2021.
//
#include <iostream>
#include <fstream>
#include "vulkan/vulkan.hpp"

int main()
{
    // Verify that the debug extension is supported.
    std::vector<vk::ExtensionProperties> props = vk::enumerateInstanceExtensionProperties();
    for (int i = 0; i < props.size(); ++i)
    {
        if(strcmp(props[i].extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
        {
            break;
        }
        if(i == props.size() - 1)
        {
            std::cout << "Something went very wrong, cannot find " << VK_EXT_DEBUG_UTILS_EXTENSION_NAME << " extension"
                      << std::endl;
            exit(1);
        }
    }

    // Initialize instance and application
    vk::ApplicationInfo appInfo("DebugMessenger", 1, "VulkanDict", 1, VK_API_VERSION_1_1);

    std::cout << "Hello World" << std::endl;
}
