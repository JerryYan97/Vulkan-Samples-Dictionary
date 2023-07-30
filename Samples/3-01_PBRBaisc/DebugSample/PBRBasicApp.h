#pragma once
#include "../../3-00_SharedLibrary/Application.h"

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

    VkPipeline GetPipeline() { return m_pipeline; }

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

    void InitCameraUboObjects(); // Create camera's GPU buffer objects and transfer data to the GPU buffers.
    void DestroyCameraUboObjects();

    SharedLib::Camera*           m_pCamera;
    std::vector<VkBuffer>        m_cameraParaBuffers;
    std::vector<VmaAllocation>   m_cameraParaBufferAllocs;

    std::vector<VkDescriptorSet> m_pipelineDescriptorSet0s;

    float*    m_pVertData;
    uint32_t* m_pIdxData;

    VkShaderModule        m_vsShaderModule;
    VkShaderModule        m_psShaderModule;
    VkDescriptorSetLayout m_pipelineDesSet0Layout;
    VkPipelineLayout      m_pipelineLayout;
    VkPipeline            m_pipeline;
};
