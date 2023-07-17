#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "../../3-00_SharedLibrary/Camera.h"
#include "../../3-00_SharedLibrary/Event.h"
#include "../../3-00_SharedLibrary/MathUtils.h"

#include <vulkan/vulkan.h>
#include <glfw3.h>
#include "hdrloader.h"

#include <vector>
#include <set>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <fstream>

#include "PBREnivBasicApp.h"

// TODO1: Make the application, realtime swapchain application class for the Level 3 examples.

#define _USE_MATH_DEFINES
#include <math.h>

bool framebufferResized = false;

static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    framebufferResized = true;
}

const int MAX_FRAMES_IN_FLIGHT = 2;
uint32_t currentFrame = 0;
VkPhysicalDevice physicalDevice;
VkDevice device;
VkSurfaceKHR surface;
VkSwapchainKHR swapchain;
GLFWwindow* window = nullptr;
unsigned int graphicsQueueFamilyIdx = -1;
unsigned int presentQueueFamilyIdx = -1;
VkSurfaceFormatKHR choisenSurfaceFormat;
VkExtent2D swapchainImageExtent;

// Resources need to be preserved and used for the entire rendering.
std::vector<VkImageView> swapchainImageViews;
std::vector<VkImage> swapchainImages;

VkImage hdrCubeMapImage;
VkImageView hdrCubeMapView;
VkSampler hdrSampler;
VmaAllocation hdrCubeMapAlloc;

SharedLib::Camera camera;
std::vector<VkBuffer> cameraParaBuffers;
std::vector<VmaAllocation> cameraParaBufferAllocs;
std::vector<VkDescriptorSet> skyboxPipelineDescriptorSet0s;

VkDescriptorPool descriptorPool;
VmaAllocator allocator;

bool isDown = false;

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS)
    {
        isDown = true;
    }

    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_RELEASE)
    {
        isDown = false;
    }
}

// Create Camera related buffer, UBO objects
void CreateCameraUboObjects()
{
    // The alignment of a vec3 is 4 floats and the element alignment of a struct is the largest element alignment,
    // which is also the 4 float. Therefore, we need 16 floats as the buffer to store the Camera's parameters.
    VkBufferCreateInfo bufferInfo{};
    {
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = 16 * sizeof(float);
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo bufferAllocInfo{};
    {
        bufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        bufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
                                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    cameraParaBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    cameraParaBufferAllocs.resize(MAX_FRAMES_IN_FLIGHT);
    
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vmaCreateBuffer(allocator, &bufferInfo, &bufferAllocInfo, &cameraParaBuffers[i], &cameraParaBufferAllocs[i], nullptr);
    }
}

void DestroyCameraUboObjects()
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vmaDestroyBuffer(allocator, cameraParaBuffers[i], cameraParaBufferAllocs[i]);
    }
}

