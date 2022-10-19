#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <vulkan/vulkan.h>
#include <glfw3.h>
#include "lodepng.h"

#include <vector>
#include <set>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <fstream>

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
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data)
{
    if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        std::cout << "Callback Warning: " << callback_data->messageIdNumber << ":" << callback_data->pMessageIdName << ":" << callback_data->pMessage << std::endl;
    }
    else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        std::cerr << "Callback Error: " << callback_data->messageIdNumber << ":" << callback_data->pMessageIdName << ":" << callback_data->pMessage << std::endl;
    }
    return VK_FALSE;
}

#define VK_CHECK(res) if(res){std::cout << "Error at line:" << __LINE__ << ", Error name:" << to_string(res) << ".\n"; exit(1);}

bool framebufferResized = false;

static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    framebufferResized = true;
}

static void CheckVkResult(VkResult err)
{
    VK_CHECK(err);
}

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

const char myLayout[] = 
"[Window][DockSpaceViewport_11111111]\n\
Pos=0,0\n\
Size=1280,640\n\
Collapsed=0\n\
\n\
[Window][Debug##Default]\n\
Pos=60,60\n\
Size=400,400\n\
Collapsed=0\n\
\n\
[Window][Window 1]\n\
Pos=0,0\n\
Size=637,640\n\
Collapsed=0\n\
DockId=0x00000001,0\n\
\n\
[Window][Window 2]\n\
Pos=639,0\n\
Size=641,640\n\
Collapsed=0\n\
DockId=0x00000002,0\n\
\n\
DockSpace   ID=0x8B93E3BD Window=0xA787BDB4 Pos=0,0 Size=1280,640 Split=X Selected=0x82C5531C\n\
  DockNode  ID = 0x00000001 Parent = 0x8B93E3BD SizeRef = 637, 640 Selected = 0xC56529CC\n\
  DockNode  ID = 0x00000002 Parent = 0x8B93E3BD SizeRef = 641, 640 CentralNode = 1 Selected = 0x82C5531C";

// NOTE: Each render pass' attachments should have same extent for render area when we start the render pass.
// In our case, the first scene rendering would be smaller than the overall gui rendering. So, we will use two render
// pass.
VkRenderPass sceneRenderPass;
VkRenderPass guiRenderPass;
std::vector<VkFramebuffer> swapchainFramebuffers;
std::vector<VkImageView> swapchainImageViews;
std::vector<VkFramebuffer> sceneRenderFramebuffers;
std::vector<VkImage> sceneRenderImages;
std::vector<VkExtent2D> sceneRenderImagesExtents;
std::vector<VmaAllocation> sceneRenderImgsAllocs;
std::vector<VkImageView> sceneRenderImageViews;
std::vector<VkSampler> guiImgRenderSamplers;
std::vector<VkDescriptorSet> guiImgDescriptors;
VkDescriptorPool descriptorPool;
VmaAllocator allocator;

// Create the swapchain frame buffer
void CreateSwapchainFramebuffer()
{
    // Create Framebuffer
    swapchainFramebuffers.resize(swapchainImageViews.size());
    for (int i = 0; i < swapchainImageViews.size(); i++)
    {
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = guiRenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &swapchainImageViews[i];
        framebufferInfo.width = swapchainImageExtent.width;
        framebufferInfo.height = swapchainImageExtent.height;
        framebufferInfo.layers = 1;
        VK_CHECK(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapchainFramebuffers[i]));
    }
}

