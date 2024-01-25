#pragma once
#include "Application.h"
#include "../Pipeline/Pipeline.h"

struct GLFWwindow;

namespace SharedLib
{

    class HEvent;

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
        bool WaitNextImgIdxOrNewSwapchain(); // True: Get the idx; False: Recreate Swapchain.
        virtual void FrameStart();
        virtual void FrameEnd();

        void GfxCmdBufferFrameSubmitAndPresent();

        VkFence GetCurrentFrameFence() { return m_inFlightFences[m_acqSwapchainImgIdx]; }
        VkCommandBuffer GetCurrentFrameGfxCmdBuffer() { return m_gfxCmdBufs[m_acqSwapchainImgIdx]; }
        uint32_t GetCurrentFrame() { return m_acqSwapchainImgIdx; }
        VkImage GetSwapchainColorImage() { return m_swapchainColorImages[m_acqSwapchainImgIdx]; }
        VkImageView GetSwapchainColorImageView() { return m_swapchainColorImageViews[m_acqSwapchainImgIdx]; }
        VkImage GetSwapchainDepthImage() { return m_swapchainDepthImages[m_acqSwapchainImgIdx]; }
        VkImageView GetSwapchainDepthImageView() { return m_swapchainDepthImageViews[m_acqSwapchainImgIdx]; }
        VkExtent2D GetSwapchainImageExtent() { return m_swapchainImageExtent; }

        void CmdSwapchainColorImgLayoutTrans(VkCommandBuffer      cmdBuffer,
                                             VkImageLayout        oldLayout,
                                             VkImageLayout        newLayout,
                                             VkAccessFlags        srcAccessMask,
                                             VkAccessFlags        dstAccessMask,
                                             VkPipelineStageFlags srcStageMask,
                                             VkPipelineStageFlags dstStageMask);
        
        void CmdSwapchainDepthImgLayoutTrans(VkCommandBuffer      cmdBuffer,
                                             VkImageLayout        oldLayout,
                                             VkImageLayout        newLayout,
                                             VkAccessFlags        srcAccessMask,
                                             VkAccessFlags        dstAccessMask,
                                             VkPipelineStageFlags srcStageMask,
                                             VkPipelineStageFlags dstStageMask);

        void CmdSwapchainColorImgToPresent(VkCommandBuffer cmdBuffer);
        void CmdSwapchainColorImgClear(VkCommandBuffer cmdBuffer);
        void CmdSwapchainDepthImgClear(VkCommandBuffer cmdBuffer);

        // Assume that the swapchain color image is in the color attachment layout.
        // The user needs to provide the sampler and color image in R32G32B32A32 SFloat format.
        // The format is assumed to be the shader input layout.
        void CmdSwapchainColorImgGammaCorrect(VkCommandBuffer cmdBuffer,
                                              VkImageView     srcImgView,
                                              VkSampler       srcImgSampler);

    protected:
        void InitSwapchain();
        void InitPresentQueueFamilyIdx();
        void InitPresentQueue();
        void InitSwapchainSyncObjects();
        void InitGlfwWindowAndCallbacks();
        void InitGammaCorrectionPipelineAndRsrc();

        HEvent CreateMiddleMouseEvent(bool isDown);
        HEvent CreateKeyboardEvent(bool isDown, std::string eventName);

        // The class manages both of the creation and destruction of the objects below.
        // uint32_t                 m_currentFrame; -- We will wait until all rsrc ready when we grab the next image.
        // uint32_t                 m_swapchainNextImgId;
        uint32_t                 m_acqSwapchainImgIdx; // Obstructive to make sure all rsrc at this slot is available.
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

        // std::vector<VkSemaphore> m_imageAvailableSemaphores;
        std::vector<VkSemaphore> m_renderFinishedSemaphores;
        std::vector<VkFence>     m_inFlightFences;

        // Deferred rendering requires final gamma correction, which can be put into the glfw application as util.
        SharedLib::Pipeline   m_gammaCorrectionPipeline;
        VkShaderModule        m_gammaCorrectionVsShaderModule;
        VkShaderModule        m_gammaCorrectionPsShaderModule;
        VkDescriptorSetLayout m_gammaCorrectionPipelineDesSetLayout;
        VkPipelineLayout      m_gammaCorrectionPipelineLayout;

    private:
        void CreateSwapchainImageViews();
        void CreateSwapchainDepthImages(VkExtent3D extent);
        void CleanupSwapchain();
        void RecreateSwapchain();
        
        void CleanupGammaCorrectionPipelineAndRsrc();
    };

    // Vulkan application draws DearImGui's Guis and uses the glfw backend.
    class DearImGuiApplication : public GlfwApplication
    {
    public:
        DearImGuiApplication();
        ~DearImGuiApplication();
    };
}