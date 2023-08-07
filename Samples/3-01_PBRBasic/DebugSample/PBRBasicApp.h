#pragma once
#include "../../3-00_SharedLibrary/Application.h"
#include "../../3-00_SharedLibrary/Pipeline.h"

VK_DEFINE_HANDLE(VmaAllocation);

namespace SharedLib
{
    class Camera;
}

class PBRBasicApp : public SharedLib::GlfwApplication
{
public:
    PBRBasicApp();
    ~PBRBasicApp();

    virtual void AppInit() override;

    VkFence GetFence(uint32_t i) { return m_inFlightFences[i]; }

    VkPipelineLayout GetPipelineLayout() { return m_pipelineLayout; }

    VkDescriptorSet GetCurrentFrameDescriptorSet0() 
        { return m_pipelineDescriptorSet0s[m_currentFrame]; }

    VkPipeline GetPipeline() { return m_pipeline.GetVkPipeline(); }

    uint32_t GetIdxCnt() { return m_idxData.size(); }

    VkBuffer GetIdxBuffer() { return m_idxBuffer; }
    VkBuffer GetVertBuffer() { return m_vertBuffer; }

private:
    void InitPipeline();
    void InitPipelineDescriptorSetLayout();
    void InitPipelineLayout();
    void InitShaderModules();
    void InitPipelineDescriptorSets();
    
    void InitSphereVertexIndexBuffers(); // Read in sphere data, create Sphere's GPU buffer objects and transfer data 
                                         // to the GPU buffers.
    void ReadInSphereData();
    void DestroySphereVertexIndexBuffers();

    void InitVpUboObjects(); // Create MVP matrices's GPU buffer objects and transfer data to the GPU buffers.
    void DestroyVpUboObjects();

    void InitFragUboObjects();
    void DestroyFragUboObjects();

    VkPipelineVertexInputStateCreateInfo CreatePipelineVertexInputInfo();
    VkPipelineDepthStencilStateCreateInfo CreateDepthStencilStateInfo();

    SharedLib::Camera* m_pCamera;
    VkBuffer           m_vpUboBuffer;
    VmaAllocation      m_vpUboAlloc;

    VkBuffer      m_lightPosBuffer;
    VmaAllocation m_lightPosBufferAlloc;

    std::vector<VkDescriptorSet> m_pipelineDescriptorSet0s;

    uint32_t  m_vertBufferByteCnt;
    uint32_t  m_idxBufferByteCnt;

    std::vector<float> m_vertData;
    std::vector<uint32_t> m_idxData;

    VkBuffer      m_vertBuffer;
    VmaAllocation m_vertBufferAlloc;
    VkBuffer      m_idxBuffer;
    VmaAllocation m_idxBufferAlloc;

    VkShaderModule        m_vsShaderModule;
    VkShaderModule        m_psShaderModule;
    VkDescriptorSetLayout m_pipelineDesSetLayout;
    VkPipelineLayout      m_pipelineLayout;
    SharedLib::Pipeline   m_pipeline;
};