// Create HDR releted objects
void CreateHdrRenderObjects(
    const HDRLoaderResult& hdrLoadRes)
{
    VmaAllocationCreateInfo hdrAllocInfo{};
    {
        hdrAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        hdrAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    VkExtent3D extent{};
    {
        extent.width = hdrLoadRes.width / 6;
        extent.height = hdrLoadRes.height;
        extent.depth = 1;
    }

    VkImageCreateInfo cubeMapImgInfo{};
    {
        cubeMapImgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        cubeMapImgInfo.imageType = VK_IMAGE_TYPE_2D;
        cubeMapImgInfo.format = VK_FORMAT_R32G32B32_SFLOAT;
        cubeMapImgInfo.extent = extent;
        cubeMapImgInfo.mipLevels = 1;
        cubeMapImgInfo.arrayLayers = 6;
        cubeMapImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        cubeMapImgInfo.tiling = VK_IMAGE_TILING_LINEAR;
        cubeMapImgInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        cubeMapImgInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        cubeMapImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    VK_CHECK(vmaCreateImage(allocator, &cubeMapImgInfo, &hdrAllocInfo, &hdrCubeMapImage, &hdrCubeMapAlloc, nullptr));
    
    VkImageViewCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.image = hdrCubeMapImage;
    info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    info.format = VK_FORMAT_R32G32B32_SFLOAT;
    info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    info.subresourceRange.levelCount = 1;
    info.subresourceRange.layerCount = 6;

    VK_CHECK(vkCreateImageView(device, &info, nullptr, &hdrCubeMapView));

    VkSamplerCreateInfo sampler_info{};
    {
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT; // outside image bounds just use border color
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.minLod = -1000;
        sampler_info.maxLod = 1000;
        sampler_info.maxAnisotropy = 1.0f;
    }
    VK_CHECK(vkCreateSampler(device, &sampler_info, nullptr, &hdrSampler));
}

// Destroy HDR related objects
void DestroyHdrRenderObjs()
{
    vmaDestroyImage(allocator, hdrCubeMapImage, hdrCubeMapAlloc);
    vkDestroyImageView(device, hdrCubeMapView, nullptr);
    vkDestroySampler(device, hdrSampler, nullptr);
}

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
        createInfo.format = choisenSurfaceFormat.format;
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

    // Choose the surface format that supports VK_FORMAT_B8G8R8A8_SRGB and color space VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
    bool foundFormat = false;
    for (auto curFormat : surfaceFormats)
    {
        if (curFormat.format == VK_FORMAT_B8G8R8A8_SRGB && curFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            foundFormat = true;
            choisenSurfaceFormat = curFormat;
            break;
        }
    }
    assert(foundFormat);

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

    uint32_t queueFamiliesIndices[] = { graphicsQueueFamilyIdx, presentQueueFamilyIdx };
    VkSwapchainCreateInfoKHR swapchainCreateInfo{};
    {
        swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainCreateInfo.surface = surface;
        swapchainCreateInfo.minImageCount = imageCount;
        swapchainCreateInfo.imageFormat = choisenSurfaceFormat.format;
        swapchainCreateInfo.imageColorSpace = choisenSurfaceFormat.colorSpace;
        swapchainCreateInfo.imageExtent = swapchainImageExtent;
        swapchainCreateInfo.imageArrayLayers = 1;
        swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if (graphicsQueueFamilyIdx != presentQueueFamilyIdx)
        {
            swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            swapchainCreateInfo.queueFamilyIndexCount = 2;
            swapchainCreateInfo.pQueueFamilyIndices = queueFamiliesIndices;
        }
        else
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
        if (strcmp(props[i].extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
        {
            break;
        }
        if (i == propNum - 1)
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
    for (uint32_t i = 0; i < layerNum; ++i)
    {
        if (strcmp("VK_LAYER_KHRONOS_validation", layers[i].layerName) == 0)
        {
            break;
        }
        if (i == layerNum - 1)
        {
            std::cout << "Something went very wrong, cannot find VK_LAYER_KHRONOS_validation extension" << std::endl;
            exit(1);
        }
    }

    // Init glfw and get the glfw required extension. NOTE: Initialize GLFW before calling any function that requires initialization.
    glfwInit();
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    // Initialize instance and application
    VkApplicationInfo appInfo{};
    {
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "DynamicRendering";
        appInfo.applicationVersion = 1;
        appInfo.pEngineName = "VulkanDict";
        appInfo.engineVersion = 1;
        appInfo.apiVersion = VK_API_VERSION_1_3;
    }

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
    VkInstanceCreateInfo instanceCreateInfo{};
    {
        instanceCreateInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceCreateInfo.pNext = &debugCreateInfo;
        instanceCreateInfo.pApplicationInfo = &appInfo;
        instanceCreateInfo.enabledExtensionCount = glfwExtensionCount + 1;
        instanceCreateInfo.ppEnabledExtensionNames = extensions.data();
        instanceCreateInfo.enabledLayerCount = 1;
        instanceCreateInfo.ppEnabledLayerNames = &validationLayerName;
    }
    VkInstance instance;
    VK_CHECK(vkCreateInstance(&instanceCreateInfo, nullptr, &instance));

    // Create debug messenger
    VkDebugUtilsMessengerEXT debugMessenger;
    auto fpVkCreateDebugUtilsMessengerExt = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (fpVkCreateDebugUtilsMessengerExt == nullptr)
    {
        exit(1);
    }
    VK_CHECK(fpVkCreateDebugUtilsMessengerExt(instance, &debugCreateInfo, nullptr, &debugMessenger));

    // Init glfw window.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    const uint32_t WIDTH = 1280;
    const uint32_t HEIGHT = 640;
    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    // Create vulkan surface from the glfw window.
    VK_CHECK(glfwCreateWindowSurface(instance, window, nullptr, &surface));

    // Enumerate the physicalDevices, select the first one and display the name of it.
    uint32_t phyDeviceCount;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &phyDeviceCount, nullptr));
    assert(phyDeviceCount >= 1);
    std::vector<VkPhysicalDevice> phyDeviceVec(phyDeviceCount);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &phyDeviceCount, phyDeviceVec.data()));
    physicalDevice = phyDeviceVec[0];
    VkPhysicalDeviceProperties physicalDevProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &physicalDevProperties);
    std::cout << "Device name:" << physicalDevProperties.deviceName << std::endl;

    // Initialize the logical device with the queue family that supports both graphics and present on the physical device
    // Find the queue family indices that supports graphics and present.
    uint32_t queueFamilyPropCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropCount, nullptr);
    assert(queueFamilyPropCount >= 1);
    std::vector<VkQueueFamilyProperties> queueFamilyProps(queueFamilyPropCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropCount, queueFamilyProps.data());

    bool foundGraphics = false;
    bool foundPresent = false;
    for (unsigned int i = 0; i < queueFamilyPropCount; ++i)
    {
        if (queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            graphicsQueueFamilyIdx = i;
            foundGraphics = true;
        }
        VkBool32 supportPresentSurface;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &supportPresentSurface);
        if (supportPresentSurface)
        {
            presentQueueFamilyIdx = i;
            foundPresent = true;
        }

        if (foundGraphics && foundPresent)
        {
            break;
        }
    }
    assert(foundGraphics && foundPresent);

    // Use the queue family index to initialize the queue create info.
    float queue_priorities[1] = { 0.0 };

    // Queue family index should be unique in vk1.2:
    // https://vulkan.lunarg.com/doc/view/1.2.198.0/windows/1.2-extensions/vkspec.html#VUID-VkDeviceCreateInfo-queueFamilyIndex-02802
    std::set<uint32_t> uniqueQueueFamilies = { graphicsQueueFamilyIdx, presentQueueFamilyIdx };
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    // We need the swap chain device extension
    const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME };

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_feature{};
    {
        dynamic_rendering_feature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
        dynamic_rendering_feature.dynamicRendering = VK_TRUE;
    }

    // Assembly the info into the device create info
    VkDeviceCreateInfo deviceInfo{};
    {
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.pNext = &dynamic_rendering_feature;
        deviceInfo.queueCreateInfoCount = uint32_t(queueCreateInfos.size());
        deviceInfo.pQueueCreateInfos = queueCreateInfos.data();
        deviceInfo.enabledExtensionCount = uint32_t(deviceExtensions.size());
        deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
    }

    // Create the logical device
    VK_CHECK(vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device));

    // Create the VMA
    VmaVulkanFunctions vkFuncs = {};
    {
        vkFuncs.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
        vkFuncs.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;
    }

    VmaAllocatorCreateInfo allocCreateInfo = {};
    {
        allocCreateInfo.vulkanApiVersion = VK_API_VERSION_1_2;
        allocCreateInfo.physicalDevice = physicalDevice;
        allocCreateInfo.device = device;
        allocCreateInfo.instance = instance;
        allocCreateInfo.pVulkanFunctions = &vkFuncs;
    }
    vmaCreateAllocator(&allocCreateInfo, &allocator);

    // Get the graphics queue and the present queue
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    vkGetDeviceQueue(device, graphicsQueueFamilyIdx, 0, &graphicsQueue);
    vkGetDeviceQueue(device, presentQueueFamilyIdx, 0, &presentQueue);

    // Create the descriptor pool
    VkDescriptorPoolSize poolSizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo pool_info{};
    {
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000 * sizeof(poolSizes) / sizeof(VkDescriptorPoolSize);
        pool_info.poolSizeCount = (uint32_t)(sizeof(poolSizes) / sizeof(VkDescriptorPoolSize));
        pool_info.pPoolSizes = poolSizes;
    }
    VK_CHECK(vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptorPool));

    CreateSwapchain();
    CreateSwapchainImageViews();

    // We will use dynamic rendering for the scene rendering
    
    // Create the graphics pipeline
    // Create Shader Modules.
    VkShaderModule vsShaderModule = createShaderModule("./skybox_vert.spv", device);
    VkShaderModule psShaderModule = createShaderModule("./skybox_frag.spv", device);
    VkPipelineShaderStageCreateInfo shaderStgInfo[2];
    {
        shaderStgInfo[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStgInfo[0].pNext = nullptr;
        shaderStgInfo[0].flags = 0;
        shaderStgInfo[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shaderStgInfo[0].module = vsShaderModule;
        shaderStgInfo[0].pName = "main";
        shaderStgInfo[0].pSpecializationInfo = nullptr;

        shaderStgInfo[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStgInfo[1].pNext = nullptr;
        shaderStgInfo[1].flags = 0;
        shaderStgInfo[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaderStgInfo[1].module = psShaderModule;
        shaderStgInfo[1].pName = "main";
        shaderStgInfo[1].pSpecializationInfo = nullptr;
    }

    // Create vertex input info (Our vertices are in the vert shader)
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;

    // Create the input assembly info.
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Create the viewport and scissor info.
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // Create the rasterization info.
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Create the multisample info.
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Create the color blend info.
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    {
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
    }
    
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    {
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;
    }

    // Create the dynamic state info for scissor and viewport
    std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // Load the HDRI image into RAM
    std::string hdriFilePath = SOURCE_PATH;
    hdriFilePath += "/../data/output_skybox.hdr";
    HDRLoaderResult hdrLdRes;
    bool ret = HDRLoader::load(hdriFilePath.c_str(), hdrLdRes);

    // Create GPU resources for the camera ubo
    CreateCameraUboObjects();

    // Create pipeline binding and descriptor objects for the camera parameters
    VkDescriptorSetLayoutBinding cameraUboBinding{};
    cameraUboBinding.binding = 1;
    cameraUboBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    cameraUboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    cameraUboBinding.descriptorCount = 1;

    // Information to link the descriptor to the camera gpu buffer.
    

    // Create GPU resources for the HDRI image
    CreateHdrRenderObjects(hdrLdRes);

    // Create pipeline binding objects for the HDRI image
    VkDescriptorSetLayoutBinding hdriSamplerBinding{};
    hdriSamplerBinding.binding = 0;
    hdriSamplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    hdriSamplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    hdriSamplerBinding.descriptorCount = 1;

    // Create pipeline's descriptors layout
    VkDescriptorSetLayoutBinding skyboxPipelineDesSet0LayoutBindings[2] = {hdriSamplerBinding, cameraUboBinding};
    VkDescriptorSetLayoutCreateInfo skyboxPipelineDesSet0LayoutInfo{};
    skyboxPipelineDesSet0LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    skyboxPipelineDesSet0LayoutInfo.bindingCount = 2;
    skyboxPipelineDesSet0LayoutInfo.pBindings = skyboxPipelineDesSet0LayoutBindings;

    VkDescriptorSetLayout skyboxPipelineDesSet0Layout{};
    VK_CHECK(vkCreateDescriptorSetLayout(device, &skyboxPipelineDesSet0LayoutInfo, nullptr, &skyboxPipelineDesSet0Layout));

    // Create pipeline descirptor
    VkDescriptorSetAllocateInfo skyboxPipelineDesSet0AllocInfo{};
    skyboxPipelineDesSet0AllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    skyboxPipelineDesSet0AllocInfo.descriptorPool = descriptorPool;
    skyboxPipelineDesSet0AllocInfo.pSetLayouts = &skyboxPipelineDesSet0Layout;
    skyboxPipelineDesSet0AllocInfo.descriptorSetCount = 1;

    skyboxPipelineDescriptorSet0s.resize(MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VK_CHECK(vkAllocateDescriptorSets(device, &skyboxPipelineDesSet0AllocInfo, &skyboxPipelineDescriptorSet0s[i]));
    }

    // Link descriptors to the buffer and image
    VkDescriptorImageInfo hdriDesImgInfo{};
    {
        hdriDesImgInfo.imageView = hdrCubeMapView;
        hdriDesImgInfo.sampler = hdrSampler;
        hdriDesImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkDescriptorBufferInfo desCameraParaBufInfo{};
        {
            desCameraParaBufInfo.buffer = cameraParaBuffers[i];
            desCameraParaBufInfo.offset = 0;
            desCameraParaBufInfo.range = sizeof(float) * 16;
        }

        VkWriteDescriptorSet writeCameraBufDesSet{};
        {
            writeCameraBufDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeCameraBufDesSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writeCameraBufDesSet.dstSet = skyboxPipelineDescriptorSet0s[i];
            writeCameraBufDesSet.dstBinding = 1;
            writeCameraBufDesSet.descriptorCount = 1;
            writeCameraBufDesSet.pBufferInfo = &desCameraParaBufInfo;
        }

        VkWriteDescriptorSet writeHdrDesSet{};
        {
            writeHdrDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeHdrDesSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writeHdrDesSet.dstSet = skyboxPipelineDescriptorSet0s[i];
            writeHdrDesSet.dstBinding = 0;
            writeHdrDesSet.pImageInfo = &hdriDesImgInfo;
            writeHdrDesSet.descriptorCount = 1;
        }

        // Linking skybox pipeline descriptors: skybox cubemap and camera buffer descriptors to their GPU memory and info.
        VkWriteDescriptorSet writeSkyboxPipelineDescriptors[2] = { writeHdrDesSet, writeCameraBufDesSet };
        vkUpdateDescriptorSets(device, 2, writeSkyboxPipelineDescriptors, 0, NULL);
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    {
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &skyboxPipelineDesSet0Layout;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
    }
    VkPipelineLayout pipelineLayout;
    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout));

    // Create the graphics pipeline -- The graphics pipeline is used for scene rendering
    VkPipelineRenderingCreateInfoKHR pipelineRenderCreateInfo{};
    {
        pipelineRenderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        pipelineRenderCreateInfo.colorAttachmentCount = 1;
        pipelineRenderCreateInfo.pColorAttachmentFormats = &choisenSurfaceFormat.format;
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    {
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.pNext = &pipelineRenderCreateInfo;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStgInfo;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = nullptr;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    }
    VkPipeline graphicsPipeline;
    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline));

    // Create the command pool belongs to the graphics queue
    VkCommandPoolCreateInfo commandPoolInfo{};
    {
        commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        commandPoolInfo.queueFamilyIndex = graphicsQueueFamilyIdx;
    }
    VkCommandPool commandPool;
    VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool));

    // Create the command buffers
    std::vector<VkCommandBuffer> commandBuffers(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo commandBufferAllocInfo{};
    {
        commandBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAllocInfo.commandPool = commandPool;
        commandBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferAllocInfo.commandBufferCount = (uint32_t)commandBuffers.size();
    }
    VK_CHECK(vkAllocateCommandBuffers(device, &commandBufferAllocInfo, commandBuffers.data()));

    // Create Sync objects
    std::vector<VkSemaphore> imageAvailableSemaphores(MAX_FRAMES_IN_FLIGHT);
    std::vector<VkSemaphore> renderFinishedSemaphores(MAX_FRAMES_IN_FLIGHT);
    std::vector<VkFence> inFlightFences(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    {
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    }

    VkFenceCreateInfo fenceInfo{};
    {
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]));
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]));
        VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]))
    }

    VkImageSubresourceRange swapchainPresentSubResRange{};
    {
        swapchainPresentSubResRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        swapchainPresentSubResRange.baseMipLevel = 0;
        swapchainPresentSubResRange.levelCount = 1;
        swapchainPresentSubResRange.baseArrayLayer = 0;
        swapchainPresentSubResRange.layerCount = 1;
    }

    // Send the HDR cubemap image to GPU:
    // - Copy RAM to GPU staging buffer;
    // - Copy buffer to image;
    // - Copy Camera parameters to the GPU buffer;
    {
        // Create the staging buffer
        VkBuffer stagingBuffer;
        VmaAllocation stagingBufAlloc;

        VmaAllocationCreateInfo stagingBufAllocInfo{};
        {
            stagingBufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            stagingBufAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        }

        VkBufferCreateInfo stgBufInfo{};
        {
            stgBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            stgBufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            stgBufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            stgBufInfo.size = 3 * sizeof(float) * hdrLdRes.width * hdrLdRes.height;
        }

        VK_CHECK(vmaCreateBuffer(allocator, &stgBufInfo, &stagingBufAllocInfo, &stagingBuffer, &stagingBufAlloc, nullptr));

        // Copy the RAM data to the staging buffer
        void* pStgBufMem;
        VK_CHECK(vmaMapMemory(allocator, stagingBufAlloc, &pStgBufMem));
        memcpy(pStgBufMem, hdrLdRes.cols, 3 * sizeof(float) * hdrLdRes.width * hdrLdRes.height);
        vmaUnmapMemory(allocator, stagingBufAlloc);

        /* Send staging buffer data to the GPU image. */ 
        VkCommandBufferBeginInfo beginInfo{};
        {
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        }
        VK_CHECK(vkBeginCommandBuffer(commandBuffers[0], &beginInfo));

        // Cubemap's 6 layers SubresourceRange
        VkImageSubresourceRange cubemapSubResRange{};
        {
            cubemapSubResRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            cubemapSubResRange.baseMipLevel = 0;
            cubemapSubResRange.levelCount = 1;
            cubemapSubResRange.baseArrayLayer = 0;
            cubemapSubResRange.layerCount = 6;
        }

        // Transform the layout of the image to copy destination
        VkImageMemoryBarrier hdrUndefToDstBarrier{};
        {
            hdrUndefToDstBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            hdrUndefToDstBarrier.image = hdrCubeMapImage;
            hdrUndefToDstBarrier.subresourceRange = cubemapSubResRange;
            hdrUndefToDstBarrier.srcAccessMask = 0;
            hdrUndefToDstBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            hdrUndefToDstBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            hdrUndefToDstBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        }
        
        vkCmdPipelineBarrier(
            commandBuffers[0], 
            VK_PIPELINE_STAGE_HOST_BIT, 
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &hdrUndefToDstBarrier);

        // Copy the data from buffer to the image
        // - The buffer data of the image cannot be interleaved (The data of a separate image should be continues in the buffer address space.)
        // - However, our cubemap data (hStrip) is interleaved. 
        // - So, we have multiple choices to put them into the cubemap image. Here, I choose to offset the buffer starting point, specify the
        // -     long row length and copy that for 6 times.
        // - We are using the hStrip skybox here. In the `cmftStudio`, we can also choose the vStrip here, which is more convenient, but we just
        // -     use the hStrip here since it's more educational.
        VkBufferImageCopy hdrBufToImgCopies[6];
        memset(hdrBufToImgCopies, 0, sizeof(hdrBufToImgCopies));
        for (uint32_t i = 0; i < 6; i++)
        {
            VkExtent3D extent{};
            {
                extent.width = hdrLdRes.width / 6;
                extent.height = hdrLdRes.height;
                extent.depth = 1;
            }

            hdrBufToImgCopies[i].bufferRowLength = hdrLdRes.width;
            hdrBufToImgCopies[i].bufferImageHeight = hdrLdRes.height;
            hdrBufToImgCopies[i].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            hdrBufToImgCopies[i].imageSubresource.mipLevel = 0;
            hdrBufToImgCopies[i].imageSubresource.baseArrayLayer = i;
            hdrBufToImgCopies[i].imageSubresource.layerCount = 1;

            hdrBufToImgCopies[i].imageExtent = extent;
            // In the unit of bytes:
            hdrBufToImgCopies[i].bufferOffset = i * (hdrLdRes.width / 6) * sizeof(float) * 3;
        }

        vkCmdCopyBufferToImage(
            commandBuffers[0],
            stagingBuffer,
            hdrCubeMapImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            6, hdrBufToImgCopies);
        
        // Transform the layout of the image to shader access resource
        VkImageMemoryBarrier hdrDstToShaderBarrier{};
        {
            hdrDstToShaderBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            hdrDstToShaderBarrier.image = hdrCubeMapImage;
            hdrDstToShaderBarrier.subresourceRange = cubemapSubResRange;
            hdrDstToShaderBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            hdrDstToShaderBarrier.dstAccessMask = VK_ACCESS_NONE;
            hdrDstToShaderBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            hdrDstToShaderBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        vkCmdPipelineBarrier(
            commandBuffers[0],
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &hdrDstToShaderBarrier);

        // End the command buffer and submit the packets
        vkEndCommandBuffer(commandBuffers[0]);

        // Submit the filled command buffer to the graphics queue to draw the image
        VkSubmitInfo submitInfo{};
        {
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffers[0];
        }
        vkResetFences(device, 1, &inFlightFences[0]);
        VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[0]));

        // Wait for the end of all transformation and reset the command buffer. The fence would be waited in the first loop.
        vkWaitForFences(device, 1, &inFlightFences[0], VK_TRUE, UINT64_MAX);
        vkResetCommandBuffer(commandBuffers[0], 0);

        // Destroy temp resources
        vmaDestroyBuffer(allocator, stagingBuffer, stagingBufAlloc);

        // Copy camera data to ubo buffer
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            void* pUboData;
            vmaMapMemory(allocator, cameraParaBufferAllocs[i], &pUboData);

            float cameraData[16] = {};
            camera.GetView(cameraData);
            camera.GetRight(&cameraData[4]);
            camera.GetUp(&cameraData[8]);
            camera.GetNearPlane(cameraData[12], cameraData[13], cameraData[14]);

            memcpy(pUboData, cameraData, sizeof(cameraData));
            vmaUnmapMemory(allocator, cameraParaBufferAllocs[i]);
        }
    }

    // Main Loop
    // Two draws. First draw draws triangle into an image with window 1 window size.
    // Second draw draws GUI. GUI would use the image drawn from the first draw.
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Get IO information and create events
        SharedLib::HEventArguments args;
        args[crc32("IS_DOWN")] = isDown;

        if (isDown)
        {
            SharedLib::HFVec2 pos;
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            pos.ele[0] = xpos;
            pos.ele[1] = ypos;
            args[crc32("POS")] = pos;
        }

        SharedLib::HEvent mEvent(args, "MOUSE_MIDDLE_BUTTON");
        camera.OnEvent(mEvent);

        // Draw Frame
        // Wait for the resources from the possible on flight frame
        vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        // Get next available image from the swapchain
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            // The surface is imcompatiable with the swapchain (resize window).
            RecreateSwapchain();
            continue;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            // Not success or usable.
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        // Reset unused previous frame's resource
        vkResetFences(device, 1, &inFlightFences[currentFrame]);
        vkResetCommandBuffer(commandBuffers[currentFrame], 0);

        // Fill the command buffer
        VkCommandBufferBeginInfo beginInfo{};
        {
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        }
        VK_CHECK(vkBeginCommandBuffer(commandBuffers[currentFrame], &beginInfo));

        // Update the camera according to mouse input and sent camera data to the UBO
        void* pUboData;
        vmaMapMemory(allocator, cameraParaBufferAllocs[currentFrame], &pUboData);

        float cameraData[16] = {};
        camera.GetView(cameraData);
        camera.GetRight(&cameraData[4]);
        camera.GetUp(&cameraData[8]);
        camera.GetNearPlane(cameraData[12], cameraData[13], cameraData[14]);

        memcpy(pUboData, cameraData, sizeof(cameraData));
        vmaUnmapMemory(allocator, cameraParaBufferAllocs[currentFrame]);

        // Transform the layout of the swapchain from undefined to render target.
        VkImageMemoryBarrier swapchainRenderTargetTransBarrier{};
        {
            swapchainRenderTargetTransBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            swapchainRenderTargetTransBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            swapchainRenderTargetTransBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            swapchainRenderTargetTransBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            swapchainRenderTargetTransBarrier.image = swapchainImages[imageIndex];
            swapchainRenderTargetTransBarrier.subresourceRange = swapchainPresentSubResRange;
        }

        vkCmdPipelineBarrier(commandBuffers[currentFrame],
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &swapchainRenderTargetTransBarrier);

        // Draw the scene
        VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };

        VkRenderingAttachmentInfoKHR renderAttachmentInfo{};
        {
            renderAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
            renderAttachmentInfo.imageView = swapchainImageViews[imageIndex];
            renderAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
            renderAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            renderAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            renderAttachmentInfo.clearValue = clearColor;
        }

        VkRenderingInfoKHR renderInfo{};
        {
            renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
            renderInfo.renderArea.offset = { 0, 0 };
            renderInfo.renderArea.extent = swapchainImageExtent;
            renderInfo.layerCount = 1;
            renderInfo.colorAttachmentCount = 1;
            renderInfo.pColorAttachments = &renderAttachmentInfo;
        }

        vkCmdBeginRendering(commandBuffers[currentFrame], &renderInfo);

        // Bind the skybox pipeline descriptor sets
        vkCmdBindDescriptorSets(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &skyboxPipelineDescriptorSet0s[currentFrame], 0, NULL);

        // Bind the graphics pipeline
        vkCmdBindPipeline(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

        // Set the viewport
        VkViewport viewport{};
        {
            viewport.x = 0.f;
            viewport.y = 0.f;
            viewport.width  = (float)swapchainImageExtent.width;
            viewport.height = (float)swapchainImageExtent.height;
            viewport.minDepth = 0.f;
            viewport.maxDepth = 1.f;
        }
        vkCmdSetViewport(commandBuffers[currentFrame], 0, 1, &viewport);

        // Set the scissor
        VkRect2D scissor{};
        {
            scissor.offset = { 0, 0 };
            scissor.extent = swapchainImageExtent;
            vkCmdSetScissor(commandBuffers[currentFrame], 0, 1, &scissor);
        }

        vkCmdDraw(commandBuffers[currentFrame], 6, 1, 0, 0);

        vkCmdEndRendering(commandBuffers[currentFrame]);

        // Transform the swapchain image layout from render target to present.
        // Transform the layout of the swapchain from undefined to render target.
        VkImageMemoryBarrier swapchainPresentTransBarrier{};
        {
            swapchainPresentTransBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            swapchainPresentTransBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            swapchainPresentTransBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            swapchainPresentTransBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            swapchainPresentTransBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            swapchainPresentTransBarrier.image = swapchainImages[imageIndex];
            swapchainPresentTransBarrier.subresourceRange = swapchainPresentSubResRange;
        }

        vkCmdPipelineBarrier(commandBuffers[currentFrame],
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &swapchainPresentTransBarrier);

        VK_CHECK(vkEndCommandBuffer(commandBuffers[currentFrame]));

        // Submit the filled command buffer to the graphics queue to draw the image
        VkSubmitInfo submitInfo{};
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        {
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            // This draw would wait at dstStage and wait for the waitSemaphores
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &imageAvailableSemaphores[currentFrame];
            submitInfo.pWaitDstStageMask = waitStages;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffers[currentFrame];
            // This draw would let the signalSemaphore sign when it finishes
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &renderFinishedSemaphores[currentFrame];
        }
        VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]));

        // Put the swapchain into the present info and wait for the graphics queue previously before presenting.
        VkPresentInfoKHR presentInfo{};
        {
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = &renderFinishedSemaphores[currentFrame];
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = &swapchain;
            presentInfo.pImageIndices = &imageIndex;
        }
        result = vkQueuePresentKHR(presentQueue, &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized)
        {
            framebufferResized = false;
            RecreateSwapchain();
        }
        else if (result != VK_SUCCESS)
        {
            throw std::runtime_error("failed to present swap chain image!");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    vkDeviceWaitIdle(device);

    // Cleanup
    // Cleanup Swapchain
    CleanupSwapchain();

    // Cleanup syn objects
    for (auto itr : imageAvailableSemaphores)
    {
        vkDestroySemaphore(device, itr, nullptr);
    }

    for (auto itr : renderFinishedSemaphores)
    {
        vkDestroySemaphore(device, itr, nullptr);
    }

    for (auto itr : inFlightFences)
    {
        vkDestroyFence(device, itr, nullptr);
    }

    DestroyHdrRenderObjs();
    DestroyCameraUboObjects();

    // Destroy the command pool
    vkDestroyCommandPool(device, commandPool, nullptr);

    // Destroy shader modules
    vkDestroyShaderModule(device, vsShaderModule, nullptr);
    vkDestroyShaderModule(device, psShaderModule, nullptr);

    // Destroy the pipeline
    vkDestroyPipeline(device, graphicsPipeline, nullptr);

    // Destroy the pipeline layout
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

    // Destroy the descriptor set layout
    vkDestroyDescriptorSetLayout(device, skyboxPipelineDesSet0Layout, nullptr);

    // Destroy the descriptor pool
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);

    // Destroy the allocator
    vmaDestroyAllocator(allocator);

    // Destroy the device
    vkDestroyDevice(device, nullptr);

    // Destroy vulkan surface
    vkDestroySurfaceKHR(instance, surface, nullptr);

    // Destroy debug messenger
    auto fpVkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (fpVkDestroyDebugUtilsMessengerEXT == nullptr)
    {
        exit(1);
    }
    fpVkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);

    // Destroy instance
    vkDestroyInstance(instance, nullptr);

    glfwDestroyWindow(window);

    glfwTerminate();
}
