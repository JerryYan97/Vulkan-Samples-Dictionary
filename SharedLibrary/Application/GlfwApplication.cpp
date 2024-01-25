#include "GlfwApplication.h"

#include "vk_mem_alloc.h"

#include "../Event/Event.h"
#include "../Utils/MathUtils.h"
#include "../HLSL/g_gammaCorrection_vert.h"
#include "../HLSL/g_gammaCorrection_frag.h"
#include "VulkanDbgUtils.h"
#include <glfw3.h>
#include <cassert>
#include <algorithm>

static bool g_framebufferResized = false;

static void FramebufferResizeCallback(
    GLFWwindow* window,
    int         width,
    int         height)
{
    g_framebufferResized = true;
}

namespace SharedLib
{

    // ================================================================================================================
    GlfwApplication::GlfwApplication() :
        Application::Application(),
        // m_currentFrame(0),
        m_surface(VK_NULL_HANDLE),
        m_swapchain(VK_NULL_HANDLE),
        m_pWindow(nullptr),
        m_presentQueueFamilyIdx(-1),
        m_choisenSurfaceFormat(),
        m_swapchainImageExtent(),
        m_presentQueue(VK_NULL_HANDLE),
        m_swapchainImgCnt(0),
        // m_swapchainNextImgId(0),
        m_acqSwapchainImgIdx(0),
        m_gammaCorrectionPipeline(),
        m_gammaCorrectionVsShaderModule(VK_NULL_HANDLE),
        m_gammaCorrectionPsShaderModule(VK_NULL_HANDLE),
        m_gammaCorrectionPipelineDesSetLayout(VK_NULL_HANDLE),
        m_gammaCorrectionPipelineLayout(VK_NULL_HANDLE)
    {}

    // ================================================================================================================
    GlfwApplication::~GlfwApplication()
    {
        CleanupGammaCorrectionPipelineAndRsrc();
        CleanupSwapchain();

        // Cleanup syn objects
        for (auto itr : m_renderFinishedSemaphores)
        {
            vkDestroySemaphore(m_device, itr, nullptr);
        }

        for (auto itr : m_inFlightFences)
        {
            vkDestroyFence(m_device, itr, nullptr);
        }

        // Destroy vulkan surface
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);

        glfwDestroyWindow(m_pWindow);

