#include <iostream>
#include <vector>
#include <unordered_set>
#include <fstream>
#include <cassert>
#include <algorithm>
#include "vulkan/vulkan.h"
#include "lodepng.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <glfw3.h>

// Features: VulkanRT, Vulkan HLSL, Vulkan Dynamic Rendering.
// Reference: 2023 SIGGRAPH Course - Real-Time Ray-Tracing with Vulkan for the Impatient.
//            https://github.com/SaschaWillems/Vulkan/blob/master/examples/raytracingbasic/raytracingbasic.cpp

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

VkShaderModule createShaderModule(const std::string& spvName, const VkDevice& device)
{
    // Create  Shader Module -- SOURCE_PATH is a MACRO definition passed in during compilation, which is specified in
    //                          the CMakeLists.txt file in the same level of repository.
    std::string shaderPath = std::string(SOURCE_PATH) + spvName;
    std::ifstream inputShader(shaderPath.c_str(), std::ios::binary | std::ios::in);
    std::vector<unsigned char> inputShaderStr(std::istreambuf_iterator<char>(inputShader), {});
    inputShader.close();
    VkShaderModuleCreateInfo shaderModuleCreateInfo{};
    {
        shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderModuleCreateInfo.codeSize = inputShaderStr.size();
        shaderModuleCreateInfo.pCode = (uint32_t*)inputShaderStr.data();
    }
    VkShaderModule shaderModule;
    vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &shaderModule);

    return shaderModule;
}

uint32_t alignedSize(uint32_t value, uint32_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

/* Debug swapchain */

const int MAX_FRAMES_IN_FLIGHT = 2;
uint32_t currentFrame = 0;
VkExtent2D swapchainImageExtent;
std::vector<VkImageView> swapchainImageViews;
std::vector<VkImage> swapchainImages;
VkSurfaceKHR surface;
VkSwapchainKHR swapchain;
GLFWwindow* window = nullptr;
VkDevice device;
VkPhysicalDevice physicalDevice;

// Create the image views
void CreateSwapchainImageViews()
{
    // Create image views for the swapchain images
    uint32_t swapchainImageCount;
    vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr);
    swapchainImages.resize(swapchainImageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data());

    swapchainImageViews.resize(swapchainImageCount);
    for (size_t i = 0; i < swapchainImageCount; i++)
    {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapchainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;
        VK_CHECK(vkCreateImageView(device, &createInfo, nullptr, &swapchainImageViews[i]));
    }
}

// Create the swapchain
void CreateSwapchain()
{
    // Create the swapchain
    // Qurery surface capabilities.
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);

    // Query surface formates
    uint32_t surfaceFormatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
    if (surfaceFormatCount != 0)
    {
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, surfaceFormats.data());
    }

    // Query the present mode
    uint32_t surfacePresentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &surfacePresentModeCount, nullptr);
    std::vector<VkPresentModeKHR> surfacePresentModes(surfacePresentModeCount);
    if (surfacePresentModeCount != 0)
    {
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &surfacePresentModeCount, surfacePresentModes.data());
    }

    // Choose the VK_PRESENT_MODE_FIFO_KHR.
    VkPresentModeKHR choisenPresentMode{};
    bool foundMailBoxPresentMode = false;
    for (const auto& avaPresentMode : surfacePresentModes)
    {
        if (avaPresentMode == VK_PRESENT_MODE_FIFO_KHR)
        {
            choisenPresentMode = VK_PRESENT_MODE_FIFO_KHR;
            foundMailBoxPresentMode = true;
            break;
        }
    }
    assert(choisenPresentMode == VK_PRESENT_MODE_FIFO_KHR);

    // Init swapchain's image extent
    int glfwFrameBufferWidth;
    int glfwFrameBufferHeight;
    glfwGetFramebufferSize(window, &glfwFrameBufferWidth, &glfwFrameBufferHeight);

    swapchainImageExtent = {
        std::clamp(static_cast<uint32_t>(glfwFrameBufferWidth), surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width),
        std::clamp(static_cast<uint32_t>(glfwFrameBufferHeight), surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height)
    };

    uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
    if (surfaceCapabilities.maxImageCount > 0 && imageCount > surfaceCapabilities.maxImageCount)
    {
        imageCount = surfaceCapabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchainCreateInfo{};
    {
        swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainCreateInfo.surface = surface;
        swapchainCreateInfo.minImageCount = imageCount;
        swapchainCreateInfo.imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
        swapchainCreateInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        swapchainCreateInfo.imageExtent = swapchainImageExtent;
        swapchainCreateInfo.imageArrayLayers = 1;
        swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        {
            swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }
        swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
        swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchainCreateInfo.presentMode = choisenPresentMode;
        swapchainCreateInfo.clipped = VK_TRUE;
    }
    VK_CHECK(vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain));
}

// Cleanup the swapchain
void CleanupSwapchain()
{
    // Cleanup the swap chain
    // Clean the image views
    for (auto imgView : swapchainImageViews)
    {
        vkDestroyImageView(device, imgView, nullptr);
    }

    // Destroy the swapchain
    vkDestroySwapchainKHR(device, swapchain, nullptr);
}

// Recreate the swapchain
void RecreateSwapchain()
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(device);
    CleanupSwapchain();
    CreateSwapchain();
    CreateSwapchainImageViews();
}