// Create the image views
void CreateSwapchainImageViews()
{
    // Create image views for the swapchain images
    std::vector<VkImage> swapchainImages;
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
    // Clean the frame buffers
    for (auto framebuffer : swapchainFramebuffers)
    {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

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
    CreateSwapchainFramebuffer();
}

void RecreateSceneRenderObjs(uint32_t width, uint32_t height)
{
    vmaDestroyImage(allocator, sceneRenderImages[currentFrame], sceneRenderImgsAllocs[currentFrame]);

    VmaAllocationCreateInfo sceneImgsAllocInfo{};
    {
        sceneImgsAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        sceneImgsAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    VkExtent3D extent{};
    {
        extent.width = width;
        extent.height = height;
        extent.depth = 1;
    }
    
    VkImageCreateInfo sceneImgsInfo{};
    {
        sceneImgsInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        sceneImgsInfo.imageType = VK_IMAGE_TYPE_2D;
        sceneImgsInfo.format = choisenSurfaceFormat.format;
        sceneImgsInfo.extent = extent;
        sceneImgsInfo.mipLevels = 1;
        sceneImgsInfo.arrayLayers = 1;
        sceneImgsInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        sceneImgsInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        sceneImgsInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        sceneImgsInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    vmaCreateImage(allocator,
                   &sceneImgsInfo,
                   &sceneImgsAllocInfo,
                   &sceneRenderImages[currentFrame],
                   &sceneRenderImgsAllocs[currentFrame],
                   nullptr);

    sceneRenderImagesExtents[currentFrame] = VkExtent2D{ width, height };

    vkDestroyImageView(device, sceneRenderImageViews[currentFrame], nullptr);

    // Create the Image View
    VkImageViewCreateInfo info = {};
    {
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image = sceneRenderImages[currentFrame];
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = choisenSurfaceFormat.format;
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.layerCount = 1;
    }
    VK_CHECK(vkCreateImageView(device, &info, nullptr, &sceneRenderImageViews[currentFrame]));

    vkDestroyFramebuffer(device, sceneRenderFramebuffers[currentFrame], nullptr);

    VkFramebufferCreateInfo sceneRenderFramebufferInfo{};
    {
        sceneRenderFramebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        sceneRenderFramebufferInfo.renderPass = sceneRenderPass;
        sceneRenderFramebufferInfo.attachmentCount = 1;
        sceneRenderFramebufferInfo.pAttachments = &sceneRenderImageViews[currentFrame];
        sceneRenderFramebufferInfo.width = extent.width;
        sceneRenderFramebufferInfo.height = extent.height;
        sceneRenderFramebufferInfo.layers = 1;
    }
    VK_CHECK(vkCreateFramebuffer(device, &sceneRenderFramebufferInfo, nullptr, &sceneRenderFramebuffers[currentFrame]));
}

void InitSceneRenderObjs()
{
    VmaAllocationCreateInfo sceneImgsAllocInfo{};
    {
        sceneImgsAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        sceneImgsAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    VkExtent3D extent{};
    {
        extent.width = 100;
        extent.height = 100;
        extent.depth = 1;
    }

    VkImageCreateInfo sceneImgsInfo{};
    {
        sceneImgsInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        sceneImgsInfo.imageType = VK_IMAGE_TYPE_2D;
        sceneImgsInfo.format = choisenSurfaceFormat.format;
        sceneImgsInfo.extent = extent;
        sceneImgsInfo.mipLevels = 1;
        sceneImgsInfo.arrayLayers = 1;
        sceneImgsInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        sceneImgsInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        sceneImgsInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        sceneImgsInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vmaCreateImage(allocator,
            &sceneImgsInfo,
            &sceneImgsAllocInfo,
            &sceneRenderImages[i],
            &sceneRenderImgsAllocs[i],
            nullptr);

        sceneRenderImagesExtents[i] = VkExtent2D{ extent.width, extent.height };

        // Create the Image View
        VkImageViewCreateInfo info = {};
        {
            info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            info.image = sceneRenderImages[i];
            info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            info.format = choisenSurfaceFormat.format;
            info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            info.subresourceRange.levelCount = 1;
            info.subresourceRange.layerCount = 1;
        }

        VK_CHECK(vkCreateImageView(device, &info, nullptr, &sceneRenderImageViews[i]));

        VkFramebufferCreateInfo sceneRenderFramebufferInfo{};
        {
            sceneRenderFramebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            sceneRenderFramebufferInfo.renderPass = sceneRenderPass;
            sceneRenderFramebufferInfo.attachmentCount = 1;
            sceneRenderFramebufferInfo.pAttachments = &sceneRenderImageViews[i];
            sceneRenderFramebufferInfo.width = extent.width;
            sceneRenderFramebufferInfo.height = extent.height;
            sceneRenderFramebufferInfo.layers = 1;
        }
        VK_CHECK(vkCreateFramebuffer(device, &sceneRenderFramebufferInfo, nullptr, &sceneRenderFramebuffers[i]));

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
        VK_CHECK(vkCreateSampler(device, &sampler_info, nullptr, &guiImgRenderSamplers[i]));
    }
}

void CleanupSceneRenderObjs()
{
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vmaDestroyImage(allocator, sceneRenderImages[i], sceneRenderImgsAllocs[i]);
        vkDestroyImageView(device, sceneRenderImageViews[i], nullptr);
        vkDestroyFramebuffer(device, sceneRenderFramebuffers[i], nullptr);
    }
}

void AddTextureToImGUI(VkDescriptorSet* img_ds, int image_width, int image_height)
{
    // Create the Sampler
    vkDestroySampler(device, guiImgRenderSamplers[currentFrame], nullptr);
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
    VK_CHECK(vkCreateSampler(device, &sampler_info, nullptr, &guiImgRenderSamplers[currentFrame]));

    if (guiImgDescriptors.size() == MAX_FRAMES_IN_FLIGHT)
    {
        vkFreeDescriptorSets(device, descriptorPool, 1, &guiImgDescriptors[currentFrame]);
    }

    // Create Descriptor Set using ImGUI's implementation
    *img_ds = ImGui_ImplVulkan_AddTexture(guiImgRenderSamplers[currentFrame], sceneRenderImageViews[currentFrame], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    if (guiImgDescriptors.size() < MAX_FRAMES_IN_FLIGHT)
    {
        guiImgDescriptors.push_back(*img_ds);
    }
    else
    {
        guiImgDescriptors[currentFrame] = *img_ds;
    }
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
        appInfo.pApplicationName = "DumpTri";
        appInfo.applicationVersion = 1;
        appInfo.pEngineName = "VulkanDict";
        appInfo.engineVersion = 1;
        appInfo.apiVersion = VK_API_VERSION_1_2;
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
    const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    // Assembly the info into the device create info
    VkDeviceCreateInfo deviceInfo{};
    {
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.queueCreateInfoCount = queueCreateInfos.size();
        deviceInfo.pQueueCreateInfos = queueCreateInfos.data();
        deviceInfo.enabledExtensionCount = 1;
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

    // Init scene render info
    sceneRenderImages.resize(MAX_FRAMES_IN_FLIGHT);
    sceneRenderImgsAllocs.resize(MAX_FRAMES_IN_FLIGHT);
    sceneRenderImageViews.resize(MAX_FRAMES_IN_FLIGHT);
    sceneRenderFramebuffers.resize(MAX_FRAMES_IN_FLIGHT);
    sceneRenderImagesExtents.resize(MAX_FRAMES_IN_FLIGHT);
    guiImgRenderSamplers.resize(MAX_FRAMES_IN_FLIGHT);
    for (auto itr : sceneRenderImagesExtents)
    {
        itr.width = 0;
        itr.height = 0;
    }

    // Create the render pass
    // Specify the scene render attachment.
    VkAttachmentDescription sceneRenderAttachment{};
    {
        sceneRenderAttachment.format = choisenSurfaceFormat.format;
        sceneRenderAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        sceneRenderAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        sceneRenderAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        sceneRenderAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        sceneRenderAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        sceneRenderAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        sceneRenderAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    // Specify the GUI attachment: We will need to present everything in GUI. So, the finalLayout would be presentable.
    VkAttachmentDescription guiRenderTargetAttachment{};
    {
        guiRenderTargetAttachment.format = choisenSurfaceFormat.format;
        guiRenderTargetAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        guiRenderTargetAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        guiRenderTargetAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        guiRenderTargetAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        guiRenderTargetAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        guiRenderTargetAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        guiRenderTargetAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }

    // Specify the color reference, which specifies the attachment layout during the subpass
    VkAttachmentReference sceneRenderAttachmentRef{};
    {
        sceneRenderAttachmentRef.attachment = 0;
        sceneRenderAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    VkAttachmentReference guiAttachmentRef{};
    {
        guiAttachmentRef.attachment = 0;
        guiAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    // Specify the subpass executed for the scene
    VkSubpassDescription sceneSubpass{};
    {
        sceneSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        sceneSubpass.colorAttachmentCount = 1;
        sceneSubpass.pColorAttachments = &sceneRenderAttachmentRef;
    }

    // Specify the dependency between the scene subpass and operations before it.
    // Here, the subpass 0 depends on the operations set before the render pass.
    // The VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT operations in the subpass 0 executes after 
    // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT operations before the render pass finishes.
    // The VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT operations in the VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    // would happen after 0 operations finishes in the VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT.
    // (So, I believe the memory access dependency is not necessary here.)
    VkSubpassDependency sceneSubpassesDependency{};
    {
        sceneSubpassesDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        sceneSubpassesDependency.dstSubpass = 0;
        sceneSubpassesDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        sceneSubpassesDependency.srcAccessMask = 0;
        sceneSubpassesDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        sceneSubpassesDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }

    // Specity the subpass executed for the GUI
    VkSubpassDescription guiSubpass{};
    {
        guiSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        guiSubpass.colorAttachmentCount = 1;
        guiSubpass.pColorAttachments = &guiAttachmentRef;
    }

    // Specify the dependency between the scene subpass (0) and the gui subpass (1).
    // The gui subpass' rendering output should wait for the scene subpass' rendering output.
    VkSubpassDependency guiSubpassesDependency{};
    {
        guiSubpassesDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        guiSubpassesDependency.dstSubpass = 0;
        guiSubpassesDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        guiSubpassesDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        guiSubpassesDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        guiSubpassesDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }

    // Create the render passes
    VkRenderPassCreateInfo sceneRenderPassInfo{};
    {
        sceneRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        sceneRenderPassInfo.attachmentCount = 1;
        sceneRenderPassInfo.pAttachments = &sceneRenderAttachment;
        sceneRenderPassInfo.subpassCount = 1;
        sceneRenderPassInfo.pSubpasses = &sceneSubpass;
        sceneRenderPassInfo.dependencyCount = 1;
        sceneRenderPassInfo.pDependencies = &sceneSubpassesDependency;
    }
    VK_CHECK(vkCreateRenderPass(device, &sceneRenderPassInfo, nullptr, &sceneRenderPass));

    VkRenderPassCreateInfo guiRenderPassInfo{};
    {
        guiRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        guiRenderPassInfo.attachmentCount = 1;
        guiRenderPassInfo.pAttachments = &guiRenderTargetAttachment;
        guiRenderPassInfo.subpassCount = 1;
        guiRenderPassInfo.pSubpasses = &guiSubpass;
        guiRenderPassInfo.dependencyCount = 1;
        guiRenderPassInfo.pDependencies = &guiSubpassesDependency;
    }
    VK_CHECK(vkCreateRenderPass(device, &guiRenderPassInfo, nullptr, &guiRenderPass));

    // Create the graphics pipeline
    // Create Shader Modules.
    VkShaderModule vsShaderModule = createShaderModule("./vert.spv", device);
    VkShaderModule psShaderModule = createShaderModule("./frag.spv", device);
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

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    {
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
    }
    VkPipelineLayout pipelineLayout;
    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout));

    // Create the graphics pipeline -- The graphics pipeline is used for scene rendering
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    {
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
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
        pipelineInfo.renderPass = sceneRenderPass;
        pipelineInfo.subpass = 0; // The first subpass for scene rendering; the second for gui rendering.
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    }
    VkPipeline graphicsPipeline;
    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline));

    // Create dummy swapchain
    InitSceneRenderObjs();
    CreateSwapchainFramebuffer();

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

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(window, true);
    ImGui_ImplVulkan_InitInfo initInfo{};
    {
        initInfo.Instance = instance;
        initInfo.PhysicalDevice = physicalDevice;
        initInfo.Device = device;
        initInfo.QueueFamily = graphicsQueueFamilyIdx;
        initInfo.Queue = graphicsQueue;
        initInfo.DescriptorPool = descriptorPool;
        initInfo.Subpass = 0; // GUI render will use the first subpass.
        initInfo.MinImageCount = MAX_FRAMES_IN_FLIGHT;
        initInfo.ImageCount = MAX_FRAMES_IN_FLIGHT;
        initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        initInfo.CheckVkResultFn = CheckVkResult;
    }
    ImGui_ImplVulkan_Init(&initInfo, guiRenderPass);

    // Upload Fonts
    {
        // Use any command queue
        VK_CHECK(vkResetCommandPool(device, commandPool, 0));

        VkCommandBufferAllocateInfo fontUploadCmdBufAllocInfo{};
        {
            fontUploadCmdBufAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            fontUploadCmdBufAllocInfo.commandPool = commandPool;
            fontUploadCmdBufAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            fontUploadCmdBufAllocInfo.commandBufferCount = 1;
        }
        VkCommandBuffer fontUploadCmdBuf;
        VK_CHECK(vkAllocateCommandBuffers(device, &fontUploadCmdBufAllocInfo, &fontUploadCmdBuf));

        VkCommandBufferBeginInfo begin_info{};
        {
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        }
        VK_CHECK(vkBeginCommandBuffer(fontUploadCmdBuf, &begin_info));

        ImGui_ImplVulkan_CreateFontsTexture(fontUploadCmdBuf);

        VkSubmitInfo end_info = {};
        {
            end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            end_info.commandBufferCount = 1;
            end_info.pCommandBuffers = &fontUploadCmdBuf;
        }
        
        VK_CHECK(vkEndCommandBuffer(fontUploadCmdBuf));
        
        VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &end_info, VK_NULL_HANDLE));

        VK_CHECK(vkDeviceWaitIdle(device));
        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }

    // UI State
    bool show_demo_window = true;

    ImGui::LoadIniSettingsFromMemory(myLayout);

    // Main Loop
    // Two draws. First draw draws triangle into an image with window 1 window size.
    // Second draw draws GUI. GUI would use the image drawn from the first draw.
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Draw Frame
        // Wait for the resources from the possible on flight frame
        vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        // Prepare the Dear ImGUI frame data
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

        // Copy the render result in first draw to the texture descriptor used by ImGUI and use that
        // as the output image of the first window.
        VkDescriptorSet my_image_texture = 0;
        ImGui::Begin("Window 1");

        ImVec2 winContentExtentUL = ImGui::GetWindowContentRegionMax();
        ImVec2 winContentExtentDR = ImGui::GetWindowContentRegionMin();
        ImVec2 winContentExtent = winContentExtentUL - winContentExtentDR;

        // Recreate scene render imgs, img views if necessary.
        uint32_t newWidth = std::max(static_cast<uint32_t>(winContentExtent.x), static_cast<uint32_t>(64));
        uint32_t newHeight = std::max(static_cast<uint32_t>(winContentExtent.y), static_cast<uint32_t>(64));
        RecreateSceneRenderObjs(newWidth, newHeight);
        AddTextureToImGUI(&my_image_texture, newWidth, newHeight);

        ImGui::Image((ImTextureID)my_image_texture, winContentExtent);
        ImGui::End();

        ImGui::Begin("Window 2");
        ImGui::End();

        // Reset unused previous frame's resource
        vkResetFences(device, 1, &inFlightFences[currentFrame]);
        vkResetCommandBuffer(commandBuffers[currentFrame], 0);

        // Fill the command buffer
        VkCommandBufferBeginInfo beginInfo{};
        {
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        }
        VK_CHECK(vkBeginCommandBuffer(commandBuffers[currentFrame], &beginInfo));

        // Draw the scene
        VkRenderPassBeginInfo sceneRenderPassBeginInfo{};
        VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
        {
            sceneRenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            sceneRenderPassBeginInfo.renderPass = sceneRenderPass;
            sceneRenderPassBeginInfo.framebuffer = sceneRenderFramebuffers[currentFrame];
            sceneRenderPassBeginInfo.renderArea.offset = { 0, 0 };
            sceneRenderPassBeginInfo.renderArea.extent = sceneRenderImagesExtents[currentFrame];
            sceneRenderPassBeginInfo.clearValueCount = 1;
            sceneRenderPassBeginInfo.pClearValues = &clearColor;
        }
        vkCmdBeginRenderPass(commandBuffers[currentFrame], &sceneRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Bind the graphics pipeline
        vkCmdBindPipeline(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

        // Set the viewport
        VkViewport viewport{};
        {
            viewport.x = 0.f;
            viewport.y = 0.f;
            viewport.width = (float)sceneRenderImagesExtents[currentFrame].width;
            viewport.height = (float)sceneRenderImagesExtents[currentFrame].height;
            viewport.minDepth = 0.f;
            viewport.maxDepth = 1.f;
        }
        vkCmdSetViewport(commandBuffers[currentFrame], 0, 1, &viewport);

        // Set the scissor
        VkRect2D scissor{};
        {
            scissor.offset = { 0, 0 };
            scissor.extent = sceneRenderImagesExtents[currentFrame];
            vkCmdSetScissor(commandBuffers[currentFrame], 0, 1, &scissor);
        }

        vkCmdDraw(commandBuffers[currentFrame], 3, 1, 0, 0);

        vkCmdEndRenderPass(commandBuffers[currentFrame]);

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

        // Create a barrier to wait for last draw's result
        vkCmdPipelineBarrier(commandBuffers[currentFrame],
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             0,
                             0, nullptr,
                             0, nullptr,
                             0, nullptr);

        // Begin the render pass and record relevant commands
        // Link framebuffer into the render pass
        VkRenderPassBeginInfo guiRenderPassBeginInfo{};
        {
            guiRenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            guiRenderPassBeginInfo.renderPass = guiRenderPass;
            guiRenderPassBeginInfo.framebuffer = swapchainFramebuffers[imageIndex];
            guiRenderPassBeginInfo.renderArea.offset = { 0, 0 };
            guiRenderPassBeginInfo.renderArea.extent = swapchainImageExtent;
            guiRenderPassBeginInfo.clearValueCount = 1;
            guiRenderPassBeginInfo.pClearValues = &clearColor;
        }
        vkCmdBeginRenderPass(commandBuffers[currentFrame], &guiRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Record the gui rendering commands. We draw GUI after scene because we don't have depth test. So, GUI
        // should be drawn later or the scene would overlay on the GUI.
        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();

        // Record the GUI rendering commands.
        ImGui_ImplVulkan_RenderDrawData(draw_data, commandBuffers[currentFrame]);

        vkCmdEndRenderPass(commandBuffers[currentFrame]);

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
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // Cleanup
    // Cleanup Swapchain
    CleanupSwapchain();

    // Cleanup scene rendering related objs
    CleanupSceneRenderObjs();

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

    for (auto itr : guiImgRenderSamplers)
    {
        vkDestroySampler(device, itr, nullptr);
    }

    // Destroy the command pool
    vkDestroyCommandPool(device, commandPool, nullptr);

    // Destroy shader modules
    vkDestroyShaderModule(device, vsShaderModule, nullptr);
    vkDestroyShaderModule(device, psShaderModule, nullptr);

    // Destroy the pipeline
    vkDestroyPipeline(device, graphicsPipeline, nullptr);

    // Destroy the pipeline layout
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

    // Destroy the render pass
    vkDestroyRenderPass(device, sceneRenderPass, nullptr);
    vkDestroyRenderPass(device, guiRenderPass, nullptr);

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
