#pragma once

#include <vulkan/vulkan.h>
#include <fstream>
#include <vector>
#include <set>

VK_DEFINE_HANDLE(VmaAllocator)
struct GLFWwindow;

// The design philosophy of the SharedLib is to reuse code as much as possible and writing new code in examples as less as possible.
// This would lead to:
// - Small granular functions and versatile input arguments.
// - Speed is not a problem. The key is easy to use. So, I should feel free to use all kinds of data structure.
// - All CmdBuffer operations need to stay in the main.cpp.
// - In principle, all member variables should be init in InitXXX(...) and destroied in the destructor. Or, the variable
//   shouldn't be in the class, but can be created by the public interface.
// 
// TODO: I may need a standalone pipeline class.
namespace SharedLib
{
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

    protected:
        // VkInstance, VkPhysicalDevice, VkDevice, gfxFamilyQueueIdx, presentFamilyQueueIdx,
        // computeFamilyQueueIdx (TODO), descriptor pool, vmaAllocator.
        // InitXXX(...) functions must initialize a member object instead of returning it.
        // Member objects initialized by the InitXXX(...) functions have to be destroied by destructor.
        void InitInstance(const std::vector<const char*>& instanceExts,
                          const uint32_t                  instanceExtsCnt);

        void InitPhysicalDevice();

        void InitGfxQueueFamilyIdx();

        void InitQueueCreateInfos(const std::set<uint32_t>&             uniqueQueueFamilies,
                                  std::vector<VkDeviceQueueCreateInfo>& queueCreateInfos);

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
        VkPipeline                           CreateGfxPipeline(const VkShaderModule& vsShaderModule,
                                                               const VkShaderModule& psShaderModule,
                                                               const VkPipelineRenderingCreateInfoKHR& pipelineRenderCreateInfo,
                                                               const VkPipelineLayout& pipelineLayout);

        void CreateVmaVkBuffer();
        void CreateVmaVkImage();

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
    class GlfwApplication : public Application
    {
    public:
        GlfwApplication();
        ~GlfwApplication();

        virtual void AppInit() override { /* Unimplemented */ };

    protected:
        void InitSwapchain();
        void InitPresentQueueFamilyIdx();
        void InitPresentQueue();

        // The class manages both of the creation and destruction of the objects below.
        uint32_t                 m_currentFrame;
        VkSurfaceKHR             m_surface;
        VkSwapchainKHR           m_swapchain;
        GLFWwindow*              m_pWindow;
        unsigned int             m_presentQueueFamilyIdx;
        VkSurfaceFormatKHR       m_choisenSurfaceFormat;
        VkExtent2D               m_swapchainImageExtent;
        VkQueue                  m_presentQueue;
        std::vector<VkImageView> m_swapchainImageViews;
        std::vector<VkImage>     m_swapchainImages;

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