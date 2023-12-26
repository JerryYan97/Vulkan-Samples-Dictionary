#include "Pipeline.h"
#include "VulkanDbgUtils.h"
#include <cassert>

namespace SharedLib
{
    Pipeline::Pipeline() :
        m_pipeline(VK_NULL_HANDLE),
        m_stgCnt(0),
        m_pShaderStgInfos(nullptr),
        m_pNext(nullptr),
        m_pVertexInputInfo(nullptr),
        m_pInputAssembly(nullptr),
        m_pViewportState(nullptr),
        m_pRasterizer(nullptr),
        m_pMultisampling(nullptr),
        m_pColorBlending(nullptr),
        m_pDynamicState(nullptr),
        m_pipelineLayout(VK_NULL_HANDLE),
        m_isVertexInputInfoDefault(false),
        m_device(VK_NULL_HANDLE),
        m_pDepthStencilState(nullptr)
    {}

    Pipeline::~Pipeline()
    {
        if ((m_pVertexInputInfo != nullptr) && m_isVertexInputInfoDefault)
        {
            delete m_pVertexInputInfo;
        }

        if (m_pInputAssembly != nullptr)
        {
            delete m_pInputAssembly;
        }

        if (m_pViewportState != nullptr)
        {
            delete m_pViewportState;
        }

        if (m_pRasterizer != nullptr)
        {
            delete m_pRasterizer;
        }

        if (m_pMultisampling != nullptr)
        {
            delete m_pMultisampling;
        }

        if (m_pColorBlending != nullptr)
        {
            delete m_pColorBlending;
        }

        if (m_pDynamicState != nullptr)
        {
            delete m_pDynamicState;
        }

        // Destroy the pipeline
        if (m_device != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(m_device, m_pipeline, nullptr);
        }
    }

    void Pipeline::SetDefaultVertexInputInfo()
    {
        m_pVertexInputInfo = new VkPipelineVertexInputStateCreateInfo();
        memset(m_pVertexInputInfo, 0, sizeof(VkPipelineVertexInputStateCreateInfo));
        m_isVertexInputInfoDefault = true;
        {
            m_pVertexInputInfo->sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            m_pVertexInputInfo->vertexBindingDescriptionCount = 0;
            m_pVertexInputInfo->vertexAttributeDescriptionCount = 0;
        }
    }

    void Pipeline::SetDefaultInputAssemblyInfo()
    {
        m_pInputAssembly = new VkPipelineInputAssemblyStateCreateInfo();
        memset(m_pInputAssembly, 0, sizeof(VkPipelineInputAssemblyStateCreateInfo));
        {
            m_pInputAssembly->sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            m_pInputAssembly->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            m_pInputAssembly->primitiveRestartEnable = VK_FALSE;
        }
    }

    void Pipeline::SetDefaultViewportStateInfo()
    {
        m_pViewportState = new VkPipelineViewportStateCreateInfo();
        memset(m_pViewportState, 0, sizeof(VkPipelineViewportStateCreateInfo));
        {
            m_pViewportState->sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            m_pViewportState->viewportCount = 1;
            m_pViewportState->scissorCount = 1;
        }
    }

    void Pipeline::SetDefaultRasterizerInfo()
    {
        m_pRasterizer = new VkPipelineRasterizationStateCreateInfo();
        memset(m_pRasterizer, 0, sizeof(VkPipelineRasterizationStateCreateInfo));
        {
            m_pRasterizer->sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            m_pRasterizer->depthClampEnable = VK_FALSE;
            m_pRasterizer->rasterizerDiscardEnable = VK_FALSE;
            m_pRasterizer->polygonMode = VK_POLYGON_MODE_FILL;
            m_pRasterizer->lineWidth = 1.0f;
            // m_pRasterizer->cullMode = VK_CULL_MODE_FRONT_BIT;
            m_pRasterizer->cullMode = VK_CULL_MODE_NONE;
            // NOTE: We reverse the depth so the faces' winding is also reversed.
            // m_pRasterizer->frontFace = VK_FRONT_FACE_CLOCKWISE;
            m_pRasterizer->frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            m_pRasterizer->depthBiasEnable = VK_FALSE;
        }
    }

    void Pipeline::SetDefaultMultisamplingInfo()
    {
        m_pMultisampling = new VkPipelineMultisampleStateCreateInfo();
        memset(m_pMultisampling, 0, sizeof(VkPipelineMultisampleStateCreateInfo));
        {
            m_pMultisampling->sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            m_pMultisampling->sampleShadingEnable = VK_FALSE;
            m_pMultisampling->rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        }
    }

