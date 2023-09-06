#pragma once
#include "../Pipeline/Pipeline.h"
#include "vulkan/vulkan.h"
#include "vk_mem_alloc.h"
#include <string>

namespace SharedLib
{
    struct VulkanInfos
    {
        VkDevice device;
        VkDescriptorPool descriptorPool;
        VmaAllocator* pAllocator;
    };

    class AppUtil
    {
    public:
        AppUtil();
        ~AppUtil() {};

        void GetVkInfos(VulkanInfos infos) { m_vkInfos = infos; }

        VkShaderModule CreateShaderModule(const std::string& spvName);
        
        virtual void Init() = 0;
        virtual void Destroy() = 0; // All resources should be released in this function instead of the destructor.

    protected:
        VulkanInfos m_vkInfos;
        Pipeline*   m_pPipeline;

    private:
    };

    class CubemapFormatTransApp : public AppUtil
    {
    public:
        CubemapFormatTransApp();
        ~CubemapFormatTransApp() {};

        virtual void Init() override;
        virtual void Destroy() override;

        void SetInputCubemapImg(VkImage iCubemapImg, VkExtent3D extent);

        void CmdConvertCubemapFormat(VkCommandBuffer cmdBuffer);

    private:
        void InitFormatPipeline();
        void InitFormatPipelineDescriptorSetLayout();
        void InitFormatPipelineLayout();
        void InitFormatShaderModules();
        void InitFormatPipelineDescriptorSet();

        void InitFormatImgsObjects();
        void DestroyFormatImgsObjects();
        void InitWidthHeightBufferInfo();

        VkShaderModule        m_vsFormatShaderModule;
        VkShaderModule        m_psFormatShaderModule;
        VkDescriptorSetLayout m_formatPipelineDesSet0Layout;
        VkPipelineLayout      m_formatPipelineLayout;

        VkDescriptorSet m_formatPipelineDescriptorSet0;

        std::vector<VkImage>       m_formatInputImages;
        std::vector<VkImageView>   m_formatInputImagesViews;
        std::vector<VmaAllocation> m_formatInputImagesAllocs;
        std::vector<VkSampler>     m_formatInputImagesSamplers;

        VkBuffer      m_formatWidthHeightBuffer;
        VmaAllocation m_formatWidthHeightAlloc;

        VkImage    m_inputCubemap;
        VkExtent3D m_inputCubemapExtent;

        VkImage       m_outputCubemap;
        VkImageView   m_outputCubemapImgView;
        VmaAllocation m_outputCubemapAlloc;
    };
}