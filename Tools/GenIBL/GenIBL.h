#pragma once
#include "../../SharedLibrary/Application/Application.h"
#include "../../SharedLibrary/Pipeline/Pipeline.h"

VK_DEFINE_HANDLE(VmaAllocation);

class GenIBL : public SharedLib::Application
{
public:
    GenIBL();
    ~GenIBL();

    virtual void AppInit() override;
    VkDeviceSize GetHdrByteNum();

private:
    void InitDiffuseIrradiancePipeline();
    void InitDiffuseIrradiancePipelineDescriptorSetLayout();
    void InitDiffuseIrradiancePipelineLayout();
    void InitDiffuseIrradianceShaderModules();
    void InitDiffuseIrradiancePipelineDescriptorSets();
    void DestroyDiffuseIrradiancePipelineResourses();

    void InitPrefilterEnvMapPipeline();
    void InitPrefilterEnvMapPipelineDescriptorSetLayout();
    void InitPrefilterEnvMapPipelineLayout();
    void InitPrefilterEnvMapShaderModules();
    void InitPrefilterEnvMapPipelineDescriptorSets();
    void DestroyPrefilterEnvMapPipelineResourses();

    

    void InitHdrRenderObjects();
    void DestroyHdrRenderObjs();

    SharedLib::Pipeline m_diffuseIrradiancePipeline;
    SharedLib::Pipeline m_preFilterEnvMapPipeline; // Specular split-sum 1st element.
    SharedLib::Pipeline m_envBrdfPipeline; // Specular split-sum 2st element.

    VkImage       m_hdrCubeMapImage;
    VkImageView   m_hdrCubeMapView;
    VkSampler     m_hdrCubeMapSampler;
    VmaAllocation m_hdrCubeMapAlloc;

    uint32_t m_hdrImgWidth;
    uint32_t m_hdrImgHeight;
    float*   m_hdrImgData;
};
