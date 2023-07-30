#include "Pipeline.h"
#include "VulkanDbgUtils.h"
#include <cassert>

namespace SharedLib
{
    Pipeline::Pipeline() :
        m_isVsPsShaderMoodulesSet(false),
        m_stgCnt(0),
        m_shaderStgInfo(nullptr)
    {}

    Pipeline::~Pipeline()
    {}

    void Pipeline::CreatePipeline()
    {
        assert(m_isVsPsShaderMoodulesSet, "Pipeline must has shader modules!");
    }

    void Pipeline::SetShaderStageInfo(
        VkPipelineShaderStageCreateInfo* shaderStgInfo,
        uint32_t                         cnt)
    {
        m_isVsPsShaderMoodulesSet = true;
        m_stgCnt = cnt;
        m_shaderStgInfo = shaderStgInfo;
    }
}