#pragma once
#include "../../../SharedLibrary/Application/GlfwApplication.h"
#include "../../../SharedLibrary/Pipeline/Pipeline.h"
// #include "../../../SharedLibrary/AnimLogger/AnimLogger.h"
#include <chrono>

VK_DEFINE_HANDLE(VmaAllocation);

namespace SharedLib
{
    class Camera;
}

struct ImgInfo
{
    uint32_t pixWidth;
    uint32_t pixHeight;
    uint32_t componentCnt;
    std::vector<uint8_t> dataVec;
    float* pData;
};

struct BinBufferInfo
{
    uint32_t byteCnt;
    float* pData;
};

struct Mesh
{
    float worldPos[4];
    std::vector<float>    vertData;
    std::vector<uint16_t> idxData;

    ImgInfo baseColorTex;
    ImgInfo metallicRoughnessTex;
    ImgInfo normalTex;
    ImgInfo occlusionTex;
    ImgInfo emissiveTex;

    VkBuffer      modelVertBuffer;
    VmaAllocation modelVertBufferAlloc;
    VkBuffer      modelIdxBuffer;
    VmaAllocation modelIdxBufferAlloc;

    VkImage               baseColorImg;
    VmaAllocation         baseColorImgAlloc;
    VkImageView           baseColorImgView;
    VkSampler             baseColorImgSampler;
    VkDescriptorImageInfo baseColorImgDescriptorInfo;

    VkImage               metallicRoughnessImg;
    VmaAllocation         metallicRoughnessImgAlloc;
    VkImageView           metallicRoughnessImgView;
    VkSampler             metallicRoughnessImgSampler;
    VkDescriptorImageInfo metallicRoughnessImgDescriptorInfo;

    VkImage               normalImg;
    VmaAllocation         normalImgAlloc;
    VkImageView           normalImgView;
    VkSampler             normalImgSampler;
    VkDescriptorImageInfo normalImgDescriptorInfo;

    VkImage               occlusionImg;
    VmaAllocation         occlusionImgAlloc;
    VkImageView           occlusionImgView;
    VkSampler             occlusionImgSampler;
    VkDescriptorImageInfo occlusionImgDescriptorInfo;

    VkImage               emissiveImg;
    VmaAllocation         emissiveImgAlloc;
    VkImageView           emissiveImgView;
    VkSampler             emissiveImgSampler;
    VkDescriptorImageInfo emissiveImgDescriptorInfo;
};

const uint32_t VpMatBytesCnt = 4 * 4 * sizeof(float);
const uint32_t IblMvpMatsBytesCnt = 2 * 4 * 4 * sizeof(float);
const float ModelWorldPos[3] = {0.f, -0.5f, 0.f};
const float Radius = 1.5f;
const float RotateRadiensPerSecond = 3.1415926 * 2.f / 10.f; // 10s -- a circle.
const bool DumpAnim = true;

class PBRIBLGltfApp : public SharedLib::GlfwApplication
{
public:
    PBRIBLGltfApp();
    ~PBRIBLGltfApp();

    virtual void AppInit() override;

    void CmdCopyPresentImgToLogAnim(VkCommandBuffer cmdBuffer, uint32_t swapchainImgIdx);
    void CmdPushCubemapDescriptors(VkCommandBuffer cmdBuffer);
    void CmdPushIblModelRenderingDescriptors(VkCommandBuffer cmdBuffer, const Mesh& mesh);

    void DumpRenderedFrame(VkCommandBuffer cmdBuffer);

    void UpdateCameraAndGpuBuffer();

    void GetCameraPos(float* pOut);
    uint32_t GetMaxMipLevel() { return m_prefilterEnvCubemapImgsInfo.size(); }

    VkDeviceSize GetHdrByteNum();
    void* GetHdrDataPointer() { return m_hdrImgCubemap.pData; }
    VkImage GetCubeMapImage() { return m_hdrCubeMapImage; }
    VkExtent2D GetHdrImgExtent() 
        { return VkExtent2D{ m_hdrImgCubemap.pixWidth, m_hdrImgCubemap.pixHeight }; }
    ImgInfo GetBackgroundCubemapInfo() { return m_hdrImgCubemap; }
    ImgInfo GetDiffuseIrradianceImgInfo() { return m_diffuseIrradianceCubemapImgInfo; }
    VkImage GetDiffuseIrradianceCubemap() { return m_diffuseIrradianceCubemap; }
    std::vector<ImgInfo> GetPrefilterEnvImgsInfo() { return m_prefilterEnvCubemapImgsInfo; }
    VkImage GetPrefilterEnvCubemap() { return m_prefilterEnvCubemap; }
    ImgInfo GetEnvBrdfImgInfo() { return m_envBrdfImgInfo; }
    VkImage GetEnvBrdf() { return m_envBrdfImg; }

    VkFence GetFence(uint32_t i) { return m_inFlightFences[i]; }

    VkPipelineLayout GetSkyboxPipelineLayout() { return m_skyboxPipelineLayout; }

    VkPipeline GetSkyboxPipeline() { return m_skyboxPipeline.GetVkPipeline(); }

