#pragma once
#include "../../3-00_SharedLibrary/Application.h"
#include "hdrloader.h"

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

    VkDeviceSize GetHdrByteNum();
    void* GetHdrDataPointer() { return m_hdrLoaderResult.cols; }
    VkImage GetCubeMapImage() { return m_hdrCubeMapImage; }
    HDRLoaderResult GetHdrLoadResult() { return m_hdrLoaderResult; }
    VkFence GetFence(uint32_t i) { return m_inFlightFences[i]; }

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
    HDRLoaderResult m_hdrLoaderResult;

    SharedLib::Camera*           m_pCamera;
    std::vector<VkBuffer>        m_cameraParaBuffers;
    std::vector<VmaAllocation>   m_cameraParaBufferAllocs;
    std::vector<VkDescriptorSet> m_skyboxPipelineDescriptorSet0s;

    VkShaderModule        m_vsSkyboxShaderModule;
    VkShaderModule        m_psSkyboxShaderModule;
    VkDescriptorSetLayout m_skyboxPipelineDesSet0Layout;
    VkPipelineLayout      m_skyboxPipelineLayout;
    VkPipeline            m_skyboxPipeline;
};