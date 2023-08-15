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
    void InitPipelineDescriptorSets();

    void ReadInHdri(const std::string& namePath);

    void CreateHdriGpuObjects();
    void DestroyHdriGpuObjects();

    float* GetInputHdriData() { return m_hdriData; }
    uint32_t GetInputHdriWidth() { return m_width; }
    uint32_t GetInputHdriHeight() { return m_height; }
    VkImage GetHdriImg() { return m_inputHdri; }
    VkImageView GetOutputCubemapImgView() { return m_outputCubemapImageView; }
    VkPipeline GetPipeline() { return m_pipeline.GetVkPipeline(); }
    VkExtent3D GetOutputCubemapExtent() { return m_outputCubemapExtent; }

private:
    SharedLib::Camera* m_pCamera;

    VkBuffer      m_uboBuffer;
    VmaAllocation m_uboAlloc;

    VkImage       m_inputHdri;
    VmaAllocation m_inputHdriAlloc;
    VkImageView   m_inputHdriImageView;

    uint32_t m_width;
    uint32_t m_height;
    float*   m_hdriData;

    VkImage       m_outputCubemap;
    VmaAllocation m_outputCubemapAlloc;
    VkImageView   m_outputCubemapImageView;
    VkExtent3D    m_outputCubemapExtent;

    VkDescriptorSet m_pipelineDescriptorSet0;

    VkShaderModule        m_vsShaderModule;
    VkShaderModule        m_psShaderModule;
    VkDescriptorSetLayout m_pipelineDesSet0Layout;
    VkPipelineLayout      m_pipelineLayout;
    SharedLib::Pipeline   m_pipeline;
};