        glfwTerminate();
    }

    // ================================================================================================================
    bool GlfwApplication::WindowShouldClose()
    {
        return glfwWindowShouldClose(m_pWindow);
    }

    // ================================================================================================================
    // TODO: It should be a more general event creation function.
    HEvent GlfwApplication::CreateMiddleMouseEvent(
        bool isDown)
    {
        // Get IO information and create events
        SharedLib::HEventArguments args;
        args[crc32("IS_DOWN")] = isDown;

        if (isDown)
        {
            SharedLib::HFVec2 pos;
            double xpos, ypos;
            glfwGetCursorPos(m_pWindow, &xpos, &ypos);
            pos.ele[0] = xpos;
            pos.ele[1] = ypos;
            args[crc32("POS")] = pos;
        }

        SharedLib::HEvent mEvent(args, "MOUSE_MIDDLE_BUTTON");
        return mEvent;
    }

    // ================================================================================================================
    HEvent GlfwApplication::CreateKeyboardEvent(
        bool        isDown,
        std::string eventName)
    {
        SharedLib::HEventArguments args;
        args[crc32("IS_DOWN")] = isDown;
        SharedLib::HEvent mEvent(args, eventName);
        return mEvent;
    }

    // ================================================================================================================
    void GlfwApplication::FrameStart()
    {
        glfwPollEvents();
    }

    // ================================================================================================================
    bool GlfwApplication::WaitNextImgIdxOrNewSwapchain()
    {
        VkFence tmpFence = CreateFence();

        // Get next available image from the swapchain. Wait for relevant rsrc.
        // The vkAcquireNextImageKHR(...) itself is not obstructive but we can use a fence to make it obstructive
        // manually. Besides, other GPU rsrc are also linked to the m_acqSwapchainImgIdx and wait here so that we can
        // manage the sync easily.
        VkResult result = vkAcquireNextImageKHR(m_device,
            m_swapchain,
            UINT64_MAX,
            // m_imageAvailableSemaphores[m_currentFrame],
            VK_NULL_HANDLE,
            tmpFence,
            &m_acqSwapchainImgIdx);

        WaitAndDestroyTheFence(tmpFence);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            // The surface is imcompatiable with the swapchain (resize window).
            RecreateSwapchain();
            return false;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            // Not success or usable.
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        WaitTheFence(m_inFlightFences[m_acqSwapchainImgIdx]);

        // Reset unused previous frame's resource
        vkResetFences(m_device, 1, &m_inFlightFences[m_acqSwapchainImgIdx]);
        vkResetCommandBuffer(m_gfxCmdBufs[m_acqSwapchainImgIdx], 0);

        return true;
    }

    // ================================================================================================================
    void GlfwApplication::GfxCmdBufferFrameSubmitAndPresent()
    {
        // Submit the filled command buffer to the graphics queue to draw the image
        VkSubmitInfo submitInfo{};
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        {
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            // This draw would wait at dstStage and wait for the waitSemaphores
            // submitInfo.waitSemaphoreCount = 0;
            // submitInfo.pWaitSemaphores = &m_imageAvailableSemaphores[m_acqSwapchainImgIdx];
            submitInfo.pWaitDstStageMask = waitStages;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &m_gfxCmdBufs[m_acqSwapchainImgIdx];
            // This draw would let the signalSemaphore sign when it finishes
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &m_renderFinishedSemaphores[m_acqSwapchainImgIdx];
        }
        VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_acqSwapchainImgIdx]));

        // Put the swapchain into the present info and wait for the graphics queue previously before presenting.
        VkPresentInfoKHR presentInfo{};
        {
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = &m_renderFinishedSemaphores[m_acqSwapchainImgIdx];
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = &m_swapchain;
            presentInfo.pImageIndices = &m_acqSwapchainImgIdx;
        }
        VkResult result = vkQueuePresentKHR(m_presentQueue, &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || g_framebufferResized)
        {
            g_framebufferResized = false;
            RecreateSwapchain();
        }
        else if (result != VK_SUCCESS)
        {
            throw std::runtime_error("failed to present swap chain image!");
        }
    }

    // ================================================================================================================
    void GlfwApplication::FrameEnd()
    {
        // m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    // ================================================================================================================
    void GlfwApplication::InitGlfwWindowAndCallbacks()
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        const uint32_t WIDTH = 1280;
        const uint32_t HEIGHT = 640;
        m_pWindow = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
        glfwSetFramebufferSizeCallback(m_pWindow, FramebufferResizeCallback);
    }

    // ================================================================================================================
    void GlfwApplication::InitSwapchainSyncObjects()
    {
        // Create Sync objects
        m_renderFinishedSemaphores.resize(m_swapchainImgCnt);
        m_inFlightFences.resize(m_swapchainImgCnt);

        VkSemaphoreCreateInfo semaphoreInfo{};
        {
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        }

        VkFenceCreateInfo fenceInfo{};
        {
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        }

        for (size_t i = 0; i < m_swapchainImgCnt; i++)
        {
            VK_CHECK(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]));
            VK_CHECK(vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]));
        }
    }

    // ================================================================================================================
    void GlfwApplication::InitPresentQueueFamilyIdx()
    {
        // Initialize the logical device with the queue family that supports both graphics and present on the physical device
        // Find the queue family indices that supports graphics and present.
        uint32_t queueFamilyPropCount;
        vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyPropCount, nullptr);
        assert(queueFamilyPropCount >= 1);
        std::vector<VkQueueFamilyProperties> queueFamilyProps(queueFamilyPropCount);
        vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyPropCount, queueFamilyProps.data());

        bool foundPresent = false;
        for (unsigned int i = 0; i < queueFamilyPropCount; ++i)
        {
            VkBool32 supportPresentSurface;
            vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, i, m_surface, &supportPresentSurface);
            if (supportPresentSurface)
            {
                m_presentQueueFamilyIdx = i;
                foundPresent = true;
                break;
            }
        }
        assert(foundPresent);
    }

    // ================================================================================================================
    void GlfwApplication::InitPresentQueue()
    {
        vkGetDeviceQueue(m_device, m_presentQueueFamilyIdx, 0, &m_presentQueue);
    }

    // ================================================================================================================
    void GlfwApplication::InitSwapchain()
    {
        // Create the swapchain
        // Qurery surface capabilities.
        VkSurfaceCapabilitiesKHR surfaceCapabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &surfaceCapabilities);

        // Query surface formates
        uint32_t surfaceFormatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &surfaceFormatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
        if (surfaceFormatCount != 0)
        {
            vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &surfaceFormatCount, surfaceFormats.data());
        }

        // Query the present mode
        uint32_t surfacePresentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &surfacePresentModeCount, nullptr);
        std::vector<VkPresentModeKHR> surfacePresentModes(surfacePresentModeCount);
        if (surfacePresentModeCount != 0)
        {
            vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &surfacePresentModeCount, surfacePresentModes.data());
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
            // if (curFormat.format == VK_FORMAT_B8G8R8A8_SRGB && curFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            if (curFormat.format == VK_FORMAT_R8G8B8A8_SRGB && curFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                foundFormat = true;
                m_choisenSurfaceFormat = curFormat;
                break;
            }
        }
        assert(foundFormat);

        // Init swapchain's image extent
        int glfwFrameBufferWidth;
        int glfwFrameBufferHeight;
        glfwGetFramebufferSize(m_pWindow, &glfwFrameBufferWidth, &glfwFrameBufferHeight);

        m_swapchainImageExtent = {
            std::clamp(static_cast<uint32_t>(glfwFrameBufferWidth), surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width),
            std::clamp(static_cast<uint32_t>(glfwFrameBufferHeight), surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height)
        };

        uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
        if (surfaceCapabilities.maxImageCount > 0 && imageCount > surfaceCapabilities.maxImageCount)
        {
            imageCount = surfaceCapabilities.maxImageCount;
        }

        uint32_t queueFamiliesIndices[] = { m_graphicsQueueFamilyIdx, m_presentQueueFamilyIdx };
        VkSwapchainCreateInfoKHR swapchainCreateInfo{};
        {
            swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            swapchainCreateInfo.surface = m_surface;
            swapchainCreateInfo.minImageCount = imageCount;
            swapchainCreateInfo.imageFormat = m_choisenSurfaceFormat.format;
            swapchainCreateInfo.imageColorSpace = m_choisenSurfaceFormat.colorSpace;
            swapchainCreateInfo.imageExtent = m_swapchainImageExtent;
            swapchainCreateInfo.imageArrayLayers = 1;
            swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            if (m_graphicsQueueFamilyIdx != m_presentQueueFamilyIdx)
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
        VK_CHECK(vkCreateSwapchainKHR(m_device, &swapchainCreateInfo, nullptr, &m_swapchain));

        vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_swapchainImgCnt, nullptr);

        VkExtent3D swapchainExtent{};
        {
            swapchainExtent.width = m_swapchainImageExtent.width;
            swapchainExtent.height = m_swapchainImageExtent.height;
            swapchainExtent.depth = 1;
        }
        CreateSwapchainDepthImages(swapchainExtent);
        CreateSwapchainImageViews();
    }

    // ================================================================================================================
    void GlfwApplication::CreateSwapchainDepthImages(
        VkExtent3D extent)
    {
        uint32_t swapchainImageCount;
        vkGetSwapchainImagesKHR(m_device, m_swapchain, &swapchainImageCount, nullptr);

        VkImageCreateInfo depthImgsInfo{};
        {
            depthImgsInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            depthImgsInfo.imageType = VK_IMAGE_TYPE_2D;
            depthImgsInfo.format = VK_FORMAT_D16_UNORM;
            depthImgsInfo.extent = extent;
            depthImgsInfo.mipLevels = 1;
            depthImgsInfo.arrayLayers = 1;
            depthImgsInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            depthImgsInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            depthImgsInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                  VK_IMAGE_USAGE_TRANSFER_DST_BIT; // For vkCmdDepthStencilClear(...).
            depthImgsInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        }

        VmaAllocationCreateInfo depthImgsAllocInfo{};
        {
            depthImgsAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            depthImgsAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        }

        m_swapchainDepthImages.resize(swapchainImageCount);
        m_swapchainDepthImagesAllocs.resize(swapchainImageCount);
        for (uint32_t i = 0; i < swapchainImageCount; i++)
        {
            vmaCreateImage(*m_pAllocator,
                &depthImgsInfo,
                &depthImgsAllocInfo,
                &m_swapchainDepthImages[i],
                &m_swapchainDepthImagesAllocs[i],
                nullptr);
        }
    }

    // ================================================================================================================
    void GlfwApplication::CreateSwapchainImageViews()
    {
        // Create image views for the swapchain images
        uint32_t swapchainImageCount;
        vkGetSwapchainImagesKHR(m_device, m_swapchain, &swapchainImageCount, nullptr);
        m_swapchainColorImages.resize(swapchainImageCount);
        vkGetSwapchainImagesKHR(m_device, m_swapchain, &swapchainImageCount, m_swapchainColorImages.data());

        m_swapchainDepthImageViews.resize(swapchainImageCount);
        m_swapchainColorImageViews.resize(swapchainImageCount);
        for (size_t i = 0; i < swapchainImageCount; i++)
        {
            // Create the image view for the color images
            VkImageViewCreateInfo colorImgViewInfo{};
            {
                colorImgViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                colorImgViewInfo.image = m_swapchainColorImages[i];
                colorImgViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                colorImgViewInfo.format = m_choisenSurfaceFormat.format;
                colorImgViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
                colorImgViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
                colorImgViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
                colorImgViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
                colorImgViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                colorImgViewInfo.subresourceRange.baseMipLevel = 0;
                colorImgViewInfo.subresourceRange.levelCount = 1;
                colorImgViewInfo.subresourceRange.baseArrayLayer = 0;
                colorImgViewInfo.subresourceRange.layerCount = 1;
            }
            VK_CHECK(vkCreateImageView(m_device, &colorImgViewInfo, nullptr, &m_swapchainColorImageViews[i]));

            // Create the depth images views
            VkImageViewCreateInfo depthImgViewInfo{};
            {
                depthImgViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                depthImgViewInfo.image = m_swapchainDepthImages[i];
                depthImgViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                depthImgViewInfo.format = VK_FORMAT_D16_UNORM;
                depthImgViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                depthImgViewInfo.subresourceRange.levelCount = 1;
                depthImgViewInfo.subresourceRange.layerCount = 1;
            }
            VK_CHECK(vkCreateImageView(m_device, &depthImgViewInfo, nullptr, &m_swapchainDepthImageViews[i]));
        }
    }

    // ================================================================================================================
    void GlfwApplication::CleanupSwapchain()
    {
        // Cleanup the swap chain
        // Clean the image views
        for (auto imgView : m_swapchainColorImageViews)
        {
            vkDestroyImageView(m_device, imgView, nullptr);
        }

        for (uint32_t i = 0; i < m_swapchainDepthImageViews.size(); i++)
        {
            vkDestroyImageView(m_device, m_swapchainDepthImageViews[i], nullptr);
            vmaDestroyImage(*m_pAllocator, m_swapchainDepthImages[i], m_swapchainDepthImagesAllocs[i]);
        }

        // Destroy the swapchain
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
    }

    // ================================================================================================================
    void GlfwApplication::RecreateSwapchain()
    {
        int width = 0, height = 0;
        glfwGetFramebufferSize(m_pWindow, &width, &height);
        while (width == 0 || height == 0)
        {
            glfwGetFramebufferSize(m_pWindow, &width, &height);
            glfwWaitEvents();
        }

        vkDeviceWaitIdle(m_device);
        CleanupSwapchain();
        InitSwapchain();
    }

    // ================================================================================================================
    void GlfwApplication::CmdSwapchainColorImgClear(
        VkCommandBuffer cmdBuffer)
    {
        VkClearColorValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };

        VkImageSubresourceRange swapchainPresentSubResRange{};
        {
            swapchainPresentSubResRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            swapchainPresentSubResRange.baseMipLevel = 0;
            swapchainPresentSubResRange.levelCount = 1;
            swapchainPresentSubResRange.baseArrayLayer = 0;
            swapchainPresentSubResRange.layerCount = 1;
        }

        vkCmdClearColorImage(cmdBuffer,
                             GetSwapchainColorImage(),
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             &clearColor,
                             1, &swapchainPresentSubResRange);
    }

    // ================================================================================================================
    void GlfwApplication::CmdSwapchainDepthImgClear(
        VkCommandBuffer cmdBuffer)
    {
        VkClearDepthStencilValue clearDepthStencil = { 0.f, 0 };

        VkImageSubresourceRange swapchainPresentSubResRange{};
        {
            swapchainPresentSubResRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            swapchainPresentSubResRange.baseMipLevel = 0;
            swapchainPresentSubResRange.levelCount = 1;
            swapchainPresentSubResRange.baseArrayLayer = 0;
            swapchainPresentSubResRange.layerCount = 1;
        }

        vkCmdClearDepthStencilImage(cmdBuffer,
                                    GetSwapchainDepthImage(),
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    &clearDepthStencil,
                                    1, &swapchainPresentSubResRange);
    }

    // ================================================================================================================
    void GlfwApplication::CmdSwapchainColorImgLayoutTrans(
        VkCommandBuffer      cmdBuffer,
        VkImageLayout        oldLayout,
        VkImageLayout        newLayout,
        VkAccessFlags        srcAccessMask,
        VkAccessFlags        dstAccessMask,
        VkPipelineStageFlags srcStageMask,
        VkPipelineStageFlags dstStageMask)
    {
        VkImageSubresourceRange swapchainPresentSubResRange{};
        {
            swapchainPresentSubResRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            swapchainPresentSubResRange.baseMipLevel = 0;
            swapchainPresentSubResRange.levelCount = 1;
            swapchainPresentSubResRange.baseArrayLayer = 0;
            swapchainPresentSubResRange.layerCount = 1;
        }

        // Transform the layout of the swapchain from undefined to render target.
        VkImageMemoryBarrier swapchainRenderTargetTransBarrier{};
        {
            swapchainRenderTargetTransBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            swapchainRenderTargetTransBarrier.srcAccessMask = srcAccessMask;
            swapchainRenderTargetTransBarrier.dstAccessMask = dstAccessMask;
            swapchainRenderTargetTransBarrier.oldLayout = oldLayout;
            swapchainRenderTargetTransBarrier.newLayout = newLayout;
            swapchainRenderTargetTransBarrier.image = GetSwapchainColorImage();
            swapchainRenderTargetTransBarrier.subresourceRange = swapchainPresentSubResRange;
        }

        vkCmdPipelineBarrier(
            cmdBuffer,
            srcStageMask,
            dstStageMask,
            0,
            0, nullptr,
            0, nullptr,
            1, &swapchainRenderTargetTransBarrier);
    }

    // ================================================================================================================
    // TODO: I maybe able to make more abstraction with the color image to reduce some repeated code.
    void GlfwApplication::CmdSwapchainDepthImgLayoutTrans(
        VkCommandBuffer      cmdBuffer,
        VkImageLayout        oldLayout,
        VkImageLayout        newLayout,
        VkAccessFlags        srcAccessMask,
        VkAccessFlags        dstAccessMask,
        VkPipelineStageFlags srcStageMask,
        VkPipelineStageFlags dstStageMask)
    {
        VkImageSubresourceRange swapchainPresentSubResRange{};
        {
            swapchainPresentSubResRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            swapchainPresentSubResRange.baseMipLevel = 0;
            swapchainPresentSubResRange.levelCount = 1;
            swapchainPresentSubResRange.baseArrayLayer = 0;
            swapchainPresentSubResRange.layerCount = 1;
        }

        // Transform the layout of the swapchain from undefined to render target.
        VkImageMemoryBarrier swapchainRenderTargetTransBarrier{};
        {
            swapchainRenderTargetTransBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            swapchainRenderTargetTransBarrier.srcAccessMask = srcAccessMask;
            swapchainRenderTargetTransBarrier.dstAccessMask = dstAccessMask;
            swapchainRenderTargetTransBarrier.oldLayout = oldLayout;
            swapchainRenderTargetTransBarrier.newLayout = newLayout;
            swapchainRenderTargetTransBarrier.image = GetSwapchainDepthImage();
            swapchainRenderTargetTransBarrier.subresourceRange = swapchainPresentSubResRange;
        }

        vkCmdPipelineBarrier(
            cmdBuffer,
            srcStageMask,
            dstStageMask,
            0,
            0, nullptr,
            0, nullptr,
            1, &swapchainRenderTargetTransBarrier);
    }

    // ================================================================================================================
    void GlfwApplication::CmdSwapchainColorImgToPresent(
        VkCommandBuffer cmdBuffer)
    {
        VkImageSubresourceRange swapchainPresentSubResRange{};
        {
            swapchainPresentSubResRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            swapchainPresentSubResRange.baseMipLevel = 0;
            swapchainPresentSubResRange.levelCount = 1;
            swapchainPresentSubResRange.baseArrayLayer = 0;
            swapchainPresentSubResRange.layerCount = 1;
        }

        // Transform the swapchain image layout from render target to present.
        // Transform the layout of the swapchain from undefined to render target.
        VkImageMemoryBarrier swapchainPresentTransBarrier{};
        {
            swapchainPresentTransBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            swapchainPresentTransBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            swapchainPresentTransBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            swapchainPresentTransBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            swapchainPresentTransBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            swapchainPresentTransBarrier.image = GetSwapchainColorImage();
            swapchainPresentTransBarrier.subresourceRange = swapchainPresentSubResRange;
        }

        vkCmdPipelineBarrier(cmdBuffer,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &swapchainPresentTransBarrier);
    }

    // ================================================================================================================
    void GlfwApplication::InitGammaCorrectionPipelineAndRsrc()
    {
        // Create shader modules
        m_gammaCorrectionVsShaderModule = CreateShaderModuleFromRam((uint32_t*)gammaCorrection_vertScript,
                                                                    sizeof(gammaCorrection_vertScript));
        m_gammaCorrectionPsShaderModule = CreateShaderModuleFromRam((uint32_t*)gammaCorrection_fragScript,
                                                                    sizeof(gammaCorrection_fragScript));

        // Create Descriptor Set Layout
        VkDescriptorSetLayoutBinding inputImgBinding{};
        {
            inputImgBinding.binding = 0;
            inputImgBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            inputImgBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            inputImgBinding.descriptorCount = 1;
        }

        // Create pipeline's descriptors layout
        // The Vulkan spec states: The VkDescriptorSetLayoutBinding::binding members of the elements of the pBindings array 
        // must each have different values 
        // (https://vulkan.lunarg.com/doc/view/1.3.236.0/windows/1.3-extensions/vkspec.html#VUID-VkDescriptorSetLayoutCreateInfo-binding-00279)
        VkDescriptorSetLayoutCreateInfo pipelineDesSetLayoutInfo{};
        {
            pipelineDesSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            // Setting this flag tells the descriptor set layouts that no actual descriptor sets are allocated but instead pushed at command buffer creation time
            pipelineDesSetLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
            pipelineDesSetLayoutInfo.bindingCount = 1;
            pipelineDesSetLayoutInfo.pBindings = &inputImgBinding;
        }

        VK_CHECK(vkCreateDescriptorSetLayout(m_device,
                                             &pipelineDesSetLayoutInfo,
                                             nullptr,
                                             &m_gammaCorrectionPipelineDesSetLayout));

        // Create Pipeline Layout
        VkPushConstantRange pushConstantInfo{};
        {
            pushConstantInfo.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            pushConstantInfo.offset = 0;
            pushConstantInfo.size = sizeof(float) * 2; // Width and height.
        }

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        {
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = 1;
            pipelineLayoutInfo.pSetLayouts = &m_gammaCorrectionPipelineDesSetLayout;
            pipelineLayoutInfo.pushConstantRangeCount = 1;
            pipelineLayoutInfo.pPushConstantRanges = &pushConstantInfo;
        }

        VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_gammaCorrectionPipelineLayout));

        // Create the pipeline
        VkPipelineRenderingCreateInfoKHR pipelineRenderCreateInfo{};
        {
            pipelineRenderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
            pipelineRenderCreateInfo.colorAttachmentCount = 1;
            pipelineRenderCreateInfo.pColorAttachmentFormats = &m_choisenSurfaceFormat.format;
        }

        m_gammaCorrectionPipeline.SetPNext(&pipelineRenderCreateInfo);
        m_gammaCorrectionPipeline.SetPipelineLayout(m_gammaCorrectionPipelineLayout);

        VkPipelineShaderStageCreateInfo shaderStgsInfo[2] = {};
        shaderStgsInfo[0] = CreateDefaultShaderStgCreateInfo(m_gammaCorrectionVsShaderModule,
                                                             VK_SHADER_STAGE_VERTEX_BIT);
        shaderStgsInfo[1] = CreateDefaultShaderStgCreateInfo(m_gammaCorrectionPsShaderModule,
                                                             VK_SHADER_STAGE_FRAGMENT_BIT);
        
        m_gammaCorrectionPipeline.SetShaderStageInfo(shaderStgsInfo, 2);
        m_gammaCorrectionPipeline.CreatePipeline(m_device);
    }

    // ================================================================================================================
    void GlfwApplication::CleanupGammaCorrectionPipelineAndRsrc()
    {
        if (m_gammaCorrectionVsShaderModule != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(m_device, m_gammaCorrectionVsShaderModule, nullptr);
        }
        
        if (m_gammaCorrectionPsShaderModule != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(m_device, m_gammaCorrectionPsShaderModule, nullptr);
        }

        if (m_gammaCorrectionPipelineDesSetLayout != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(m_device, m_gammaCorrectionPipelineDesSetLayout, nullptr);
        }
        
        if (m_gammaCorrectionPipelineLayout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(m_device, m_gammaCorrectionPipelineLayout, nullptr);
        }
    }

    // ================================================================================================================
    void GlfwApplication::CmdSwapchainColorImgGammaCorrect(
        VkCommandBuffer cmdBuffer,
        VkImageView     srcImgView,
        VkSampler       srcImgSampler)
    {
        VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
        VkRenderingAttachmentInfoKHR gammaCorrectionColorAttachmentInfo{};
        {
            gammaCorrectionColorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
            gammaCorrectionColorAttachmentInfo.imageView = m_swapchainColorImageViews[m_acqSwapchainImgIdx];
            gammaCorrectionColorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
            gammaCorrectionColorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            gammaCorrectionColorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            gammaCorrectionColorAttachmentInfo.clearValue = clearColor;
        }

        VkRenderingInfoKHR renderInfo{};
        {
            renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
            renderInfo.renderArea.offset = { 0, 0 };
            renderInfo.renderArea.extent = m_swapchainImageExtent;
            renderInfo.layerCount = 1;
            renderInfo.colorAttachmentCount = 1;
            renderInfo.pColorAttachments = &gammaCorrectionColorAttachmentInfo;
            // renderInfo.pDepthAttachment = &geoPassDepthAttachmentInfo;
        }

        vkCmdBeginRendering(cmdBuffer, &renderInfo);

        VkDescriptorImageInfo srcImgDescriptorInfo{};
        {
            srcImgDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            srcImgDescriptorInfo.imageView = srcImgView;
            srcImgDescriptorInfo.sampler = srcImgSampler;
        }

        VkWriteDescriptorSet writeDescriptorSet{};
        {
            writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writeDescriptorSet.descriptorCount = 1;
            writeDescriptorSet.dstBinding = 0;
            writeDescriptorSet.pImageInfo = &srcImgDescriptorInfo;
        }
        m_vkCmdPushDescriptorSetKHR(cmdBuffer,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    m_gammaCorrectionPipelineLayout,
                                    0, 1, &writeDescriptorSet);

        float pushConstantData[2] = {m_swapchainImageExtent.width, m_swapchainImageExtent.height};

        vkCmdPushConstants(cmdBuffer,
            m_gammaCorrectionPipelineLayout,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(pushConstantData), pushConstantData);

        VkViewport viewport{};
        {
            viewport.x = 0.f;
            viewport.y = 0.f;
            viewport.width = (float)m_swapchainImageExtent.width;
            viewport.height = (float)m_swapchainImageExtent.height;
            viewport.minDepth = 0.f;
            viewport.maxDepth = 1.f;
        }

        VkRect2D scissor{};
        {
            scissor.offset = { 0, 0 };
            scissor.extent = m_swapchainImageExtent;
        }

        vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
        vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gammaCorrectionPipeline.GetVkPipeline());
        vkCmdDraw(cmdBuffer, 6, 1, 0, 0);

        vkCmdEndRendering(cmdBuffer);
    }

    // ================================================================================================================
    DearImGuiApplication::DearImGuiApplication()
        : GlfwApplication()
    {}

    // ================================================================================================================
    DearImGuiApplication::~DearImGuiApplication()
    {}
}