#pragma once

#include <vulkan/vulkan.h>
#include <fstream>
#include <vector>
#include <set>

VK_DEFINE_HANDLE(VmaAllocator)
VK_DEFINE_HANDLE(VmaAllocation)
struct GLFWwindow;

enum VmaMemoryUsage;
typedef VkFlags VmaAllocationCreateFlags;

// The design philosophy of the SharedLib is to reuse code as much as possible and writing new code in examples as less as possible.
// This would lead to:
// - Small granular functions and versatile input arguments.
// - Speed is not a problem. The key is easy to use. So, I should feel free to use all kinds of data structure.
// - All CmdBuffer operations need to stay in the main.cpp.
// - In principle, all member variables should be init in InitXXX(...) and destroied in the destructor. Or, the variable
//   shouldn't be in the class, but can be created by the public interface.
// - Sync, CmdBuffer operations should be explicit in the main.cpp.
// TODO1: I may need a standalone pipeline class.
// TODO2: Dictionary Vma/VkBuffer/VkImage Management -- Need explicit create/destroy; return internal ids; element: id - {alloc, vkBuffer}.
namespace SharedLib
{
    class HEvent;

    constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    // Base Vulkan application without a swapchain -- Basically abstract.
    // It has vmaAllocator and descriptor pool. Besides, it also provides basic vulkan objects creation functions.
    class Application
    {
    public:
        Application();
        ~Application();

        // The InitXXX(...) should be called in it. It is expected to called after the constructor.
        virtual void AppInit() = 0;

        void CreateVmaVkBuffer(VmaMemoryUsage           vmaMemUsage, 
                               VmaAllocationCreateFlags vmaAllocFlags, 
                               VkSharingMode            sharingMode,
                               VkBufferUsageFlags       bufferUsageFlag,
                               VkDeviceSize             byteNum,
                               VkBuffer*                pBuffer,
                               VmaAllocation*           pAllocation);

        void CreateVmaVkImage();

        void CopyRamDataToGpuBuffer(void*         pSrc,
                                    VkBuffer      dstBuffer, 
                                    VmaAllocation dstAllocation,
                                    uint32_t      byteNum);

        void SubmitCmdBufToGfxQueue(VkCommandBuffer cmdBuf, VkFence signalFence);

        VmaAllocator* GetVmaAllocator() { return m_pAllocator; }
        VkCommandBuffer GetGfxCmdBuffer(uint32_t i) { return m_gfxCmdBufs[i]; }
        VkDevice GetVkDevice() { return m_device; }

    protected:
        // VkInstance, VkPhysicalDevice, VkDevice, gfxFamilyQueueIdx, presentFamilyQueueIdx,
        // computeFamilyQueueIdx (TODO), descriptor pool, vmaAllocator.
        // InitXXX(...) functions must initialize a member object instead of returning it.
        // Member objects initialized by the InitXXX(...) functions have to be destroied by destructor.
        void InitInstance(const std::vector<const char*>& instanceExts,
                          const uint32_t                  instanceExtsCnt);

        void InitPhysicalDevice();

        void InitGfxQueueFamilyIdx();
        
        void InitDevice(const std::vector<const char*>&             deviceExts,
                        const uint32_t                              deviceExtsCnt,
                        const std::vector<VkDeviceQueueCreateInfo>& queueCreateInfos,
                        void*                                       pNext);

        void InitGraphicsQueue();
        void InitVmaAllocator();
        void InitDescriptorPool();
        void InitGfxCommandPool();
        void InitGfxCommandBuffers(const uint32_t cmdBufCnt);

        // CreateXXX(...) functions are more flexible. They are utility functions for children classes.
        // CreateXXX(...) cannot initialize any member objects. They have to return objects.
        VkShaderModule                       CreateShaderModule(const std::string& spvName);
        std::vector<VkDeviceQueueCreateInfo> CreateDeviceQueueInfos(const std::set<uint32_t>& uniqueQueueFamilies);
        
        VkPipelineShaderStageCreateInfo CreateDefaultShaderStgCreateInfo(const VkShaderModule& shaderModule, const VkShaderStageFlagBits stg);

        // The class manages both of the creation and destruction of the objects below.
        VkInstance       m_instance;
        VkPhysicalDevice m_physicalDevice;
        VkDevice         m_device;
        unsigned int     m_graphicsQueueFamilyIdx;
        VkDescriptorPool m_descriptorPool;
        VkQueue          m_graphicsQueue;
        VkCommandPool    m_gfxCmdPool;
        
        VkDebugUtilsMessengerEXT     m_debugMessenger;
        VmaAllocator*                m_pAllocator;
        std::vector<VkCommandBuffer> m_gfxCmdBufs;
    };

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
        VkImage GetSwapchainImage(uint32_t i) { return m_swapchainImages[i]; }
        VkImageView GetSwapchainImageView(uint32_t i) { return m_swapchainImageViews[i]; }
        VkExtent2D GetSwapchainImageExtent() { return m_swapchainImageExtent; }

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
        std::vector<VkImageView> m_swapchainImageViews;
        std::vector<VkImage>     m_swapchainImages;

        std::vector<VkSemaphore> m_imageAvailableSemaphores;
        std::vector<VkSemaphore> m_renderFinishedSemaphores;
        std::vector<VkFence>     m_inFlightFences;

    private:
        void CreateSwapchainImageViews();
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