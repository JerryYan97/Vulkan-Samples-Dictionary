#pragma once

#include <vulkan/vulkan.h>
#include <fstream>
#include <vector>
#include <set>

VK_DEFINE_HANDLE(VmaAllocator)
VK_DEFINE_HANDLE(VmaAllocation)

enum VmaMemoryUsage;
typedef VkFlags VmaAllocationCreateFlags;

// The design philosophy of the SharedLib is to reuse code as much as possible and writing new code in examples as less as possible.
// This would lead to:
// - Small granular functions and versatile input arguments.
// - Speed is not a problem. The key is easy to use. So, I should feel free to use all kinds of data structure.
// - All CmdBuffer operations need to stay in the main.cpp.
// - In principle, all member variables should be init in InitXXX(...) and destroied in the destructor. Or, the variable
//   shouldn't be in the class, but can be created by the public interface.
// - Sync, CmdBuffer operations should be explicit in the main.cpp. -- No... The code reuse maybe really bad if we do this.
// TODO2: Dictionary Vma/VkBuffer/VkImage Management -- Need explicit create/destroy; return internal ids; element: id - {alloc, vkBuffer}.
// TODO3: GPU image format should have more information like currnet GPU image format.
namespace SharedLib
{
    typedef std::pair<VkDescriptorType, void*> PushDescriptorInfo;

    struct GpuBuffer
    {
        VkBuffer               buffer;
        VmaAllocation          bufferAlloc;
        VkDescriptorBufferInfo bufferDescInfo;
        VkDescriptorType       gpuBufferDescriptorType; // Uniform buffer or storage buffer?
    };

    struct GpuImg
    {
        VkImage               image;
        VmaAllocation         imageAllocation;
        VkDescriptorImageInfo imageDescInfo;
        VkImageView           imageView;
        VkSampler             imageSampler;
    };

    struct GpuImgCreateInfo
    {
        // VkImageCreateInfo        imgInfo;
        VmaAllocationCreateFlags allocFlags;
        VkImageSubresourceRange  imgSubresRange;
        VkImageViewType          imgViewType;
        VkFormat                 imgFormat;
        VkExtent3D               imgExtent;
        VkImageUsageFlags        imgUsageFlags;
        VkImageCreateFlags       imgCreateFlags; // For the cubemap image.

        bool                hasSampler;
        VkSamplerCreateInfo samplerInfo;
    };

    // Used as a temporary command buffer in a function.
    class RAIICommandBuffer
    {
    public:
        RAIICommandBuffer(VkCommandPool cmdPool, VkDevice device);
        ~RAIICommandBuffer();

        VkCommandBuffer m_cmdBuffer;

    private:
        VkDevice m_device;
        VkCommandPool m_cmdPool;
    };

    class RAIIHeapRAM
    {
    public:
        RAIIHeapRAM(void* pData, bool isArray) : m_pData(pData), m_isArray(isArray) {}
        ~RAIIHeapRAM() { if (m_isArray) { delete[] m_pData; } else { delete m_pData; } }

    private:
        void* m_pData;
        bool m_isArray;
    };

    // Base Vulkan application without a swapchain -- Basically abstract.
    // It has vmaAllocator and descriptor pool. Besides, it also provides basic vulkan objects creation functions.
    class Application
    {
    public:
        Application();
        ~Application();

        void GpuWaitForIdle();

        // The InitXXX(...) should be called in it. It is expected to called after the constructor.
        virtual void AppInit() = 0;

        void CreateVmaVkBuffer(VmaMemoryUsage           vmaMemUsage, 
                               VmaAllocationCreateFlags vmaAllocFlags, 
                               VkSharingMode            sharingMode,
                               VkBufferUsageFlags       bufferUsageFlag,
                               VkDeviceSize             byteNum,
                               VkBuffer*                pBuffer,
                               VmaAllocation*           pAllocation);

        void CopyRamDataToGpuBuffer(void*         pSrc,
                                    VkBuffer      dstBuffer, 
                                    VmaAllocation dstAllocation,
                                    uint32_t      byteNum);

        void CmdImgLayoutTrans(VkCommandBuffer      cmdBuffer,
                               VkImage              img,
                               VkImageLayout        oldLayout,
                               VkImageLayout        newLayout,
                               VkAccessFlags        srcAccessMask,
                               VkAccessFlags        dstAccessMask,
                               VkPipelineStageFlags srcStageMask,
                               VkPipelineStageFlags dstStageMask);

