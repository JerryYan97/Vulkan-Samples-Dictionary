#pragma once
#include "../../SharedLibrary/Application/Application.h"
#include "../../SharedLibrary/Pipeline/Pipeline.h"

namespace SharedLib
{
    class Camera;
}

class SphericalToCubemap : public SharedLib::Application
{
public:
    SphericalToCubemap();
    ~SphericalToCubemap();

    virtual void AppInit() override;

    void InitPipeline();
    void InitPipelineDescriptorSetLayout();
    void InitPipelineLayout();
    void InitShaderModules();

    void ReadInHdri(const std::string& namePath);
    void SaveCubemap(const std::string& namePath, uint32_t width, uint32_t height, uint32_t components, float* pData);

    void InitHdriGpuObjects();
    void DestroyHdriGpuObjects();
    void InitSceneBufferInfo();

    float* GetInputHdriData() { return m_hdriData; }
    uint32_t GetInputHdriWidth() { return m_width; }
    uint32_t GetInputHdriHeight() { return m_height; }
    VkImage GetHdriImg() { return m_inputHdri; }
    VkImageView GetOutputCubemapImgView() { return m_outputCubemapImageView; }
    VkPipeline GetPipeline() { return m_pipeline.GetVkPipeline(); }
    VkExtent3D GetOutputCubemapExtent() { return m_outputCubemapExtent; }
    VkImage GetOutputCubemapImg() { return m_outputCubemap; }
    VkPipelineLayout GetPipelineLayout() { return m_pipelineLayout; }
    std::vector<VkWriteDescriptorSet> GetDescriptorSet0Writes();

private:
    VkBuffer               m_uboBuffer;
    VmaAllocation          m_uboAlloc;
    VkDescriptorBufferInfo m_uboDesBufferInfo;

    VkImage               m_inputHdri;
    VmaAllocation         m_inputHdriAlloc;
    VkImageView           m_inputHdriImageView;
    VkSampler             m_inputHdriSampler;
    VkDescriptorImageInfo m_inputHdriDesImgInfo;

    uint32_t m_width;
    uint32_t m_height;
    float*   m_hdriData;

    VkImage       m_outputCubemap;
    VmaAllocation m_outputCubemapAlloc;
    VkImageView   m_outputCubemapImageView;
    VkExtent3D    m_outputCubemapExtent;

    std::vector<VkWriteDescriptorSet> m_descriptorSet0Writes;

    VkShaderModule        m_vsShaderModule;
    VkShaderModule        m_psShaderModule;
    VkDescriptorSetLayout m_pipelineDesSet0Layout;
    VkPipelineLayout      m_pipelineLayout;
    SharedLib::Pipeline   m_pipeline;
};