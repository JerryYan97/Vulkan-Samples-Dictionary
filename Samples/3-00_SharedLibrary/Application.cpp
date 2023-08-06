#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "Application.h"
#include "VulkanDbgUtils.h"
#include "../3-00_SharedLibrary/Event.h"
#include "../3-00_SharedLibrary/MathUtils.h"
#include <cassert>
#include <glfw3.h>

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
    Application::Application() :
        m_graphicsQueueFamilyIdx(-1),
        m_instance(VK_NULL_HANDLE),
        m_physicalDevice(VK_NULL_HANDLE),
        m_device(VK_NULL_HANDLE),
        m_descriptorPool(VK_NULL_HANDLE),
        m_graphicsQueue(VK_NULL_HANDLE),
        m_pAllocator(nullptr)
    {
        m_pAllocator = new VmaAllocator();
    }

    // ================================================================================================================
    Application::~Application()
    {
        // Destroy the command pool
        vkDestroyCommandPool(m_device, m_gfxCmdPool, nullptr);

        // Destroy the descriptor pool
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);

        // Destroy the allocator
        vmaDestroyAllocator(*m_pAllocator);
        delete m_pAllocator;

        // Destroy the device
        vkDestroyDevice(m_device, nullptr);

        // Destroy debug messenger
        auto fpVkDestroyDebugUtilsMessengerEXT = 
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
        if (fpVkDestroyDebugUtilsMessengerEXT == nullptr)
        {
            exit(1);
        }
        fpVkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);

        // Destroy instance
        vkDestroyInstance(m_instance, nullptr);

        // Release all temp used memory
        for (void* ptr : m_heapMemPtrVec)
        {
            delete ptr;
        }

        for (void* ptr : m_heapArrayMemPtrVec)
        {
            delete[] ptr;
        }
    }

    void Application::GpuWaitForIdle()
    {
        VK_CHECK(vkDeviceWaitIdle(m_device));
    }

    // ================================================================================================================
    void Application::InitInstance(
        const std::vector<const char*>& instanceExts,
        const uint32_t                  instanceExtsCnt)
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
                std::cout << "Something went very wrong, cannot find " << VK_EXT_DEBUG_UTILS_EXTENSION_NAME
                    << " extension" << std::endl;
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

        // Initialize instance and application
        VkApplicationInfo appInfo{};
        {
            appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            appInfo.pApplicationName = "VulkanExample";
            appInfo.applicationVersion = 1;
            appInfo.pEngineName = "VulkanDict";
            appInfo.engineVersion = 1;
            appInfo.apiVersion = VK_API_VERSION_1_3;
        }

        std::vector<const char*> instExtensions(instanceExts);
        instExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
        VkInstanceCreateInfo instanceCreateInfo{};
        {
            instanceCreateInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            instanceCreateInfo.pNext = &debugCreateInfo;
            instanceCreateInfo.pApplicationInfo = &appInfo;
            instanceCreateInfo.enabledExtensionCount = instanceExtsCnt + 1;
            instanceCreateInfo.ppEnabledExtensionNames = instExtensions.data();
            instanceCreateInfo.enabledLayerCount = 1;
            instanceCreateInfo.ppEnabledLayerNames = &validationLayerName;
        }
        VK_CHECK(vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance));

        // Create debug messenger
        auto fpVkCreateDebugUtilsMessengerExt = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");
        if (fpVkCreateDebugUtilsMessengerExt == nullptr)
        {
            exit(1);
        }
        VK_CHECK(fpVkCreateDebugUtilsMessengerExt(m_instance, &debugCreateInfo, nullptr, &m_debugMessenger));
    }

    // ================================================================================================================
    void Application::InitPhysicalDevice()
    {
        // Enumerate the physicalDevices, select the first one and display the name of it.
        uint32_t phyDeviceCount;
        VK_CHECK(vkEnumeratePhysicalDevices(m_instance, &phyDeviceCount, nullptr));
        assert(phyDeviceCount >= 1);
        std::vector<VkPhysicalDevice> phyDeviceVec(phyDeviceCount);
        VK_CHECK(vkEnumeratePhysicalDevices(m_instance, &phyDeviceCount, phyDeviceVec.data()));
        m_physicalDevice = phyDeviceVec[0];
        VkPhysicalDeviceProperties physicalDevProperties;
        vkGetPhysicalDeviceProperties(m_physicalDevice, &physicalDevProperties);
        std::cout << "Device name:" << physicalDevProperties.deviceName << std::endl;
    }

    // ================================================================================================================
    void Application::InitGfxQueueFamilyIdx()
    {
        // Initialize the logical device with the queue family that supports both graphics and present on the physical device
        // Find the queue family indices that supports graphics and present.
        uint32_t queueFamilyPropCount;
        vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyPropCount, nullptr);
        assert(queueFamilyPropCount >= 1);
        std::vector<VkQueueFamilyProperties> queueFamilyProps(queueFamilyPropCount);
        vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyPropCount, queueFamilyProps.data());

        bool foundGraphics = false;
        for (unsigned int i = 0; i < queueFamilyPropCount; ++i)
        {
            if (queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                m_graphicsQueueFamilyIdx = i;
                foundGraphics = true;
                break;
            }
        }
        assert(foundGraphics);
    }

    // ================================================================================================================
    void Application::InitDevice(
        const std::vector<const char*>&             deviceExts,
        const uint32_t                              deviceExtsCnt,
        const std::vector<VkDeviceQueueCreateInfo>& queueCreateInfos,
        void*                                       pNext)
    {
        VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_feature{};
        {
            dynamic_rendering_feature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
            dynamic_rendering_feature.dynamicRendering = VK_TRUE;
        }

        // Assembly the info into the device create info
        VkDeviceCreateInfo deviceInfo{};
        {
            deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            deviceInfo.pNext = pNext;
            deviceInfo.queueCreateInfoCount = uint32_t(queueCreateInfos.size());
            deviceInfo.pQueueCreateInfos = queueCreateInfos.data();
            deviceInfo.enabledExtensionCount = deviceExtsCnt;
            deviceInfo.ppEnabledExtensionNames = deviceExts.data();
        }

        // Create the logical device
        VK_CHECK(vkCreateDevice(m_physicalDevice, &deviceInfo, nullptr, &m_device));
    }

    // ================================================================================================================
    void Application::InitGraphicsQueue()
    {
        vkGetDeviceQueue(m_device, m_graphicsQueueFamilyIdx, 0, &m_graphicsQueue);
    }

    // ================================================================================================================
    void Application::InitVmaAllocator()
    {
        // Create the VMA
        VmaVulkanFunctions vkFuncs = {};
        {
            vkFuncs.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
            vkFuncs.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;
        }

        VmaAllocatorCreateInfo allocCreateInfo = {};
        {
            allocCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
            allocCreateInfo.physicalDevice = m_physicalDevice;
            allocCreateInfo.device = m_device;
            allocCreateInfo.instance = m_instance;
            allocCreateInfo.pVulkanFunctions = &vkFuncs;
        }
        
        vmaCreateAllocator(&allocCreateInfo, m_pAllocator);
    }

    // ================================================================================================================
    void Application::InitDescriptorPool()
    {
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

        VK_CHECK(vkCreateDescriptorPool(m_device, &pool_info, nullptr, &m_descriptorPool));
    }

    // ================================================================================================================
    void Application::InitGfxCommandPool()
    {
        // Create the command pool belongs to the graphics queue
        VkCommandPoolCreateInfo commandPoolInfo{};
        {
            commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            commandPoolInfo.queueFamilyIndex = m_graphicsQueueFamilyIdx;
        }
        VK_CHECK(vkCreateCommandPool(m_device, &commandPoolInfo, nullptr, &m_gfxCmdPool));
    }

    // ================================================================================================================
    void Application::InitGfxCommandBuffers(
        const uint32_t cmdBufCnt)
    {
        // Create the command buffers
        m_gfxCmdBufs.resize(cmdBufCnt);
        VkCommandBufferAllocateInfo commandBufferAllocInfo{};
        {
            commandBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            commandBufferAllocInfo.commandPool = m_gfxCmdPool;
            commandBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            commandBufferAllocInfo.commandBufferCount = (uint32_t)m_gfxCmdBufs.size();
        }
        VK_CHECK(vkAllocateCommandBuffers(m_device, &commandBufferAllocInfo, m_gfxCmdBufs.data()));
    }

    // ================================================================================================================
    VkShaderModule Application::CreateShaderModule(
        const std::string& spvName)
    {
        // Create  Shader Module -- SOURCE_PATH is a MACRO definition passed in during compilation, which is specified
        //                          in the CMakeLists.txt file in the same level of repository.
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
        CheckVkResult(vkCreateShaderModule(m_device, &shaderModuleCreateInfo, nullptr, &shaderModule));

        return shaderModule;
    }

    // ================================================================================================================
    std::vector<VkDeviceQueueCreateInfo> Application::CreateDeviceQueueInfos(
        const std::set<uint32_t>& uniqueQueueFamilies)
    {
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies) 
        {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        return queueCreateInfos;
    }

    // ================================================================================================================
    VkPipelineShaderStageCreateInfo Application::CreateDefaultShaderStgCreateInfo(
        const VkShaderModule& shaderModule,
        const VkShaderStageFlagBits stg)
    {
        VkPipelineShaderStageCreateInfo info{};
        {
            info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            info.pNext = nullptr;
            info.flags = 0;
            info.stage = stg;
            info.module = shaderModule;
            info.pName = "main";
            info.pSpecializationInfo = nullptr;
        }
        return info;
    }

    // ================================================================================================================
    void Application::CreateVmaVkBuffer(
        VmaMemoryUsage           vmaMemUsage,
        VmaAllocationCreateFlags vmaAllocFlags,
        VkSharingMode            sharingMode,
        VkBufferUsageFlags       bufferUsageFlag,
        VkDeviceSize             byteNum,
        VkBuffer*                pBuffer,
        VmaAllocation*           pAllocation)
    {
        VmaAllocationCreateInfo stagingBufAllocInfo{};
        {
            stagingBufAllocInfo.usage = vmaMemUsage;
            stagingBufAllocInfo.flags = vmaAllocFlags;
        }

        VkBufferCreateInfo stgBufInfo{};
        {
            stgBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            stgBufInfo.sharingMode = sharingMode;
            stgBufInfo.usage = bufferUsageFlag;
            stgBufInfo.size = byteNum;
        }

        VK_CHECK(vmaCreateBuffer(*m_pAllocator, &stgBufInfo, &stagingBufAllocInfo, pBuffer, pAllocation, nullptr));
    }

    // ================================================================================================================
    void Application::CreateVmaVkImage()
    {}

    // ================================================================================================================
    void Application::CopyRamDataToGpuBuffer(
        void*         pSrc,
        VkBuffer      dstBuffer,
        VmaAllocation dstAllocation,
        uint32_t      byteNum)
    {
        void* pBufferDataMap;
        VK_CHECK(vmaMapMemory(*m_pAllocator, dstAllocation, &pBufferDataMap));
        memcpy(pBufferDataMap, pSrc, byteNum);
        vmaUnmapMemory(*m_pAllocator, dstAllocation);
    }

    // ================================================================================================================
    void Application::SubmitCmdBufToGfxQueue(
        VkCommandBuffer cmdBuf, 
        VkFence         signalFence)
    {
        // Submit the filled command buffer to the graphics queue to draw the image
        VkSubmitInfo submitInfo{};
        {
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &cmdBuf;
        }
        vkResetFences(m_device, 1, &signalFence);
        VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, signalFence));

        // Wait for the end of all transformation and reset the command buffer. The fence would be waited in the first
        // loop.
        vkWaitForFences(m_device, 1, &signalFence, VK_TRUE, UINT64_MAX);
        vkResetCommandBuffer(cmdBuf, 0);
    }

    // ================================================================================================================
    GlfwApplication::GlfwApplication() :
        Application::Application(),
        m_currentFrame(0),
        m_surface(VK_NULL_HANDLE),
        m_swapchain(VK_NULL_HANDLE),
        m_pWindow(nullptr),
        m_presentQueueFamilyIdx(-1),
        m_choisenSurfaceFormat(),
        m_swapchainImageExtent(),
        m_presentQueue(VK_NULL_HANDLE)
    {}

    // ================================================================================================================
    GlfwApplication::~GlfwApplication()
    {
        CleanupSwapchain();

        // Cleanup syn objects
        for (auto itr : m_imageAvailableSemaphores)
        {
            vkDestroySemaphore(m_device, itr, nullptr);
        }

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
    void GlfwApplication::FrameStart()
    {
        glfwPollEvents();
    }

    // ================================================================================================================
    bool GlfwApplication::NextImgIdxOrNewSwapchain(
        uint32_t& idx)
    {
        // Get next available image from the swapchain
        VkResult result = vkAcquireNextImageKHR(m_device,
                                                m_swapchain,
                                                UINT64_MAX,
                                                m_imageAvailableSemaphores[m_currentFrame],
                                                VK_NULL_HANDLE,
                                                &idx);

        m_swapchainNextImgId = idx;

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
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &m_imageAvailableSemaphores[m_currentFrame];
            submitInfo.pWaitDstStageMask = waitStages;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &m_gfxCmdBufs[m_currentFrame];
            // This draw would let the signalSemaphore sign when it finishes
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &m_renderFinishedSemaphores[m_currentFrame];
        }
        VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]));

        // Put the swapchain into the present info and wait for the graphics queue previously before presenting.
        VkPresentInfoKHR presentInfo{};
        {
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = &m_renderFinishedSemaphores[m_currentFrame];
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = &m_swapchain;
            presentInfo.pImageIndices = &m_swapchainNextImgId;
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

    void GlfwApplication::FrameEnd()
    {
        m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

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
        m_imageAvailableSemaphores.resize(SharedLib::MAX_FRAMES_IN_FLIGHT);
        m_renderFinishedSemaphores.resize(SharedLib::MAX_FRAMES_IN_FLIGHT);
        m_inFlightFences.resize(SharedLib::MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{};
        {
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        }

        VkFenceCreateInfo fenceInfo{};
        {
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        }

        for (size_t i = 0; i < SharedLib::MAX_FRAMES_IN_FLIGHT; i++)
        {
            VK_CHECK(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]));
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
            if (curFormat.format == VK_FORMAT_B8G8R8A8_SRGB && curFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
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
            swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
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
            depthImgsInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
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

        VkExtent3D swapchainExtent{};
        {
            swapchainExtent.width = width;
            swapchainExtent.height = height;
            swapchainExtent.depth = 1;
        }
        CreateSwapchainDepthImages(swapchainExtent);
        CreateSwapchainImageViews();
    }

    // ================================================================================================================
    DearImGuiApplication::DearImGuiApplication()
        : GlfwApplication()
    {}

    // ================================================================================================================
    DearImGuiApplication::~DearImGuiApplication()
    {}
}