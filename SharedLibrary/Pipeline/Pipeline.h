#pragma once

#include <vector>
#include <vulkan/vulkan.h>

// The design philosophy of the pipeline is to set the pipeline states or infos along the way and record what we set.
// When we create the pipeline, if we find out that some infos are not fed before, we'll just use the default settings.
//
// In addition, we always use the dynamic rendering.
namespace SharedLib
{
    struct PipelineColorBlendInfo
    {
        std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
        VkPipelineColorBlendStateCreateInfo colorBlending;
    };

    struct PipelineDynamicStatesInfo
    {
        std::vector<VkDynamicState> dynamicStates;
        VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo;
    };

    class Pipeline
    {
    public:
        Pipeline();
        ~Pipeline();

        VkPipeline GetVkPipeline() { return m_pipeline; }

        void CreatePipeline(VkDevice device);

        void SetShaderStageInfo(VkPipelineShaderStageCreateInfo* shaderStgInfo, uint32_t cnt);
        void SetPNext(void* pNext) { m_pNext = pNext; }

        void SetVertexInputInfo(VkPipelineVertexInputStateCreateInfo* pVertexInputInfo)
            { m_pVertexInputInfo = pVertexInputInfo; }

        void SetPipelineLayout(VkPipelineLayout pipelineLayout) { m_pipelineLayout = pipelineLayout; }
        void SetDepthStencilStateInfo(VkPipelineDepthStencilStateCreateInfo* pDepthStencilInfo) 
            { m_pDepthStencilState = pDepthStencilInfo; }

        void SetPipelineColorBlendInfo(PipelineColorBlendInfo& pipelineColorBlendAttachmentStates); // For the MRT.

        void SetRasterizerInfo(VkPipelineRasterizationStateCreateInfo* pRasterizerInfo) { m_pRasterizer = pRasterizerInfo; }

    protected:

    private:
        VkPipelineShaderStageCreateInfo* m_pShaderStgInfos;
        uint32_t                         m_stgCnt;

        void* m_pNext;

        VkPipelineVertexInputStateCreateInfo*   m_pVertexInputInfo;
        VkPipelineInputAssemblyStateCreateInfo* m_pInputAssembly;
        VkPipelineViewportStateCreateInfo*      m_pViewportState;
        VkPipelineRasterizationStateCreateInfo* m_pRasterizer;
        VkPipelineMultisampleStateCreateInfo*   m_pMultisampling;
        PipelineColorBlendInfo*                 m_pColorBlending;
        PipelineDynamicStatesInfo*              m_pDynamicState;
        VkPipelineDepthStencilStateCreateInfo*  m_pDepthStencilState;

        bool m_isVertexInputInfoDefault;
        bool m_isRasterizationInputInfoDefault;

        void SetDefaultVertexInputInfo();
        void SetDefaultInputAssemblyInfo();
        void SetDefaultViewportStateInfo();
        void SetDefaultRasterizerInfo();
        void SetDefaultMultisamplingInfo();
        void SetDefaultColorBlendingInfo();
        void SetDefaultDynamicStateInfo();

        VkPipeline       m_pipeline;
        VkPipelineLayout m_pipelineLayout;
        VkDevice         m_device;
    };
}