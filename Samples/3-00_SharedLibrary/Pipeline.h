#pragma once

#include <vulkan/vulkan.h>

// The design philosophy of the pipeline is to set the pipeline states or infos along the way and record what we set.
// When we create the pipeline, if we find out that some infos are not fed before, we'll just use the default settings.
namespace SharedLib
{
    class Pipeline
    {
    public:
        Pipeline();
        ~Pipeline();

        VkPipeline GetVkPipeline() { return m_pipeline; }

        void CreatePipeline();

        void SetShaderStageInfo(VkPipelineShaderStageCreateInfo* shaderStgInfo, uint32_t cnt);

    protected:

    private:
        
        VkPipelineShaderStageCreateInfo* m_shaderStgInfo;
        uint32_t                         m_stgCnt;
        bool                             m_isVsPsShaderMoodulesSet; // It must be set before calling the CreatePipeline().


        VkPipeline m_pipeline;
    };
}