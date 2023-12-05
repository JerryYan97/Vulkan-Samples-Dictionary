#pragma once
#include "../../../SharedLibrary/Application/GlfwApplication.h"
#include "../../../SharedLibrary/Pipeline/Pipeline.h"


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

    // The first layer is the desciptor ids.
    // The second layer is the bindings writes to that descriptor.
    std::vector<VkWriteDescriptorSet> GetWriteDescriptorSets();

    VkPipeline GetPipeline() { return m_pipeline.GetVkPipeline(); }

    uint32_t GetIdxCnt() { return m_idxData.size(); }

    VkBuffer GetIdxBuffer() { return m_idxBuffer; }
    VkBuffer GetVertBuffer() { return m_vertBuffer; }

private:
    void InitPipeline();
    void InitPipelineDescriptorSetLayout();
    void InitPipelineLayout();
    void InitShaderModules();
    
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

    SharedLib::Camera*     m_pCamera;
    VkBuffer               m_vpUboBuffer;
    VmaAllocation          m_vpUboAlloc;
    VkDescriptorBufferInfo m_vpUboDesBufferInfo;

    VkBuffer               m_lightPosBuffer;
    VmaAllocation          m_lightPosBufferAlloc;
    VkDescriptorBufferInfo m_lightPosUboDesBufferInfo;

    // Descriptor set 0 bindings
    std::vector<VkWriteDescriptorSet> m_writeDescriptorSet0;

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
