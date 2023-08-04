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

    void UpdateCameraAndGpuBuffer();

    VkFence GetFence(uint32_t i) { return m_inFlightFences[i]; }

    VkPipelineLayout GetPipelineLayout() { return m_pipelineLayout; }

    VkDescriptorSet GetCurrentFrameDescriptorSet0() 
        { return m_pipelineDescriptorSet0s[m_currentFrame]; }

    VkPipeline GetPipeline() { return m_pipeline.GetVkPipeline(); }

    uint32_t GetIdxCnt() { return m_idxCnt; }

    // VkBuffer GetIdxBuffer() { return m_idx}

    void GetCameraData(float* pBuffer);

    void SendCameraDataToBuffer(uint32_t i);

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

    void InitMvpUboObjects(); // Create MVP matrices's GPU buffer objects and transfer data to the GPU buffers.
    void DestroyMvpUboObjects();

    void InitLightsUboObjects();
    void DestroyLightsUboObjects();

    VkPipelineVertexInputStateCreateInfo CreatePipelineVertexInputInfo();
    VkPipelineDepthStencilStateCreateInfo CreateDepthStencilStateInfo();

    SharedLib::Camera* m_pCamera;
    VkBuffer           m_mvpUboBuffer;
    VmaAllocation      m_mvpUboAlloc;

    VkBuffer      m_lightPosBuffer;
    VmaAllocation m_lightPosBufferAlloc;

    std::vector<VkDescriptorSet> m_pipelineDescriptorSet0s;

    float*    m_pVertData;
    uint32_t  m_vertBufferByteCnt;
    uint32_t* m_pIdxData;
    uint32_t  m_idxCnt;
    uint32_t  m_idxBufferByteCnt;

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
