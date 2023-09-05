#pragma once
#include "../../SharedLibrary/Application/Application.h"
#include "../../SharedLibrary/Pipeline/Pipeline.h"

VK_DEFINE_HANDLE(VmaAllocation);

struct ImgInfo
{
    uint32_t width;
    uint32_t height;
    float*   pData;
};

constexpr int CameraScreenBufferSizeInFloats = 4 * 3 * 6 + 4;
constexpr int CameraScreenBufferSizeInBytes = sizeof(float) * CameraScreenBufferSizeInFloats;
constexpr VkFormat HdriRenderTargetFormat = VK_FORMAT_R32G32B32A32_SFLOAT;

class GenIBL : public SharedLib::Application
{
public:
    GenIBL();
    ~GenIBL();

    virtual void AppInit() override;

    void ReadInCubemap(const std::string& namePath);
private:
    // Shared pipeline resources
    void InitDiffIrrPreFilterEnvMapDescriptorSets();
    void InitDiffIrrPreFilterEnvMapDescriptorSetLayout();

    // Diffuse Irradiance
    void InitDiffuseIrradiancePipeline();
    void InitDiffuseIrradiancePipelineLayout();
    void InitDiffuseIrradianceShaderModules();
    void DestroyDiffuseIrradiancePipelineResourses();

    void InitDiffuseIrradianceOutputObjects();

    // Prefilter Environment Map
    void InitPrefilterEnvMapPipeline();
    void InitPrefilterEnvMapPipelineDescriptorSetLayout();
    void InitPrefilterEnvMapPipelineLayout();
    void InitPrefilterEnvMapShaderModules();
    void InitPrefilterEnvMapPipelineDescriptorSets();
    void DestroyPrefilterEnvMapPipelineResourses();

    // Environment brdf

    // Common resources
    void InitInputCubemapObjects();
    void DestroyInputCubemapRenderObjs();

    void InitCameraScreenUbo();
    void DestroyCameraScreenUbo();

    // Shared pipeline resources
    VkDescriptorSet       m_diffIrrPreFilterEnvMapDesSet0;
    VkDescriptorSetLayout m_diffIrrPreFilterEnvMapDesSet0Layout;

    // Resources for the diffuse irradiance generation.
    SharedLib::Pipeline m_diffuseIrradiancePipeline;
    VkShaderModule      m_diffuseIrradianceVsShaderModule;
    VkShaderModule      m_diffuseIrradiancePsShaderModule;
    VkPipelineLayout    m_diffuseIrradiancePipelineLayout;

    VkImage       m_diffuseIrradianceCubemap; // Note: Mutiview can draw to different layers automatically.
    VmaAllocation m_diffuseIrradianceCubemapAlloc;
    VkImageView   m_diffuseIrradianceCubemapImageView;

    // Resources for the prefilter environment map
    SharedLib::Pipeline m_preFilterEnvMapPipeline; // Specular split-sum 1st element.

    // Resrouces for the environment brdf
    SharedLib::Pipeline m_envBrdfPipeline; // Specular split-sum 2st element.

    // Input cubemap resources.
    VkImage       m_hdrCubeMapImage;
    VkImageView   m_hdrCubeMapView;
    VkSampler     m_hdrCubeMapSampler;
    VmaAllocation m_hdrCubeMapAlloc;
    ImgInfo       m_hdrCubeMapInfo;

    // Camera and screen info buffer for cubemap gen (Diffuse irradiance and prefilter env map).
    VkBuffer      m_uboCameraScreenBuffer;
    VmaAllocation m_uboCameraScreenAlloc;
};