    void Pipeline::SetDefaultColorBlendingInfo()
    {
        m_pColorBlending = new PipelineColorBlendInfo();
        memset(m_pColorBlending, 0, sizeof(PipelineColorBlendInfo));
        {
            VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
            colorBlendAttachmentState.colorWriteMask = 0xf;
            colorBlendAttachmentState.blendEnable = VK_FALSE;

            m_pColorBlending->colorBlendAttachments.push_back(colorBlendAttachmentState);

            m_pColorBlending->colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            m_pColorBlending->colorBlending.logicOpEnable = VK_FALSE;
            m_pColorBlending->colorBlending.logicOp = VK_LOGIC_OP_COPY;
            m_pColorBlending->colorBlending.attachmentCount = m_pColorBlending->colorBlendAttachments.size();
            m_pColorBlending->colorBlending.pAttachments = m_pColorBlending->colorBlendAttachments.data();
            m_pColorBlending->colorBlending.blendConstants[0] = 0.0f;
            m_pColorBlending->colorBlending.blendConstants[1] = 0.0f;
            m_pColorBlending->colorBlending.blendConstants[2] = 0.0f;
            m_pColorBlending->colorBlending.blendConstants[3] = 0.0f;
        }
    }

    void Pipeline::SetPipelineColorBlendInfo(
        PipelineColorBlendInfo& pipelineColorBlendAttachmentStates)
    {
        m_pColorBlending = new PipelineColorBlendInfo();
        memset(m_pColorBlending, 0, sizeof(PipelineColorBlendInfo));
        {
            *m_pColorBlending = pipelineColorBlendAttachmentStates;
            m_pColorBlending->colorBlending.attachmentCount = m_pColorBlending->colorBlendAttachments.size();
            m_pColorBlending->colorBlending.pAttachments = m_pColorBlending->colorBlendAttachments.data();
        }
    }

    void Pipeline::SetDefaultDynamicStateInfo()
    {
        m_pDynamicState = new PipelineDynamicStatesInfo();
        memset(m_pDynamicState, 0, sizeof(PipelineDynamicStatesInfo));
        {
            m_pDynamicState->dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
            m_pDynamicState->dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);

            m_pDynamicState->dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            m_pDynamicState->dynamicStateCreateInfo.dynamicStateCount = 
                static_cast<uint32_t>(m_pDynamicState->dynamicStates.size());
            m_pDynamicState->dynamicStateCreateInfo.pDynamicStates = m_pDynamicState->dynamicStates.data();
        }
    }

    void Pipeline::CreatePipeline(
        VkDevice device)
    {
        assert(m_stgCnt != 0, "Pipeline must has shader modules!");

        m_device = device;

        if (m_pVertexInputInfo == nullptr)
        {
            SetDefaultVertexInputInfo();
        }

        if (m_pInputAssembly == nullptr)
        {
            SetDefaultInputAssemblyInfo();
        }

        if (m_pViewportState == nullptr)
        {
            SetDefaultViewportStateInfo();
        }

        if (m_pRasterizer == nullptr)
        {
            SetDefaultRasterizerInfo();
        }

        if (m_pMultisampling == nullptr)
        {
            SetDefaultMultisamplingInfo();
        }

        if (m_pColorBlending == nullptr)
        {
            SetDefaultColorBlendingInfo();
        }

        if (m_pDynamicState == nullptr)
        {
            SetDefaultDynamicStateInfo();
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        {
            pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipelineInfo.pNext = m_pNext;
            pipelineInfo.stageCount = m_stgCnt;
            pipelineInfo.pStages = m_pShaderStgInfos;
            pipelineInfo.pVertexInputState = m_pVertexInputInfo;
            pipelineInfo.pInputAssemblyState = m_pInputAssembly;
            pipelineInfo.pViewportState = m_pViewportState;
            pipelineInfo.pRasterizationState = m_pRasterizer;
            pipelineInfo.pMultisampleState = m_pMultisampling;
            pipelineInfo.pColorBlendState = &(m_pColorBlending->colorBlending);
            pipelineInfo.pDynamicState = &(m_pDynamicState->dynamicStateCreateInfo);
            pipelineInfo.layout = m_pipelineLayout;
            pipelineInfo.renderPass = nullptr;
            pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
            pipelineInfo.pDepthStencilState = m_pDepthStencilState;
        }

        VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline));
    }

    void Pipeline::SetShaderStageInfo(
        VkPipelineShaderStageCreateInfo* shaderStgInfo,
        uint32_t                         cnt)
    {
        m_stgCnt = cnt;
        m_pShaderStgInfos = shaderStgInfo;
    }
}