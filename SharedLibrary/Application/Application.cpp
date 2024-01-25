#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "CmdBufUtils.h"
#include "Application.h"
#include "VulkanDbgUtils.h"
#include "AppUtils.h"
#include <cassert>

namespace SharedLib
{
    // ================================================================================================================
    Application::Application() :
        m_graphicsQueueFamilyIdx(-1),
        m_instance(VK_NULL_HANDLE),
        m_physicalDevice(VK_NULL_HANDLE),
        m_device(VK_NULL_HANDLE),
        m_graphicsQueue(VK_NULL_HANDLE),
        m_pAllocator(nullptr),
        m_debugMessenger(VK_NULL_HANDLE),
        m_gfxCmdPool(VK_NULL_HANDLE),
        m_vkCmdPushDescriptorSetKHR(nullptr)
    {
        m_pAllocator = new VmaAllocator();
    }

    // ================================================================================================================
    Application::~Application()
    {
        // Destroy the command pool
        vkDestroyCommandPool(m_device, m_gfxCmdPool, nullptr);

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

        // SharedLib::PrintDeviceImageCapbility(m_physicalDevice);
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
    // Enable possible extensions all at once: dynamic rendering, swapchain and push descriptors.
    void Application::InitDevice(
        const std::vector<const char*>&             deviceExts,
        const std::vector<VkDeviceQueueCreateInfo>& queueCreateInfos,
        void*                                       pNext)
    {
        VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeature{};
        {
            dynamicRenderingFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
            dynamicRenderingFeature.pNext = pNext;
            dynamicRenderingFeature.dynamicRendering = VK_TRUE;
        }

        std::vector<const char*> allDeviceExtensions = deviceExts;
        allDeviceExtensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
        allDeviceExtensions.push_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);

        // Assembly the info into the device create info
        VkDeviceCreateInfo deviceInfo{};
        {
            deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            deviceInfo.pNext = &dynamicRenderingFeature;
            deviceInfo.queueCreateInfoCount = uint32_t(queueCreateInfos.size());
            deviceInfo.pQueueCreateInfos = queueCreateInfos.data();
            deviceInfo.enabledExtensionCount = allDeviceExtensions.size();
            deviceInfo.ppEnabledExtensionNames = allDeviceExtensions.data();
        }

        // Create the logical device
        VK_CHECK(vkCreateDevice(m_physicalDevice, &deviceInfo, nullptr, &m_device));
    }

