#pragma once
#include "../../SharedLibrary/Application/Application.h"
#include "../../SharedLibrary/Pipeline/Pipeline.h"

namespace SharedLib
{
    class Camera;
}

class SphericalToCubemap : public SharedLib::GlfwApplication
{
public:
    SphericalToCubemap();
    ~SphericalToCubemap();

    virtual void AppInit() override;

private:
    SharedLib::Camera* m_pCamera;

    VkBuffer      m_uboBuffer;
    VmaAllocation m_uboAlloc;

    VkImage       m_inputHdri;
    VmaAllocation m_inputHdriAlloc;
    VkImageView   m_inputHdriImageView;

    VkImage       m_outputCubemap;
    VmaAllocation m_outputCubemapAlloc;
    VkImageView   m_outputCubemapImageView;

    VkDescriptorSet m_pipelineDescriptorSet0;

    VkShaderModule        m_vsShaderModule;
    VkShaderModule        m_psShaderModule;
    VkDescriptorSetLayout m_pipelineDesSetLayout;
    VkPipelineLayout      m_pipelineLayout;
    SharedLib::Pipeline   m_pipeline;
};