/*******************/


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
        appInfo.pApplicationName = "DumpTriRT";
        appInfo.applicationVersion = 1;
        appInfo.pEngineName = "VulkanDict";
        appInfo.engineVersion = 1;
        appInfo.apiVersion = VK_API_VERSION_1_3;
    }
    
    const std::vector<const char*> requiredInstExtensions = {
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
    };

    const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
    VkInstanceCreateInfo instanceCreateInfo{};
    {
        instanceCreateInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceCreateInfo.pNext = &debugCreateInfo;
        instanceCreateInfo.pApplicationInfo = &appInfo;
        instanceCreateInfo.enabledExtensionCount = requiredInstExtensions.size();
        instanceCreateInfo.ppEnabledExtensionNames = requiredInstExtensions.data();
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

    // Enumerate the physicalDevices.
    // Write down devices that support VulkanRT:
    // - VK_KHR_acceleration_structure
    // - VK_KHR_deferred_host_operations
    // - VK_KHR_ray_query
    // - VK_KHR_ray_tracing_maintenance1
    // - VK_KHR_ray_tracing_pipeline
    // Select the first one that fulfills the requirements and display the name of it.
    const std::vector<const char*> requiredDeviceExtensions = {
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_QUERY_EXTENSION_NAME,
        VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
        VK_KHR_MAINTENANCE3_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_EXT_MESH_SHADER_EXTENSION_NAME, // NOTE: I don't know why the SPIRV compiled from hlsl needs it...
        VK_KHR_SPIRV_1_4_EXTENSION_NAME,
        VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME
    };

    // VkPhysicalDevice physicalDevice;
    uint32_t phyDeviceCount;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &phyDeviceCount, nullptr));
    assert(phyDeviceCount >= 1);
    std::vector<VkPhysicalDevice> phyDeviceVec(phyDeviceCount);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &phyDeviceCount, phyDeviceVec.data()));

    // We need the physical device ray tracing pipeline properties afterward.
    VkPhysicalDeviceAccelerationStructurePropertiesKHR phyDevAccStructProperties = {};
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR phyDevRtPipelineProperties = {};

    for (VkPhysicalDevice phyDevice : phyDeviceVec)
    {
        VkPhysicalDeviceProperties physicalDevProperties;
        vkGetPhysicalDeviceProperties(phyDevice, &physicalDevProperties);
        std::cout << "Device name:" << physicalDevProperties.deviceName << std::endl;

        // Note - if you don't include VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME in
        // ppEnabledExtensionNames when you create instance, there will be no rayTracingPipelines
        
        // Check if ray query is supported.
        VkPhysicalDeviceRayQueryFeaturesKHR phyDevRayQueryFeatures = {};
        {
            phyDevRayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
        }

        // Check if Acceleration structure is supported. (Whether the native acceleration structure is supported.)
        VkPhysicalDeviceAccelerationStructureFeaturesKHR phyDevAccStructFeatures = {};
        {
            phyDevAccStructFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
            phyDevAccStructFeatures.pNext = &phyDevRayQueryFeatures;
        }

        // Check if ray tracing extension is supported
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures = {};
        {
            rtPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
            rtPipelineFeatures.pNext = &phyDevAccStructFeatures;
        }

        VkPhysicalDeviceFeatures2 phyDevFeatures2 = {};
        {
            phyDevFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            phyDevFeatures2.pNext = &rtPipelineFeatures;
        }

        vkGetPhysicalDeviceFeatures2(phyDevice, &phyDevFeatures2);

        std::cout << "Support raytracing pipeline: " << rtPipelineFeatures.rayTracingPipeline << std::endl;

        if (rtPipelineFeatures.rayTracingPipeline)
        {
            physicalDevice = phyDevice;

            // Accerlation structure properties.
            {
                phyDevAccStructProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
            }

            // Ray tracing properties.
            {
                phyDevRtPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
                phyDevRtPipelineProperties.pNext = &phyDevAccStructProperties;
            }

            VkPhysicalDeviceProperties2 phyDevRtProperties2 = {};
            {
                phyDevRtProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
                phyDevRtProperties2.pNext = &phyDevRtPipelineProperties;
            }

            vkGetPhysicalDeviceProperties2(phyDevice, &phyDevRtProperties2);

            std::cout << "Max recursion depth: " << phyDevRtPipelineProperties.maxRayRecursionDepth << std::endl;

            std::cout << "Has acceleration structure: " << phyDevAccStructFeatures.accelerationStructure << std::endl;

            std::cout << "Max acceleration structure geometry count: " << phyDevAccStructProperties.maxGeometryCount << std::endl;

            std::cout << "Has ray query: " << phyDevRayQueryFeatures.rayQuery << std::endl;
        }

        // Choose the device if the physical device supports all extensions
        std::unordered_set<std::string> requiredDeviceExtensionsCheckList(requiredDeviceExtensions.begin(), requiredDeviceExtensions.end());

        std::vector<VkExtensionProperties> phyDevExtensionProperties;
        uint32_t propertiesCnt;
        vkEnumerateDeviceExtensionProperties(phyDevice, NULL, &propertiesCnt, nullptr);
        phyDevExtensionProperties.resize(propertiesCnt);
        vkEnumerateDeviceExtensionProperties(phyDevice, NULL, &propertiesCnt, phyDevExtensionProperties.data());

        for (const VkExtensionProperties& prop : phyDevExtensionProperties)
        {
            requiredDeviceExtensionsCheckList.erase(prop.extensionName);
            if (requiredDeviceExtensionsCheckList.size() == 0)
            {
                std::cout << "Choose this physical device for ray tracing." << std::endl;
                break;
            }
        }

        // Only select the first supported physical device.
        if (requiredDeviceExtensionsCheckList.size() == 0)
        {
            break;
        }
    }

    // Find a queue that supports both compute and graphics.
    uint32_t queueFamilyPropCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropCount, nullptr);
    assert(queueFamilyPropCount >= 1);
    std::vector<VkQueueFamilyProperties> queueFamilyProps(queueFamilyPropCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropCount, queueFamilyProps.data());
    
    uint32_t queueId = ~0;
    // for (const VkQueueFamilyProperties& queueFamilyProperty : queueFamilyProps)
    for(uint32_t i = 0; i < queueFamilyProps.size(); i++)
    {
        const VkQueueFamilyProperties& queueFamilyProperty = queueFamilyProps[i];
        bool supportsGraphics = (queueFamilyProperty.queueFlags & VK_QUEUE_GRAPHICS_BIT) > 0;
        bool supportsCompute = (queueFamilyProperty.queueFlags & VK_QUEUE_COMPUTE_BIT) > 0;

        if (supportsGraphics && supportsCompute)
        {
            queueId = i;
            break;
        }
    }

    if (queueId == ~0)
    {
        std::cerr << "Unable to find a queue that supports both compute and graphic family" << std::endl;
    }

    // Create the logical device
    float queuePriority = 1.f;
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    {
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueId;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
    }

    // Setup required rt features
    VkPhysicalDeviceRayQueryFeaturesKHR phyDevRayQueryFeatures = {};
    {
        phyDevRayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
        phyDevRayQueryFeatures.rayQuery = VK_TRUE;
    }

    VkPhysicalDeviceBufferDeviceAddressFeatures phyDeviceBufferDeviceAddrFeatures = {};
    {
        phyDeviceBufferDeviceAddrFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
        phyDeviceBufferDeviceAddrFeatures.pNext = &phyDevRayQueryFeatures;
        phyDeviceBufferDeviceAddrFeatures.bufferDeviceAddress = true;
        phyDeviceBufferDeviceAddrFeatures.bufferDeviceAddressCaptureReplay = false;
        phyDeviceBufferDeviceAddrFeatures.bufferDeviceAddressMultiDevice = false;
    }

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR phyDeviceRTPipelineFeatures = {};
    {
        phyDeviceRTPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
        phyDeviceRTPipelineFeatures.pNext = &phyDeviceBufferDeviceAddrFeatures;
        phyDeviceRTPipelineFeatures.rayTracingPipeline = true;
    }

    VkPhysicalDeviceAccelerationStructureFeaturesKHR phyDeviceAccStructureFeatures = {};
    {
        phyDeviceAccStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        phyDeviceAccStructureFeatures.pNext = &phyDeviceRTPipelineFeatures;
        phyDeviceAccStructureFeatures.accelerationStructure = true;
        phyDeviceAccStructureFeatures.accelerationStructureCaptureReplay = true;
        phyDeviceAccStructureFeatures.accelerationStructureIndirectBuild = false;
        phyDeviceAccStructureFeatures.accelerationStructureHostCommands = false;
        phyDeviceAccStructureFeatures.descriptorBindingAccelerationStructureUpdateAfterBind = false;
    }

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeature{};
    {
        dynamicRenderingFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
        dynamicRenderingFeature.pNext = &phyDeviceAccStructureFeatures;
        dynamicRenderingFeature.dynamicRendering = VK_TRUE;
    }

    VkDeviceCreateInfo deviceInfo{};
    {
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.pNext = &dynamicRenderingFeature;
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.pQueueCreateInfos = &queueCreateInfo;
        deviceInfo.enabledExtensionCount = requiredDeviceExtensions.size();
        deviceInfo.ppEnabledExtensionNames = requiredDeviceExtensions.data();
        deviceInfo.pEnabledFeatures = nullptr;
    }

    // VkDevice device;
    VK_CHECK(vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device));

    // Get a graphics and compute queue
    VkQueue rtQueue;
    vkGetDeviceQueue(device, queueId, 0, &rtQueue);

    // Create a command buffer pool and allocate a command buffer
    VkCommandPoolCreateInfo cmdPoolInfo = {};
    {
        cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmdPoolInfo.queueFamilyIndex = queueId;
        cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    }
    VkCommandPool cmdPool;
    VK_CHECK(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &cmdPool));

    VkCommandBufferAllocateInfo cmdBufferAllocInfo{};
    {
        cmdBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdBufferAllocInfo.commandPool = cmdPool;
        cmdBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdBufferAllocInfo.commandBufferCount = 1;
    }
    VkCommandBuffer cmdBuffer;
    VK_CHECK(vkAllocateCommandBuffers(device, &cmdBufferAllocInfo, &cmdBuffer));

    // Init VMA
    VmaVulkanFunctions vkFuncs = {};
    {
        vkFuncs.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
        vkFuncs.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;
    }

    VmaAllocatorCreateInfo allocCreateInfo = {};
    {
        allocCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        allocCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
        allocCreateInfo.physicalDevice = physicalDevice;
        allocCreateInfo.device = device;
        allocCreateInfo.instance = instance;
        allocCreateInfo.pVulkanFunctions = &vkFuncs;
    }

    VmaAllocator allocator;
    vmaCreateAllocator(&allocCreateInfo, &allocator);

    // Init descriptors pool
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorPoolSize> poolSizes = {
            { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
    };
    
    VkDescriptorPoolCreateInfo poolInfo{};
    {
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = poolSizes.size();
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = 1;
    }

    VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool));

    //
    //
    // BLAS - Bottom Level Acceleration Structure (Verts/Tris)
    //
    //
    const uint32_t numTriangles = 1;
    const uint32_t idxCount = 3;

    float vertices[9] = {
        1.f, 1.f, 0.f,
        -1.f, 1.f, 0.f,
        0.f, -1.f, 0.f
    };

    uint32_t indices[3] = {0, 1, 2};

    // Note: RT doesn't have perspective so it's just <3, 4> x <4, 1>
    VkTransformMatrixKHR transformMatrix = {
        1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f
    };

    // Vertex buffer
    VkBuffer vertBuffer;
    VmaAllocation vertBufferAlloc;
    VkBufferCreateInfo vertBufferInfo = {};
    {
        vertBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        vertBufferInfo.size = sizeof(vertices);
        vertBufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        vertBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo vertBufferAllocInfo = {};
    {
        vertBufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        vertBufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
                                    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    vmaCreateBuffer(allocator, &vertBufferInfo, &vertBufferAllocInfo, &vertBuffer, &vertBufferAlloc, nullptr);
    
    void* pVertGpuAddr = nullptr;
    vmaMapMemory(allocator, vertBufferAlloc, &pVertGpuAddr);
    memcpy(pVertGpuAddr, vertices, sizeof(vertices));
    vmaUnmapMemory(allocator, vertBufferAlloc);

    VkDeviceOrHostAddressConstKHR vertBufferDeviceAddr = {};
    {
        VkBufferDeviceAddressInfo bufferDeviceAddrInfo = {};
        {
            bufferDeviceAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            bufferDeviceAddrInfo.buffer = vertBuffer;
        }
        vertBufferDeviceAddr.deviceAddress = vkGetBufferDeviceAddress(device, &bufferDeviceAddrInfo);
    }

    // Index buffer
    VkBuffer idxBuffer;
    VmaAllocation idxBufferAlloc;
    VkBufferCreateInfo idxBufferInfo = {};
    {
        idxBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        idxBufferInfo.size = sizeof(indices);
        idxBufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                              VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        idxBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo idxBufferAllocInfo = {};
    {
        idxBufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        idxBufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
                                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    vmaCreateBuffer(allocator, &idxBufferInfo, &idxBufferAllocInfo, &idxBuffer, &idxBufferAlloc, nullptr);

    void* pIdxGpuAddr = nullptr;
    vmaMapMemory(allocator, idxBufferAlloc, &pIdxGpuAddr);
    memcpy(pIdxGpuAddr, indices, sizeof(indices));
    vmaUnmapMemory(allocator, idxBufferAlloc);

    VkDeviceOrHostAddressConstKHR idxBufferDeviceAddr = {};
    {
        VkBufferDeviceAddressInfo bufferDeviceAddrInfo = {};
        {
            bufferDeviceAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            bufferDeviceAddrInfo.buffer = idxBuffer;
        }
        idxBufferDeviceAddr.deviceAddress = vkGetBufferDeviceAddress(device, &bufferDeviceAddrInfo);
    }

    // Transform matrix buffer
    VkBuffer transformMatrixBuffer;
    VmaAllocation transformMatrixAlloc;
    VkBufferCreateInfo transformMatrixBufferInfo = {};
    {
        transformMatrixBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        transformMatrixBufferInfo.size = sizeof(transformMatrix);
        transformMatrixBufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        transformMatrixBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo transformMatrixAllocInfo = {};
    {
        transformMatrixAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        transformMatrixAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
                                         VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    vmaCreateBuffer(allocator, &transformMatrixBufferInfo, &transformMatrixAllocInfo, &transformMatrixBuffer, &transformMatrixAlloc, nullptr);

    void* pTransformMatrixGpuAddr = nullptr;
    vmaMapMemory(allocator, transformMatrixAlloc, &pTransformMatrixGpuAddr);
    memcpy(pTransformMatrixGpuAddr, transformMatrix.matrix, sizeof(transformMatrix));
    vmaUnmapMemory(allocator, transformMatrixAlloc);

    VkDeviceOrHostAddressConstKHR transformMatBufferDeviceAddr = {};
    {
        VkBufferDeviceAddressInfo bufferDeviceAddrInfo = {};
        {
            bufferDeviceAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            bufferDeviceAddrInfo.buffer = transformMatrixBuffer;
        }
        transformMatBufferDeviceAddr.deviceAddress = vkGetBufferDeviceAddress(device, &bufferDeviceAddrInfo);
    }

    // Build the accerlation structure
    // Get the ray tracing and accelertion structure related function pointers required by this sample
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR"));
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR"));
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR"));
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR"));
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR"));
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR"));
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR"));
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR"));

    struct BottomLevelAccelerationStructure
    {
        VkAccelerationStructureKHR accStructure;
        VkBuffer accStructureBuffer;
        VmaAllocation accStructureBufferAlloc;
        uint64_t deviceAddress;
        VkBuffer scratchBuffer;
        VmaAllocation scratchBufferAlloc;
    };
    BottomLevelAccelerationStructure bottomLevelAccelerationStructure;

    VkAccelerationStructureGeometryKHR blasGeometry = {};
    {
        blasGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        blasGeometry.geometryType = VkGeometryTypeKHR::VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        blasGeometry.flags = VkGeometryFlagBitsKHR::VK_GEOMETRY_OPAQUE_BIT_KHR;
        {
            VkAccelerationStructureGeometryDataKHR accStructureGeoData = {};
            VkAccelerationStructureGeometryTrianglesDataKHR accStructureGeoTriData = {};
            {
                accStructureGeoTriData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
                accStructureGeoTriData.vertexFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
                accStructureGeoTriData.vertexData = vertBufferDeviceAddr;
                accStructureGeoTriData.vertexStride = sizeof(float) * 3;
                accStructureGeoTriData.maxVertex = 2;
                accStructureGeoTriData.indexType = VK_INDEX_TYPE_UINT32;
                accStructureGeoTriData.indexData = idxBufferDeviceAddr;
                accStructureGeoTriData.transformData = transformMatBufferDeviceAddr;
            }
            accStructureGeoData.triangles = accStructureGeoTriData;
            blasGeometry.geometry = accStructureGeoData;
        }
    }

    VkAccelerationStructureBuildGeometryInfoKHR blasBuildGeoInfo = {};
    {
        blasBuildGeoInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        blasBuildGeoInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        blasBuildGeoInfo.flags = VkBuildAccelerationStructureFlagBitsKHR::VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        blasBuildGeoInfo.mode = VkBuildAccelerationStructureModeKHR::VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        blasBuildGeoInfo.geometryCount = 1;
        blasBuildGeoInfo.pGeometries = &blasGeometry;
    }

    // Get BLAS size info
    VkAccelerationStructureBuildSizesInfoKHR blasBuildSizeInfo = {};
    {
        blasBuildSizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    }
    vkGetAccelerationStructureBuildSizesKHR(
        device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &blasBuildGeoInfo,
        &numTriangles,
        &blasBuildSizeInfo);

    // Create BLAS buffer
    VkBufferCreateInfo blasBufferInfo = {};
    {
        blasBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        blasBufferInfo.size  = blasBuildSizeInfo.accelerationStructureSize;
        blasBufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        blasBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo blasBufferAllocInfo = {};
    {
        blasBufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        blasBufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    vmaCreateBuffer(allocator,
                    &blasBufferInfo,
                    &blasBufferAllocInfo,
                    &bottomLevelAccelerationStructure.accStructureBuffer,
                    &bottomLevelAccelerationStructure.accStructureBufferAlloc,
                    nullptr);

    // Create BLAS scratch buffer
    VkBufferCreateInfo blasScratchBufferInfo = {};
    {
        blasScratchBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        blasScratchBufferInfo.size = blasBuildSizeInfo.buildScratchSize;
        blasScratchBufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        blasScratchBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo blasScratchBufferAllocInfo = {};
    {
        blasScratchBufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        blasScratchBufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    vmaCreateBuffer(allocator,
                    &blasScratchBufferInfo,
                    &blasScratchBufferAllocInfo,
                    &bottomLevelAccelerationStructure.scratchBuffer,
                    &bottomLevelAccelerationStructure.scratchBufferAlloc,
                    nullptr);

    VkAccelerationStructureCreateInfoKHR blasCreateInfo = {};
    {
        blasCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        blasCreateInfo.buffer = bottomLevelAccelerationStructure.accStructureBuffer;
        blasCreateInfo.size = blasBuildSizeInfo.accelerationStructureSize;
        blasCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    }
    vkCreateAccelerationStructureKHR(device, &blasCreateInfo, nullptr, &bottomLevelAccelerationStructure.accStructure);

    // Fill in the remaining BLAS meta info for building
    blasBuildGeoInfo.dstAccelerationStructure = bottomLevelAccelerationStructure.accStructure;
    VkBufferDeviceAddressInfo blasBuildScratchBufferDevAddrInfo = {};
    {
        blasBuildScratchBufferDevAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        blasBuildScratchBufferDevAddrInfo.buffer = bottomLevelAccelerationStructure.scratchBuffer;
    }
    blasBuildGeoInfo.scratchData.deviceAddress = vkGetBufferDeviceAddress(device, &blasBuildScratchBufferDevAddrInfo);
    
    VkAccelerationStructureBuildRangeInfoKHR blasBuildRangeInfo = {};
    {
        blasBuildRangeInfo.primitiveCount = numTriangles;
        blasBuildRangeInfo.primitiveOffset = 0;
        blasBuildRangeInfo.firstVertex = 0;
        blasBuildRangeInfo.transformOffset = 0;
    }
    VkAccelerationStructureBuildRangeInfoKHR* blasBuildRangeInfos[1] = { &blasBuildRangeInfo };

    // Tell the GPU and driver to build the BLAS.
    VkCommandBufferBeginInfo cmdBufferBeginInfo = {};
    {
        cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    }
    
    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &cmdBufferBeginInfo));

    vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &blasBuildGeoInfo, blasBuildRangeInfos);

    VK_CHECK(vkEndCommandBuffer(cmdBuffer));

    VkSubmitInfo submitInfo{};
    {
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pCommandBuffers = &cmdBuffer;
        submitInfo.commandBufferCount = 1;
    }
    VK_CHECK(vkQueueSubmit(rtQueue, 1, &submitInfo, VK_NULL_HANDLE));
    
    vkDeviceWaitIdle(device);

    VkAccelerationStructureDeviceAddressInfoKHR blasAddressInfo = {};
    {
        blasAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        blasAddressInfo.accelerationStructure = bottomLevelAccelerationStructure.accStructure;
    }
    bottomLevelAccelerationStructure.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &blasAddressInfo);
    
    vkResetCommandBuffer(cmdBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

    // Destroy build scratch buffers
    vmaDestroyBuffer(allocator,
                     bottomLevelAccelerationStructure.scratchBuffer,
                     bottomLevelAccelerationStructure.scratchBufferAlloc);

    /*
        Build the TLAS. The top level acceleration structure contains the scene's object instances
    */
    struct TopLevelAccelerationStructure
    {
        VkAccelerationStructureKHR accStructure;
        VkBuffer accStructureBuffer;
        VmaAllocation accStructureBufferAlloc;
        uint64_t deviceAddress;
        VkBuffer scratchBuffer;
        VmaAllocation scratchBufferAlloc;
        VkBuffer instancesBuffer;
        VmaAllocation instancesBufferAlloc;
    };
    TopLevelAccelerationStructure topLevelAccelerationStructure{};

    VkAccelerationStructureInstanceKHR tlasInstance = {};
    {
        tlasInstance.transform = transformMatrix;
        tlasInstance.instanceCustomIndex = 0;
        tlasInstance.mask = 0xff;
        tlasInstance.instanceShaderBindingTableRecordOffset = 0;
        tlasInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        tlasInstance.accelerationStructureReference = bottomLevelAccelerationStructure.deviceAddress;
    }

    VkBufferCreateInfo instBufferInfo = {};
    {
        instBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        instBufferInfo.size = sizeof(VkAccelerationStructureInstanceKHR);
        instBufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        instBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo instBufferAllocInfo = {};
    {
        instBufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        instBufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
                                    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    vmaCreateBuffer(allocator,
                    &instBufferInfo,
                    &instBufferAllocInfo,
                    &topLevelAccelerationStructure.instancesBuffer,
                    &topLevelAccelerationStructure.instancesBufferAlloc,
                    nullptr);

    void* pInstBuffer;
    vmaMapMemory(allocator, topLevelAccelerationStructure.instancesBufferAlloc, &pInstBuffer);
    memcpy(pInstBuffer, &tlasInstance, sizeof(tlasInstance));
    vmaUnmapMemory(allocator, topLevelAccelerationStructure.instancesBufferAlloc);

    VkDeviceOrHostAddressConstKHR instBufferDeviceAddr = {};
    {
        VkBufferDeviceAddressInfo bufferDeviceAddrInfo = {};
        {
            bufferDeviceAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            bufferDeviceAddrInfo.buffer = topLevelAccelerationStructure.instancesBuffer;
        }
        instBufferDeviceAddr.deviceAddress = vkGetBufferDeviceAddress(device, &bufferDeviceAddrInfo);
    }

    VkAccelerationStructureGeometryKHR tlasStructureGeo = {};
    {
        tlasStructureGeo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        tlasStructureGeo.geometryType = VkGeometryTypeKHR::VK_GEOMETRY_TYPE_INSTANCES_KHR;
        tlasStructureGeo.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        tlasStructureGeo.geometry.instances.arrayOfPointers = VK_FALSE;
        tlasStructureGeo.geometry.instances.data = instBufferDeviceAddr;
    }

    VkAccelerationStructureBuildGeometryInfoKHR tlasBuildGeoInfo{};
    {
        tlasBuildGeoInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        tlasBuildGeoInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        tlasBuildGeoInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        tlasBuildGeoInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        tlasBuildGeoInfo.geometryCount = 1;
        tlasBuildGeoInfo.pGeometries = &tlasStructureGeo;
    }

    VkAccelerationStructureBuildSizesInfoKHR tlasBuildSizesInfo{};
    {
        tlasBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    }

    vkGetAccelerationStructureBuildSizesKHR(
        device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &tlasBuildGeoInfo,
        &numTriangles,
        &tlasBuildSizesInfo);

    VkBufferCreateInfo tlasBufferInfo = {};
    {
        tlasBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        tlasBufferInfo.size = tlasBuildSizesInfo.accelerationStructureSize;
        tlasBufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        tlasBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo tlasBufferAllocInfo = {};
    {
        tlasBufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        tlasBufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    vmaCreateBuffer(allocator,
                    &tlasBufferInfo,
                    &tlasBufferAllocInfo,
                    &topLevelAccelerationStructure.accStructureBuffer,
                    &topLevelAccelerationStructure.accStructureBufferAlloc,
                    nullptr);

    // Create TLAS scratch buffer
    VkBufferCreateInfo tlasScratchBufferInfo = {};
    {
        tlasScratchBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        tlasScratchBufferInfo.size = tlasBuildSizesInfo.buildScratchSize;
        tlasScratchBufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        tlasScratchBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo tlasScratchBufferAllocInfo = {};
    {
        tlasScratchBufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        tlasScratchBufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    vmaCreateBuffer(allocator,
                    &tlasScratchBufferInfo,
                    &tlasScratchBufferAllocInfo,
                    &topLevelAccelerationStructure.scratchBuffer,
                    &topLevelAccelerationStructure.scratchBufferAlloc,
                    nullptr);

    VkAccelerationStructureCreateInfoKHR tlasCreateInfo = {};
    {
        tlasCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        tlasCreateInfo.buffer = topLevelAccelerationStructure.accStructureBuffer;
        tlasCreateInfo.size = tlasBuildSizesInfo.accelerationStructureSize;
        tlasCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    }
    vkCreateAccelerationStructureKHR(device, &tlasCreateInfo, nullptr, &topLevelAccelerationStructure.accStructure);
    tlasBuildGeoInfo.dstAccelerationStructure = topLevelAccelerationStructure.accStructure;

    VkBufferDeviceAddressInfo tlasBuildScratchBufferDevAddrInfo = {};
    {
        tlasBuildScratchBufferDevAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        tlasBuildScratchBufferDevAddrInfo.buffer = topLevelAccelerationStructure.scratchBuffer;
    }
    tlasBuildGeoInfo.scratchData.deviceAddress = vkGetBufferDeviceAddress(device, &tlasBuildScratchBufferDevAddrInfo);

    VkAccelerationStructureBuildRangeInfoKHR tlasBuildRangeInfo = {};
    {
        tlasBuildRangeInfo.primitiveCount = 1;
        tlasBuildRangeInfo.primitiveOffset = 0;
        tlasBuildRangeInfo.firstVertex = 0;
        tlasBuildRangeInfo.transformOffset = 0;
    }
    VkAccelerationStructureBuildRangeInfoKHR* tlasBuildRangeInfos[1] = { &tlasBuildRangeInfo };

    // Tell the GPU and driver to build the TLAS.
    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &cmdBufferBeginInfo));

    vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &tlasBuildGeoInfo, tlasBuildRangeInfos);

    VK_CHECK(vkEndCommandBuffer(cmdBuffer));

    

    vkDeviceWaitIdle(device);

    vmaDestroyBuffer(allocator,
                     topLevelAccelerationStructure.scratchBuffer,
                     topLevelAccelerationStructure.scratchBufferAlloc);

    VkAccelerationStructureDeviceAddressInfoKHR tlasAddressInfo = {};
    {
        tlasAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        tlasAddressInfo.accelerationStructure = topLevelAccelerationStructure.accStructure;
    }
    topLevelAccelerationStructure.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &tlasAddressInfo);

    /*
    * Create shader modules and ray tracing pipeline.
    */
    VkDescriptorSetLayoutBinding tlasLayoutBinding = {};
    {
        tlasLayoutBinding.binding = 0;
        tlasLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        tlasLayoutBinding.descriptorCount = 1;
        tlasLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    }

    VkDescriptorSetLayoutBinding resultImageLayoutBinding = {};
    {
        resultImageLayoutBinding.binding = 1;
        resultImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        resultImageLayoutBinding.descriptorCount = 1;
        resultImageLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    }

    std::vector<VkDescriptorSetLayoutBinding> bindings({
        tlasLayoutBinding, resultImageLayoutBinding
        });

    VkDescriptorSetLayoutCreateInfo desSetLayoutInfo = {};
    {
        desSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        desSetLayoutInfo.bindingCount = bindings.size();
        desSetLayoutInfo.pBindings = bindings.data();
    }
    VkDescriptorSetLayout desSetLayout;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &desSetLayoutInfo, nullptr, &desSetLayout));

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    {
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &desSetLayout;
    }
    VkPipelineLayout pipelineLayout;
    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout));

    std::vector<VkPipelineShaderStageCreateInfo>      shaderStages;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups;

    // Ray generation group
    VkShaderModule rgenShaderModule = createShaderModule("/hlsl/dumpTri_rgen.spv", device);
    {
        VkPipelineShaderStageCreateInfo rgenStageInfo = {};
        {
            rgenStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            rgenStageInfo.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
            rgenStageInfo.pName = "main";
            rgenStageInfo.module = rgenShaderModule;
        }
        shaderStages.push_back(rgenStageInfo);

        VkRayTracingShaderGroupCreateInfoKHR shaderGroup = {};
        {
            shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
            shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
            shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
            shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
            shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
        }
        shaderGroups.push_back(shaderGroup);
    }

    // Miss group
    VkShaderModule rmissShaderModule = createShaderModule("./hlsl/dumpTri_rmiss.spv", device);
    {
        VkPipelineShaderStageCreateInfo rmissStageInfo = {};
        {
            rmissStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            rmissStageInfo.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
            rmissStageInfo.pName = "main";
            rmissStageInfo.module = rmissShaderModule;
        }
        shaderStages.push_back(rmissStageInfo);

        VkRayTracingShaderGroupCreateInfoKHR shaderGroup = {};
        {
            shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
            shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
            shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
            shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
            shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
        }
        shaderGroups.push_back(shaderGroup);
    }

    // Closest hit group
    VkShaderModule rchitShaderModule = createShaderModule("./hlsl/dumpTri_rchit.spv", device);
    {
        VkPipelineShaderStageCreateInfo rchitStageInfo = {};
        {
            rchitStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            rchitStageInfo.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
            rchitStageInfo.pName = "main";
            rchitStageInfo.module = rchitShaderModule;
        }
        shaderStages.push_back(rchitStageInfo);

        VkRayTracingShaderGroupCreateInfoKHR shaderGroup = {};
        {
            shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
            shaderGroup.generalShader = VK_SHADER_UNUSED_KHR;
            shaderGroup.closestHitShader = static_cast<uint32_t>(shaderStages.size()) - 1;
            shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
            shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
        }
        shaderGroups.push_back(shaderGroup);
    }

    VkRayTracingPipelineCreateInfoKHR rayTracingPipelineInfo = {};
    {
        rayTracingPipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
        rayTracingPipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
        rayTracingPipelineInfo.pStages = shaderStages.data();
        rayTracingPipelineInfo.groupCount = static_cast<uint32_t>(shaderGroups.size());
        rayTracingPipelineInfo.pGroups = shaderGroups.data();
        rayTracingPipelineInfo.maxPipelineRayRecursionDepth = 1;
        rayTracingPipelineInfo.layout = pipelineLayout;
    }
    VkPipeline rtPipeline;
    VK_CHECK(vkCreateRayTracingPipelinesKHR(device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayTracingPipelineInfo, nullptr, &rtPipeline));

    /*
    * Build the Shader Binding Table
    */
    VkBuffer rgenShaderGroupHandle;
    VmaAllocation rgenShaderGroupHandleAlloc;
    VkStridedDeviceAddressRegionKHR rgenShaderSbtEntry = {};

    VkBuffer rmissShaderGroupHandle;
    VmaAllocation rmissShaderGroupHandleAlloc;
    VkStridedDeviceAddressRegionKHR rmissShaderSbtEntry = {};

    VkBuffer rchitShaderGroupHandle;
    VmaAllocation rchitShaderGroupHandleAlloc;
    VkStridedDeviceAddressRegionKHR rchitShaderSbtEntry = {};

    {
        const uint32_t handleSize = phyDevRtPipelineProperties.shaderGroupHandleSize;
        const uint32_t alignedHandleSize = alignedSize(handleSize, phyDevRtPipelineProperties.shaderGroupHandleAlignment);
        const uint32_t groupCount = static_cast<uint32_t>(shaderGroups.size());
        const uint32_t stbSize = groupCount * alignedHandleSize;

        std::vector<uint8_t> shaderHandles(stbSize);
        VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(device, rtPipeline, 0, groupCount, stbSize, shaderHandles.data()));

        const VkBufferUsageFlags shaderGroupHandleBufferUsageFlags = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        const VmaAllocationCreateFlags shaderGroupHandleAllocFlags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        VkBufferCreateInfo shaderGroupHandleBufferInfo = {};
        {
            shaderGroupHandleBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            shaderGroupHandleBufferInfo.size = handleSize;
            shaderGroupHandleBufferInfo.usage = shaderGroupHandleBufferUsageFlags;
            shaderGroupHandleBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        VmaAllocationCreateInfo shaderGroupHandleBufferAllocInfo = {};
        {
            shaderGroupHandleBufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            shaderGroupHandleBufferAllocInfo.flags = shaderGroupHandleAllocFlags;
        }

        // Ray generation
        {
            vmaCreateBuffer(allocator, &shaderGroupHandleBufferInfo, &shaderGroupHandleBufferAllocInfo, &rgenShaderGroupHandle, &rgenShaderGroupHandleAlloc, nullptr);

            void* pShaderGroupHandle;
            vmaMapMemory(allocator, rgenShaderGroupHandleAlloc, &pShaderGroupHandle);
            memcpy(pShaderGroupHandle, shaderHandles.data(), handleSize);
            vmaUnmapMemory(allocator, rgenShaderGroupHandleAlloc);

            VkBufferDeviceAddressInfo bufferDeviceAddrInfo = {};
            {
                bufferDeviceAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
                bufferDeviceAddrInfo.buffer = rgenShaderGroupHandle;
            }
            rgenShaderSbtEntry.deviceAddress = vkGetBufferDeviceAddress(device, &bufferDeviceAddrInfo);
            rgenShaderSbtEntry.stride = alignedHandleSize;
            rgenShaderSbtEntry.size = alignedHandleSize;
        }

        // Ray miss
        {
            vmaCreateBuffer(allocator, &shaderGroupHandleBufferInfo, &shaderGroupHandleBufferAllocInfo, &rmissShaderGroupHandle, &rmissShaderGroupHandleAlloc, nullptr);

            void* pShaderGroupHandle;
            vmaMapMemory(allocator, rmissShaderGroupHandleAlloc, &pShaderGroupHandle);
            memcpy(pShaderGroupHandle, shaderHandles.data() + alignedHandleSize, handleSize);
            vmaUnmapMemory(allocator, rmissShaderGroupHandleAlloc);

            VkBufferDeviceAddressInfo bufferDeviceAddrInfo = {};
            {
                bufferDeviceAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
                bufferDeviceAddrInfo.buffer = rmissShaderGroupHandle;
            }
            rmissShaderSbtEntry.deviceAddress = vkGetBufferDeviceAddress(device, &bufferDeviceAddrInfo);
            rmissShaderSbtEntry.stride = alignedHandleSize;
            rmissShaderSbtEntry.size = alignedHandleSize;
        }

        // Ray closest hit
        {
            vmaCreateBuffer(allocator, &shaderGroupHandleBufferInfo, &shaderGroupHandleBufferAllocInfo, &rchitShaderGroupHandle, &rchitShaderGroupHandleAlloc, nullptr);

            void* pShaderGroupHandle;
            vmaMapMemory(allocator, rchitShaderGroupHandleAlloc, &pShaderGroupHandle);
            memcpy(pShaderGroupHandle, shaderHandles.data() + 2 * alignedHandleSize, handleSize);
            vmaUnmapMemory(allocator, rchitShaderGroupHandleAlloc);

            VkBufferDeviceAddressInfo bufferDeviceAddrInfo = {};
            {
                bufferDeviceAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
                bufferDeviceAddrInfo.buffer = rchitShaderGroupHandle;
            }
            rchitShaderSbtEntry.deviceAddress = vkGetBufferDeviceAddress(device, &bufferDeviceAddrInfo);
            rchitShaderSbtEntry.stride = alignedHandleSize;
            rchitShaderSbtEntry.size = alignedHandleSize;
        }
    }

    /*
    * Create the storage image
    */
    VkImage       storageImage;
    VmaAllocation storageImageAlloc;
    VkImageView   storageImageView;

    VkBuffer      storageImgDumpBuffer;
    VmaAllocation storageImgDumpBufferAlloc;

    VkImageCreateInfo imageInfo = {};
    {
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageInfo.extent.width = 1024;
        imageInfo.extent.height = 1024;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    }

    VmaAllocationCreateInfo imageAllocInfo = {};
    {
        imageAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        imageAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    vmaCreateImage(allocator, &imageInfo, &imageAllocInfo, &storageImage, &storageImageAlloc, nullptr);

    VkImageViewCreateInfo storageImageViewInfo = {};
    {
        storageImageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        storageImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        storageImageViewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        storageImageViewInfo.image = storageImage;
        storageImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        storageImageViewInfo.subresourceRange.baseMipLevel = 0;
        storageImageViewInfo.subresourceRange.levelCount = 1;
        storageImageViewInfo.subresourceRange.baseArrayLayer = 0;
        storageImageViewInfo.subresourceRange.layerCount = 1;
        storageImageViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        storageImageViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        storageImageViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        storageImageViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    }

    VK_CHECK(vkCreateImageView(device, &storageImageViewInfo, nullptr, &storageImageView));

    VkBufferCreateInfo storageImgDumpBufferInfo = {};
    {
        storageImgDumpBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        storageImgDumpBufferInfo.size = sizeof(uint8_t) * 4 * 1024 * 1024;
        storageImgDumpBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        storageImgDumpBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo storageImgDumpBufferAllocInfo = {};
    {
        storageImgDumpBufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        storageImgDumpBufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
                                              VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    vmaCreateBuffer(allocator,
                    &storageImgDumpBufferInfo,
                    &storageImgDumpBufferAllocInfo,
                    &storageImgDumpBuffer,
                    &storageImgDumpBufferAlloc, nullptr);


    /*
    *   Create the descriptor sets used for the ray tracing dispatch
    */
    VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {};
    {
        descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorSetAllocInfo.descriptorPool = descriptorPool;
        descriptorSetAllocInfo.descriptorSetCount = 1;
        descriptorSetAllocInfo.pSetLayouts = &desSetLayout;
    }
    VkDescriptorSet rtDescriptorSet;
    vkAllocateDescriptorSets(device, &descriptorSetAllocInfo, &rtDescriptorSet);

    VkWriteDescriptorSetAccelerationStructureKHR tlasDescriptorInfo = {};
    {
        tlasDescriptorInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        tlasDescriptorInfo.accelerationStructureCount = 1;
        tlasDescriptorInfo.pAccelerationStructures = &topLevelAccelerationStructure.accStructure;
    }

    VkWriteDescriptorSet tlasDescriptorWrite = {};
    {
        tlasDescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        tlasDescriptorWrite.pNext = &tlasDescriptorInfo;
        tlasDescriptorWrite.dstSet = rtDescriptorSet;
        tlasDescriptorWrite.dstBinding = 0;
        tlasDescriptorWrite.descriptorCount = 1;
        tlasDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    }

    VkDescriptorImageInfo storageImgDescriptorInfo{};
    {
        storageImgDescriptorInfo.imageView = storageImageView;
        storageImgDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    }

    VkWriteDescriptorSet storageImgDescriptorWrite = {};
    {
        storageImgDescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        storageImgDescriptorWrite.dstSet = rtDescriptorSet;
        storageImgDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        storageImgDescriptorWrite.dstBinding = 1;
        storageImgDescriptorWrite.pImageInfo = &storageImgDescriptorInfo;
        storageImgDescriptorWrite.descriptorCount = 1;
    }

    VkWriteDescriptorSet descriptorSetWrites[2] = {
        tlasDescriptorWrite, storageImgDescriptorWrite
    };

    vkUpdateDescriptorSets(device, 2, descriptorSetWrites, 0, nullptr);

    /*
    *   Fill the command buffer and kick off the job.
    */
    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &cmdBufferBeginInfo));

    {
        // Transfer the storage image from layout unknown to VK_IMAGE_LAYOUT_GENERAL.
        VkImageMemoryBarrier storageImgToGeneralLayoutBarrier{};
        {
            storageImgToGeneralLayoutBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            storageImgToGeneralLayoutBarrier.srcAccessMask = VK_ACCESS_NONE;
            storageImgToGeneralLayoutBarrier.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
            storageImgToGeneralLayoutBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            storageImgToGeneralLayoutBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            storageImgToGeneralLayoutBarrier.image = storageImage;
            storageImgToGeneralLayoutBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            storageImgToGeneralLayoutBarrier.subresourceRange.baseMipLevel = 0;
            storageImgToGeneralLayoutBarrier.subresourceRange.levelCount = 1;
            storageImgToGeneralLayoutBarrier.subresourceRange.baseArrayLayer = 0;
            storageImgToGeneralLayoutBarrier.subresourceRange.layerCount = 1;
        }

        vkCmdPipelineBarrier(cmdBuffer,
            VK_PIPELINE_STAGE_HOST_BIT,
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            0,
            0, nullptr,
            0, nullptr,
            1, &storageImgToGeneralLayoutBarrier);

        // Dispatch rays
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline);
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, 1, &rtDescriptorSet, 0, nullptr);

        VkStridedDeviceAddressRegionKHR dummyShaderSbtEntry{};
        vkCmdTraceRaysKHR(cmdBuffer, &rgenShaderSbtEntry, &rmissShaderSbtEntry, &rchitShaderSbtEntry, &dummyShaderSbtEntry, 1024, 1024, 1);

        // Transfer the storage image from layout VK_IMAGE_LAYOUT_GENERAL to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL.
        VkImageMemoryBarrier storageImgToTransSrcLayoutBarrier{};
        {
            storageImgToTransSrcLayoutBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            storageImgToTransSrcLayoutBarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
            storageImgToTransSrcLayoutBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            storageImgToTransSrcLayoutBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            storageImgToTransSrcLayoutBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            storageImgToTransSrcLayoutBarrier.image = storageImage;
            storageImgToTransSrcLayoutBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            storageImgToTransSrcLayoutBarrier.subresourceRange.baseMipLevel = 0;
            storageImgToTransSrcLayoutBarrier.subresourceRange.levelCount = 1;
            storageImgToTransSrcLayoutBarrier.subresourceRange.baseArrayLayer = 0;
            storageImgToTransSrcLayoutBarrier.subresourceRange.layerCount = 1;
        }

        vkCmdPipelineBarrier(cmdBuffer,
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &storageImgToTransSrcLayoutBarrier);

        // Copy image data out into a staging buffer
        VkBufferImageCopy imgToBufferCopy{};
        {
            imgToBufferCopy.bufferRowLength = 1024;
            imgToBufferCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imgToBufferCopy.imageSubresource.baseArrayLayer = 0;
            imgToBufferCopy.imageSubresource.layerCount = 1;
            imgToBufferCopy.imageSubresource.mipLevel = 0;
            imgToBufferCopy.imageExtent.width = 1024;
            imgToBufferCopy.imageExtent.height = 1024;
            imgToBufferCopy.imageExtent.depth = 1;
        }

        vkCmdCopyImageToBuffer(cmdBuffer,
                               storageImage,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               storageImgDumpBuffer,
                               1, &imgToBufferCopy);
    }

    VK_CHECK(vkEndCommandBuffer(cmdBuffer));

    // Submit the command buffer, wait until the work done and dump the image data on disk.
    VK_CHECK(vkQueueSubmit(rtQueue, 1, &submitInfo, VK_NULL_HANDLE));

    vkDeviceWaitIdle(device);

    // Map out the data
    const int dataInBytes = 4 * sizeof(uint8_t) * 1024 * 1024;
    void* pStorageImgDumpBufferData;
    std::vector<uint8_t> ramStorageImgDumpData(dataInBytes);
    vmaMapMemory(allocator, storageImgDumpBufferAlloc, &pStorageImgDumpBufferData);
    memcpy(ramStorageImgDumpData.data(), pStorageImgDumpBufferData, dataInBytes);
    vmaUnmapMemory(allocator, storageImgDumpBufferAlloc);

    std::string pathName = std::string(SOURCE_PATH) + std::string("/test.png");
    std::cout << pathName << std::endl;
    unsigned int error = lodepng::encode(pathName, ramStorageImgDumpData.data(), 1024, 1024);
    if (error) { std::cout << "encoder error " << error << ": " << lodepng_error_text(error) << std::endl; }

    // Destroy the image and its image view
    vkDestroyImageView(device, storageImageView, nullptr);
    vmaDestroyImage(allocator, storageImage, storageImageAlloc);
    vmaDestroyBuffer(allocator, storageImgDumpBuffer, storageImgDumpBufferAlloc);

    // Destroy the shader group handles
    vmaDestroyBuffer(allocator, rgenShaderGroupHandle, rgenShaderGroupHandleAlloc);
    vmaDestroyBuffer(allocator, rmissShaderGroupHandle, rmissShaderGroupHandleAlloc);
    vmaDestroyBuffer(allocator, rchitShaderGroupHandle, rchitShaderGroupHandleAlloc);

    // Destroy the ray tracing pipeline
    vkDestroyShaderModule(device, rgenShaderModule, nullptr);
    vkDestroyShaderModule(device, rmissShaderModule, nullptr);
    vkDestroyShaderModule(device, rchitShaderModule, nullptr);
    vkDestroyDescriptorSetLayout(device, desSetLayout, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyPipeline(device, rtPipeline, nullptr);

    // Destroy TLAS structure
    vkDestroyAccelerationStructureKHR(device, topLevelAccelerationStructure.accStructure, nullptr);

    vmaDestroyBuffer(allocator,
                     topLevelAccelerationStructure.accStructureBuffer,
                     topLevelAccelerationStructure.accStructureBufferAlloc);
    vmaDestroyBuffer(allocator,
                     topLevelAccelerationStructure.instancesBuffer,
                     topLevelAccelerationStructure.instancesBufferAlloc);

    // Destroy BLAS structure
    vkDestroyAccelerationStructureKHR(device, bottomLevelAccelerationStructure.accStructure, nullptr);

    // Destroy buffers
    vmaDestroyBuffer(allocator,
                     bottomLevelAccelerationStructure.accStructureBuffer,
                     bottomLevelAccelerationStructure.accStructureBufferAlloc);
    vmaDestroyBuffer(allocator, idxBuffer, idxBufferAlloc);
    vmaDestroyBuffer(allocator, vertBuffer, vertBufferAlloc);
    vmaDestroyBuffer(allocator, transformMatrixBuffer, transformMatrixAlloc);

    // Destroy the command pool and release the command buffer
    vkFreeCommandBuffers(device, cmdPool, 1, &cmdBuffer);
    vkDestroyCommandPool(device, cmdPool, nullptr);

    // Destroy the descriptor pool
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);

    // Destroy the allocator
    vmaDestroyAllocator(allocator);

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
