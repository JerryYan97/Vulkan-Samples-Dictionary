#pragma once
#include "../Pipeline/Pipeline.h"
#include "vulkan/vulkan.h"
#include "vk_mem_alloc.h"
#include <string>

namespace SharedLib
{
    // Assume that each channel is a float.
    // The input image's layout should be VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL.
    void CopyImgToRam(VkCommandBuffer          cmdBuffer,
                      VkDevice                 device,
                      VkQueue                  gfxQueue,
                      VmaAllocator             allocator,
                      VkImage                  srcImg,
                      VkImageSubresourceLayers srcImgSubres,
                      VkExtent3D               srcImgExtent,
                      uint32_t                 srcImgChannelCnt,
                      uint32_t                 srcImgChannelByteCnt,
                      void*                    pDst);

    void Img4EleTo3Ele(float* pSrc, float* pDst, uint32_t pixCnt);
    void Img3EleTo4Ele(float* pSrc, float* pDst, uint32_t pixCnt);

    void PrintDeviceImageCapbility(VkPhysicalDevice phyDevice);

    void CopyRamDataToGpuBuffer(void*         pSrc,
                                VmaAllocator* pAllocator,
                                VkBuffer      dstBuffer,
                                VmaAllocation dstAllocation,
                                uint32_t      byteNum);

    struct VulkanInfos
    {
        VkDevice      device; // NOTE: The push descriptors feature has to be enabled.
        VmaAllocator* pAllocator;
        VkCommandPool gfxCmdPool;
        VkQueue       gfxQueue;
    };

    class AppUtil
    {
    public:
        AppUtil();
        ~AppUtil() {};

        void GetVkInfos(VulkanInfos infos) { m_vkInfos = infos; }

        VkShaderModule CreateShaderModule(const std::string& spvName);
        VkShaderModule CreateShaderModuleFromRam(uint32_t* pCode, uint32_t codeSizeInBytes);
        
        virtual void Init() = 0;
        virtual void Destroy() = 0; // All resources should be released in this function instead of the destructor.

    protected:
        VulkanInfos m_vkInfos;
        Pipeline*   m_pPipeline;

    private:
    };

    // NOTE: Nvidia doesn't support 6 image layers for the R32G32B32A32 linear tiling, so we use the optimal tiling.
    class CubemapFormatTransApp : public AppUtil
    {
    public:
        CubemapFormatTransApp();
        ~CubemapFormatTransApp() {};

        virtual void Init() override;
        virtual void Destroy() override;

        // Only copy one mipmap at a time, which is specified in the mip base level.
        void SetInputCubemapImg(VkImage iCubemapImg, VkExtent3D extent, VkImageSubresourceRange iSubres);
        VkImage GetOutputCubemap() { return m_outputCubemap; }

        void CmdConvertCubemapFormat(VkCommandBuffer cmdBuffer);

        void DumpOutputCubemapToDisk(const std::string& outputCubemapPathName);

    private:
        void InitFormatPipeline();
        void InitFormatPipelineDescriptorSetLayout();
        void InitFormatPipelineLayout();
        void InitFormatShaderModules();

        void InitFormatImgsObjects();
        void DestroyFormatImgsObjects();
        void InitWidthHeightBufferInfo();

        VkShaderModule        m_vsFormatShaderModule;
        VkShaderModule        m_psFormatShaderModule;
        VkDescriptorSetLayout m_formatPipelineDesSet0Layout;
        VkPipelineLayout      m_formatPipelineLayout;

        // VkDescriptorSet m_formatPipelineDescriptorSet0;
        std::vector<VkWriteDescriptorSet> m_descriptorSet0Writes;

        // Copy the input cubemap to an image array with 6 images as the inputs of the pipeline.
        // 6 layers to accomodate that hlsl doesn't have separate descriptors.
        VkImage               m_formatInputImages;
        VkImageView           m_formatInputImagesViews;
        VmaAllocation         m_formatInputImagesAllocs;
        VkSampler             m_formatInputImagesSamplers;
        VkDescriptorImageInfo m_formatInputImageInfo;

        VkBuffer               m_formatWidthHeightBuffer;
        VmaAllocation          m_formatWidthHeightAlloc;
        VkDescriptorBufferInfo m_formatWidthHeightDesBufferInfo;

        VkImage                 m_inputCubemap;
        VkExtent3D              m_inputCubemapExtent;
        VkImageSubresourceRange m_inputSubres;

        VkImage       m_outputCubemap;
        VkImageView   m_outputCubemapImgView;
        VmaAllocation m_outputCubemapAlloc;

        // The push descriptor update function is part of an extension so it has to be manually loaded
        PFN_vkCmdPushDescriptorSetKHR m_pfnVkCmdPushDescriptorSetKHR;
    };
}