        void CmdClearImg(VkCommandBuffer cmdBuffer, VkImage img);

        void SubmitCmdBufToGfxQueue(VkCommandBuffer cmdBuf, VkFence signalFence);

        VkFence CreateFence();
        void WaitAndDestroyTheFence(VkFence fence);
        void WaitTheFence(VkFence fence);

        GpuImg CreateGpuImage(GpuImgCreateInfo createInfo);
        void DestroyGpuImgResource(const GpuImg& gpuImg);

        GpuBuffer CreateGpuBuffer(VkBufferUsageFlags usage, VmaAllocationCreateFlags vmaFlags, uint32_t bytesNum);
        void DestroyGpuBufferResource(const GpuBuffer& pGpuBuffer);

        GpuImg CreateDummyPureColorImg(float* pColor);
        void TransImageLayout(GpuImg* pTargetImg,
            VkImageLayout targetLayout,
            VkAccessFlags srcAccess,
            VkAccessFlags dstAccess,
            VkPipelineStageFlags srcPipelineStg,
            VkPipelineStageFlags dstPipelineStg) {}

        VkImageSubresourceRange GetImgSubrsrcRange(uint32_t baseMipLevel, uint32_t levelCnt, uint32_t baseArrayLayer, uint32_t layerCnt, VkImageAspectFlags imgAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT);
        VkImageSubresource GetImgSubrsrc(uint32_t mipLevel, uint32_t baseArrayLayer, VkImageAspectFlags imgAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT);
        VkImageSubresourceLayers GetImgSubrsrcLayers(uint32_t mipLevel, uint32_t baseArrayLayer, uint32_t layerCnt, VkImageAspectFlags imgAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT);

        // GpuImg CreateSwapchainSizeOneMipOneLayerGpuImg(usage, ...);
        // void DestroyGpuImg(GpuImg xxx);

        VmaAllocator* GetVmaAllocator() { return m_pAllocator; }
        VkCommandBuffer GetGfxCmdBuffer(uint32_t i) { return m_gfxCmdBufs[i]; }
        VkDevice GetVkDevice() { return m_device; }
        VkQueue GetGfxQueue() { return m_graphicsQueue; }
        VkCommandPool GetGfxCmdPool() { return m_gfxCmdPool; }

        // The push descriptor update function is part of an extension so it has to be manually loaded
        PFN_vkCmdPushDescriptorSetKHR m_vkCmdPushDescriptorSetKHR;

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
                        const std::vector<VkDeviceQueueCreateInfo>& queueCreateInfos,
                        void*                                       pNext);

        void InitKHRFuncPtrs();

        void InitGraphicsQueue();
        void InitVmaAllocator();
        void InitGfxCommandPool();
        void InitGfxCommandBuffers(const uint32_t cmdBufCnt);

        // CreateXXX(...) functions are more flexible. They are utility functions for children classes.
        // CreateXXX(...) cannot initialize any member objects. They have to return objects.
        VkShaderModule                       CreateShaderModule(const std::string& spvName);
        VkShaderModule                       CreateShaderModuleFromRam(uint32_t* pCode, uint32_t codeSizeInBytes);
        std::vector<VkDeviceQueueCreateInfo> CreateDeviceQueueInfos(const std::set<uint32_t>& uniqueQueueFamilies);
        
        VkPipelineShaderStageCreateInfo CreateDefaultShaderStgCreateInfo(const VkShaderModule& shaderModule, const VkShaderStageFlagBits stg);

        VkSamplerCreateInfo GetSamplerInfo(VkFilter filter, VkSamplerAddressMode addrMode);

        void CmdAutoPushDescriptors(const std::vector<PushDescriptorInfo> pushDescriptors);

        // The class manages both of the creation and destruction of the objects below.
        VkInstance       m_instance;
        VkPhysicalDevice m_physicalDevice;
        VkDevice         m_device;
        unsigned int     m_graphicsQueueFamilyIdx;
        VkQueue          m_graphicsQueue;
        VkCommandPool    m_gfxCmdPool;
        
        VkDebugUtilsMessengerEXT     m_debugMessenger;
        VmaAllocator*                m_pAllocator;
        std::vector<VkCommandBuffer> m_gfxCmdBufs;

        std::vector<void*> m_heapMemPtrVec; // Manage heap memory -- Auto delete at the end.
        std::vector<void*> m_heapArrayMemPtrVec;

        std::vector<VkImageMemoryBarrier> m_imgTransBarriers; // A queue to collect all barriers to run at one time to simpliy coding.
    };
}