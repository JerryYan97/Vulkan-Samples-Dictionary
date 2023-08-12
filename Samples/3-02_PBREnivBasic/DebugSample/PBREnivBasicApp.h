#pragma once
#include "../../../SharedLibrary/Application/Application.h"
#include "../../../SharedLibrary/Pipeline/Pipeline.h"

VK_DEFINE_HANDLE(VmaAllocation);

namespace SharedLib
{
    class Camera;
}

class PBREnivBasicApp : public SharedLib::GlfwApplication
{
public:
    PBREnivBasicApp();
    ~PBREnivBasicApp();

    virtual void AppInit() override;

    void UpdateCameraAndGpuBuffer();

    VkDeviceSize GetHdrByteNum();
    void* GetHdrDataPointer() { return m_hdrImgData; }
    VkImage GetCubeMapImage() { return m_hdrCubeMapImage; }
    VkExtent2D GetHdrImgExtent() 
        { return VkExtent2D{ m_hdrImgWidth, m_hdrImgHeight }; }

    VkFence GetFence(uint32_t i) { return m_inFlightFences[i]; }

    VkPipelineLayout GetSkyboxPipelineLayout() { return m_skyboxPipelineLayout; }

    VkDescriptorSet GetSkyboxCurrentFrameDescriptorSet0() 
        { return m_skyboxPipelineDescriptorSet0s[m_currentFrame]; }

    VkPipeline GetSkyboxPipeline() { return m_skyboxPipeline.GetVkPipeline(); }

    void GetCameraData(float* pBuffer);

    void SendCameraDataToBuffer(uint32_t i);

private:
    void InitSkyboxPipeline();
    void InitSkyboxPipelineDescriptorSetLayout();
    void InitSkyboxPipelineLayout();
    void InitSkyboxShaderModules();
    void InitSkyboxPipelineDescriptorSets();

    void InitHdrRenderObjects();
    void InitCameraUboObjects();

    void DestroyHdrRenderObjs();
    void DestroyCameraUboObjects();

    VkImage         m_hdrCubeMapImage;
    VkImageView     m_hdrCubeMapView;
    VkSampler       m_hdrSampler;
    VmaAllocation   m_hdrCubeMapAlloc;

    uint32_t m_hdrImgWidth;
    uint32_t m_hdrImgHeight;
    float*   m_hdrImgData;

    SharedLib::Camera*           m_pCamera;
    std::vector<VkBuffer>        m_cameraParaBuffers;
    std::vector<VmaAllocation>   m_cameraParaBufferAllocs;
    std::vector<VkDescriptorSet> m_skyboxPipelineDescriptorSet0s;

    VkShaderModule        m_vsSkyboxShaderModule;
    VkShaderModule        m_psSkyboxShaderModule;
    VkDescriptorSetLayout m_skyboxPipelineDesSet0Layout;
    VkPipelineLayout      m_skyboxPipelineLayout;
    SharedLib::Pipeline   m_skyboxPipeline;
};
