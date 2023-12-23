#pragma once
#include "Application.h"

struct GLFWwindow;

namespace SharedLib
{

    class HEvent;

    constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    // Vulkan application with a swapchain and glfwWindow.
    // - Hide swapchain operations.
    // - Hide GLFW.
    class GlfwApplication : public Application
    {
    public:
        GlfwApplication();
        ~GlfwApplication();

        virtual void AppInit() override { /* Unimplemented */ };

        bool WindowShouldClose();
        bool NextImgIdxOrNewSwapchain(uint32_t& idx); // True: Get the idx; False: Recreate Swapchain.
        virtual void FrameStart();
        virtual void FrameEnd();

        void GfxCmdBufferFrameSubmitAndPresent();

        VkFence GetCurrentFrameFence() { return m_inFlightFences[m_currentFrame]; }
        VkCommandBuffer GetCurrentFrameGfxCmdBuffer() { return m_gfxCmdBufs[m_currentFrame]; }
        uint32_t GetCurrentFrame() { return m_currentFrame; }
        VkImage GetSwapchainColorImage(uint32_t i) { return m_swapchainColorImages[i]; }
        VkImageView GetSwapchainColorImageView(uint32_t i) { return m_swapchainColorImageViews[i]; }
        VkImage GetSwapchainDepthImage(uint32_t i) { return m_swapchainDepthImages[i]; }
        VkImageView GetSwapchainDepthImageView(uint32_t i) { return m_swapchainDepthImageViews[i]; }
        VkExtent2D GetSwapchainImageExtent() { return m_swapchainImageExtent; }

        void CmdSwapchainColorImgToRenderTarget(VkCommandBuffer cmdBuffer);
        void CmdSwapchainColorImgToPresent(VkCommandBuffer cmdBuffer);

    protected:
        void InitSwapchain();
        void InitPresentQueueFamilyIdx();
        void InitPresentQueue();
        void InitSwapchainSyncObjects();
        void InitGlfwWindowAndCallbacks();

        HEvent CreateMiddleMouseEvent(bool isDown);

        // The class manages both of the creation and destruction of the objects below.
        uint32_t                 m_currentFrame;
        uint32_t                 m_swapchainNextImgId;
        VkSurfaceKHR             m_surface;
        VkSwapchainKHR           m_swapchain;
        GLFWwindow*              m_pWindow;
        unsigned int             m_presentQueueFamilyIdx;
        VkSurfaceFormatKHR       m_choisenSurfaceFormat;
        VkExtent2D               m_swapchainImageExtent;
        VkQueue                  m_presentQueue;
        uint32_t m_swapchainImgCnt;

        std::vector<VkImageView>   m_swapchainColorImageViews;
        std::vector<VkImage>       m_swapchainColorImages;
        std::vector<VkImageView>   m_swapchainDepthImageViews;
        std::vector<VkImage>       m_swapchainDepthImages;
        std::vector<VmaAllocation> m_swapchainDepthImagesAllocs;

        std::vector<VkSemaphore> m_imageAvailableSemaphores;
        std::vector<VkSemaphore> m_renderFinishedSemaphores;
        std::vector<VkFence>     m_inFlightFences;

    private:
        void CreateSwapchainImageViews();
        void CreateSwapchainDepthImages(VkExtent3D extent);
        void CleanupSwapchain();
        void RecreateSwapchain();
    };

    // Vulkan application draws DearImGui's Guis and uses the glfw backend.
    class DearImGuiApplication : public GlfwApplication
    {
    public:
        DearImGuiApplication();
        ~DearImGuiApplication();
    };
}