    VkPipeline GetIblPipeline() { return m_iblPipeline.GetVkPipeline(); }
    VkPipelineLayout GetIblPipelineLayout() { return m_iblPipelineLayout; }
    
    const std::vector<Mesh>& GetModelMeshes() { return m_gltfModeMeshes; }
    uint32_t GetModelTexCnt();

    void SendCameraDataToBuffer(uint32_t i);

private:
    VkPipelineVertexInputStateCreateInfo CreatePipelineVertexInputInfo();
    VkPipelineDepthStencilStateCreateInfo CreateDepthStencilStateInfo();

    // Init mesh data
    void InitModelInfo();
    void DestroyModelInfo();

    // Skybox pipeline resources init.
    void InitSkyboxPipeline();
    void InitSkyboxPipelineDescriptorSetLayout();
    void InitSkyboxPipelineLayout();
    void InitSkyboxShaderModules();
    void DestroySkyboxPipelineRes();

    // IBL spheres pipeline resources init.
    void InitIblPipeline();
    void InitIblPipelineDescriptorSetLayout();
    void InitIblPipelineLayout();
    void InitIblShaderModules();
    void DestroyIblPipelineRes();

    void InitVpMatBuffer();
    void DestroyVpMatBuffer();

    void InitIblMvpMatsBuffer();
    void DestroyIblMvpMatsBuffer();

    // Shared resources init and destroy.
    void InitHdrRenderObjects();
    void InitCameraUboObjects();

    void DestroyHdrRenderObjs();
    void DestroyCameraUboObjects();

    VkImage         m_hdrCubeMapImage;
    VkImageView     m_hdrCubeMapView;
    VkSampler       m_hdrSampler;
    VmaAllocation   m_hdrCubeMapAlloc;
    VkDescriptorImageInfo m_hdrCubeMapImgDescriptorInfo;

    ImgInfo m_hdrImgCubemap;

    std::vector<VkBuffer>      m_vpMatUboBuffer;
    std::vector<VmaAllocation> m_vpMatUboAlloc;
    std::vector<VkDescriptorBufferInfo> m_vpMatUboDescriptorBuffersInfos;

    std::vector<VkBuffer>      m_iblMvpMatsUboBuffer;
    std::vector<VmaAllocation> m_iblMvpMatsUboAlloc;
    std::vector<VkDescriptorBufferInfo> m_iblMvpMatsUboDescriptorBuffersInfos;

    SharedLib::Camera*                  m_pCamera;
    std::vector<VkBuffer>               m_cameraParaBuffers;
    std::vector<VmaAllocation>          m_cameraParaBufferAllocs;
    std::vector<VkDescriptorBufferInfo> m_cameraParaBuffersDescriptorsInfos;

    VkShaderModule        m_vsSkyboxShaderModule;
    VkShaderModule        m_psSkyboxShaderModule;
    VkDescriptorSetLayout m_skyboxPipelineDesSet0Layout;
    VkPipelineLayout      m_skyboxPipelineLayout;
    SharedLib::Pipeline   m_skyboxPipeline;

    // Sphere rendering
    VkShaderModule                      m_vsIblShaderModule;
    VkShaderModule                      m_psIblShaderModule;
    VkDescriptorSetLayout               m_iblPipelineDesSetLayout; // For a pipeline, it can only have one descriptor set layout.
    VkPipelineLayout                    m_iblPipelineLayout;
    std::vector<VkDescriptorImageInfo>  m_iblModelTexturesDescriptorsInfos;
    VkDescriptorImageInfo               m_iblBackgroundTextureDescriptorInfo;
    std::vector<VkDescriptorBufferInfo> m_iblUboDescriptorInfo;
    SharedLib::Pipeline                 m_iblPipeline;

    VkImage               m_diffuseIrradianceCubemap;
    VkImageView           m_diffuseIrradianceCubemapImgView;
    VkSampler             m_diffuseIrradianceCubemapSampler;
    VmaAllocation         m_diffuseIrradianceCubemapAlloc;
    ImgInfo               m_diffuseIrradianceCubemapImgInfo;
    VkDescriptorImageInfo m_diffuseIrradianceCubemapDescriptorImgInfo;

    VkImage               m_prefilterEnvCubemap;
    VkImageView           m_prefilterEnvCubemapView;
    VkSampler             m_prefilterEnvCubemapSampler;
    VmaAllocation         m_prefilterEnvCubemapAlloc;
    std::vector<ImgInfo>  m_prefilterEnvCubemapImgsInfo;
    VkDescriptorImageInfo m_prefilterEnvCubemapDescriptorImgInfo;

    VkImage               m_envBrdfImg;
    VkImageView           m_envBrdfImgView;
    VkSampler             m_envBrdfImgSampler;
    VmaAllocation         m_envBrdfImgAlloc;
    ImgInfo               m_envBrdfImgInfo;
    VkDescriptorImageInfo m_envBrdfImgDescriptorImgInfo;

    std::vector<Mesh> m_gltfModeMeshes;

    float m_currentRadians;
    std::chrono::steady_clock::time_point m_lastTime;
    bool m_isFirstTimeRecord;

    // SharedLib::AnimLogger* m_pAnimLogger;
};