    // ================================================================================================================
    void Application::InitKHRFuncPtrs()
    {
        // Get necessary function pointers
        m_vkCmdPushDescriptorSetKHR = (PFN_vkCmdPushDescriptorSetKHR)vkGetDeviceProcAddr(m_device, "vkCmdPushDescriptorSetKHR");
        if (!m_vkCmdPushDescriptorSetKHR) {
            exit(1);
        }
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
    VkShaderModule Application::CreateShaderModuleFromRam(
        uint32_t* pCode,
        uint32_t codeSizeInBytes)
    {
        VkShaderModuleCreateInfo shaderModuleCreateInfo{};
        {
            shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            shaderModuleCreateInfo.codeSize = codeSizeInBytes;
            shaderModuleCreateInfo.pCode = pCode;
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
    void Application::CmdClearImg(
        VkCommandBuffer cmdBuffer,
        VkImage         img)
    {
        VkClearColorValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };

        VkImageSubresourceRange subResRange{};
        {
            subResRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            subResRange.baseMipLevel = 0;
            subResRange.levelCount = 1;
            subResRange.baseArrayLayer = 0;
            subResRange.layerCount = 1;
        }

        vkCmdClearColorImage(cmdBuffer,
                             img,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             &clearColor,
                             1, &subResRange);
    }
    
    // ================================================================================================================
    void Application::CmdImgLayoutTrans(
        VkCommandBuffer      cmdBuffer,
        VkImage              img,
        VkImageLayout        oldLayout,
        VkImageLayout        newLayout,
        VkAccessFlags        srcAccessMask,
        VkAccessFlags        dstAccessMask,
        VkPipelineStageFlags srcStageMask,
        VkPipelineStageFlags dstStageMask)
    {
        VkImageSubresourceRange subResRange{};
        {
            subResRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            subResRange.baseMipLevel = 0;
            subResRange.levelCount = 1;
            subResRange.baseArrayLayer = 0;
            subResRange.layerCount = 1;
        }

        // Transform the layout of the swapchain from undefined to render target.
        VkImageMemoryBarrier renderTargetTransBarrier{};
        {
            renderTargetTransBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            renderTargetTransBarrier.srcAccessMask = srcAccessMask;
            renderTargetTransBarrier.dstAccessMask = dstAccessMask;
            renderTargetTransBarrier.oldLayout = oldLayout;
            renderTargetTransBarrier.newLayout = newLayout;
            renderTargetTransBarrier.image = img;
            renderTargetTransBarrier.subresourceRange = subResRange;
        }

        vkCmdPipelineBarrier(
            cmdBuffer,
            srcStageMask,
            dstStageMask,
            0,
            0, nullptr,
            0, nullptr,
            1, &renderTargetTransBarrier);
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
    GpuImg Application::CreateGpuImage(
        GpuImgCreateInfo createInfo)
    {
        VmaAllocationCreateInfo imgAllocInfo = {};
        {
            imgAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            imgAllocInfo.flags = createInfo.allocFlags;
        }

        // Create VkImage
        GpuImg gpuImgRes;

        VkImageCreateInfo imgInfo{};
        {
            imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imgInfo.imageType = VK_IMAGE_TYPE_2D;
            imgInfo.format = createInfo.imgFormat;
            imgInfo.extent = createInfo.imgExtent;
            imgInfo.mipLevels = createInfo.imgSubresRange.levelCount;
            imgInfo.arrayLayers = createInfo.imgSubresRange.layerCount;
            imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imgInfo.usage = createInfo.imgUsageFlags;
            imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imgInfo.flags = createInfo.imgCreateFlags;
        }

        VK_CHECK(vmaCreateImage(*m_pAllocator,
            &imgInfo,
            &imgAllocInfo,
            &(gpuImgRes.image),
            &(gpuImgRes.imageAllocation),
            nullptr));

        // Create VkImageView -- Currently, we only assume a 2D image. We may need a cubemap or a 3D image.
        VkImageViewCreateInfo imgViewInfo{};
        {
            imgViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            imgViewInfo.image = gpuImgRes.image;
            imgViewInfo.viewType = createInfo.imgViewType;
            imgViewInfo.format = createInfo.imgFormat;
            imgViewInfo.subresourceRange = createInfo.imgSubresRange;
        }

        VK_CHECK(vkCreateImageView(
            m_device,
            &imgViewInfo,
            nullptr,
            &(gpuImgRes.imageView)));

        if (createInfo.hasSampler)
        {
            VK_CHECK(vkCreateSampler(m_device, &createInfo.samplerInfo, nullptr, &(gpuImgRes.imageSampler)));
        }

        gpuImgRes.imageDescInfo.sampler = gpuImgRes.imageSampler;
        gpuImgRes.imageDescInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        gpuImgRes.imageDescInfo.imageView = gpuImgRes.imageView;

        return gpuImgRes;
    }

    // ================================================================================================================
    GpuImg Application::CreateDummyPureColorImg(
        float* pColor)
    {
        GpuImg gpuImgRes;

        VmaAllocationCreateInfo imgAllocInfo = {};
        {
            imgAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            imgAllocInfo.flags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }

        VkImageSubresourceRange imgSubRsrcRange{};
        {
            imgSubRsrcRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imgSubRsrcRange.baseArrayLayer = 0;
            imgSubRsrcRange.layerCount = 1;
            imgSubRsrcRange.baseMipLevel = 0;
            imgSubRsrcRange.levelCount = 1;
        }

        VkSamplerCreateInfo samplerInfo{};
        {
            samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter = VK_FILTER_LINEAR;
            samplerInfo.minFilter = VK_FILTER_LINEAR;
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.minLod = -1000;
            samplerInfo.maxLod = 1000;
            samplerInfo.maxAnisotropy = 1.0f;
        }

        GpuImgCreateInfo gpuImgCreateInfo{};
        {
            gpuImgCreateInfo.allocFlags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
            gpuImgCreateInfo.hasSampler = true;
            gpuImgCreateInfo.imgSubresRange = imgSubRsrcRange;
            gpuImgCreateInfo.imgUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            gpuImgCreateInfo.imgViewType = VK_IMAGE_VIEW_TYPE_2D;
            gpuImgCreateInfo.samplerInfo = samplerInfo;
            gpuImgCreateInfo.imgExtent = VkExtent3D{ 1, 1, 1 };
            gpuImgCreateInfo.imgFormat = VK_FORMAT_R8G8B8A8_UNORM;
        }

        gpuImgRes = CreateGpuImage(gpuImgCreateInfo);

        // TODO: We may want to put it as a static/const member of the class so that we can reuse it.
        VkBufferImageCopy bufToImgCopyTemplate{};
        {
            VkExtent3D extent{};
            {
                extent.width = 1;
                extent.height = 1;
                extent.depth = 1;
            }

            bufToImgCopyTemplate.bufferRowLength = 1;
            bufToImgCopyTemplate.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bufToImgCopyTemplate.imageSubresource.mipLevel = 0;
            bufToImgCopyTemplate.imageSubresource.baseArrayLayer = 0;
            bufToImgCopyTemplate.imageSubresource.layerCount = 1;
            bufToImgCopyTemplate.imageExtent = extent;
        }

        RAIICommandBuffer raiiCmdBuffer(m_gfxCmdPool, m_device);

        uint8_t colorData[4] = {pColor[0] * 255, pColor[1] * 255, pColor[2] * 255, 255};
        SendImgDataToGpu(raiiCmdBuffer.m_cmdBuffer,
                         m_device,
                         m_graphicsQueue,
                         colorData, sizeof(colorData),
                         gpuImgRes.image,
                         imgSubRsrcRange,
                         VK_IMAGE_LAYOUT_UNDEFINED,
                         bufToImgCopyTemplate, *m_pAllocator);

        return gpuImgRes;
    }

    // ================================================================================================================
    VkFence Application::CreateFence()
    {
        VkFence fence;

        VkFenceCreateInfo fenceInfo{};
        {
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        }
        VK_CHECK(vkCreateFence(m_device, &fenceInfo, nullptr, &fence));
        VK_CHECK(vkResetFences(m_device, 1, &fence));

        return fence;
    }

    // ================================================================================================================
    void Application::WaitAndDestroyTheFence(
        VkFence fence)
    {
        VK_CHECK(vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX));
        vkDestroyFence(m_device, fence, nullptr);
    }

    // ================================================================================================================
    void Application::WaitTheFence(
        VkFence fence)
    {
        VK_CHECK(vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX));
    }

    // ================================================================================================================
    RAIICommandBuffer::RAIICommandBuffer(
        VkCommandPool cmdPool,
        VkDevice      device) :
        m_device(device),
        m_cmdPool(cmdPool)
    {
        VkCommandBufferAllocateInfo commandBufferAllocInfo{};
        {
            commandBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            commandBufferAllocInfo.commandPool = cmdPool;
            commandBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            commandBufferAllocInfo.commandBufferCount = 1;
        }
        VK_CHECK(vkAllocateCommandBuffers(device, &commandBufferAllocInfo, &m_cmdBuffer));
    }

    // ================================================================================================================
    RAIICommandBuffer::~RAIICommandBuffer()
    {
        vkFreeCommandBuffers(m_device, m_cmdPool, 1, &m_cmdBuffer);
    }
}