#pragma once
#include "../../../SharedLibrary/Application/GlfwApplication.h"
#include "../../../SharedLibrary/Pipeline/Pipeline.h"

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

    VkImage       baseColorImg;
    VmaAllocation baseColorImgAlloc;
    VkImageView   baseColorImgView;
    VkSampler     baseColorImgSampler;

    VkImage       metallicRoughnessImg;
    VmaAllocation metallicRoughnessImgAlloc;
    VkImageView   metallicRoughnessImgView;
    VkSampler     metallicRoughnessImgSampler;

    VkImage       normalImg;
    VmaAllocation normalImgAlloc;
    VkImageView   normalImgView;
    VkSampler     normalImgSampler;

    VkImage       occlusionImg;
    VmaAllocation occlusionImgAlloc;
    VkImageView   occlusionImgView;
    VkSampler     occlusionImgSampler;

    VkImage       emissiveImg;
    VmaAllocation emissiveImgAlloc;
    VkImageView   emissiveImgView;
    VkSampler     emissiveImgSampler;
};

/*
enum GLTF_ACCESSOR_DATA_TYPE
{
    SIGNED_BYTE = 5120,
    UNSIGNED_BYTE,
    SIGNED_SHORT,
    UNSIGNED_SHORT,
    UNSIGNED_INT = 5125,
    FLOAT
};
*/

const uint32_t VpMatBytesCnt = 4 * 4 * sizeof(float);
const uint32_t IblMvpMatsBytesCnt = 2 * 4 * 4 * sizeof(float);

class PBRIBLGltfApp : public SharedLib::GlfwApplication
{
public:
    PBRIBLGltfApp();
    ~PBRIBLGltfApp();

    virtual void AppInit() override;

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

    VkDescriptorSet GetSkyboxCurrentFrameDescriptorSet0()
        { return m_skyboxPipelineDescriptorSet0s[m_currentFrame]; }

    VkPipeline GetSkyboxPipeline() { return m_skyboxPipeline.GetVkPipeline(); }

    void GetCameraData(float* pBuffer);

    VkDescriptorSet GetIblCurrentFrameUboDescriptorSet() 
        { return m_iblPipelineUboDescriptorSets[m_currentFrame]; }

    VkDescriptorSet GetIblTexDescriptorSet()
        { return m_iblPipelineBackgroundTexDescriptorSet; }

    VkDescriptorSet GetMeshTexDescriptorSet(uint32_t i)
        { return m_iblPipelineModelTexDescriptorSets[i]; }
    
    VkPipeline GetIblPipeline() { return m_iblPipeline.GetVkPipeline(); }
    VkPipelineLayout GetIblPipelineLayout() { return m_iblPipelineLayout; }
    
    void SendCameraDataToBuffer(uint32_t i);
    void SendModelTexDataToGPU(VkCommandBuffer cmdBuffer);

    const std::vector<Mesh>& GetModelMeshes() { return m_gltfModeMeshes; }
    uint32_t GetModelTexCnt();

private:
    VkPipelineVertexInputStateCreateInfo CreatePipelineVertexInputInfo();
    VkPipelineDepthStencilStateCreateInfo CreateDepthStencilStateInfo();

    // Init mesh data
    // void InitSphereVertexIndexBuffers();
    // void DestroySphereVertexIndexBuffers();
    void InitModelInfo();
    void DestroyModelInfo();

    // Skybox pipeline resources init.
    void InitSkyboxPipeline();
    void InitSkyboxPipelineDescriptorSetLayout();
    void InitSkyboxPipelineLayout();
    void InitSkyboxShaderModules();
    void InitSkyboxPipelineDescriptorSets();
    void DestroySkyboxPipelineRes();

    // IBL spheres pipeline resources init.
    void InitIblPipeline();
    void InitIblPipelineDescriptorSetLayout();
    void InitIblPipelineLayout();
    void InitIblShaderModules();
    void InitIblPipelineDescriptorSets();
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

    ImgInfo m_hdrImgCubemap;

    std::vector<VkBuffer>      m_vpMatUboBuffer;
    std::vector<VmaAllocation> m_vpMatUboAlloc;

    std::vector<VkBuffer>      m_iblMvpMatsUboBuffer;
    std::vector<VmaAllocation> m_iblMvpMatsUboAlloc;

    SharedLib::Camera*           m_pCamera;
    std::vector<VkBuffer>        m_cameraParaBuffers;
    std::vector<VmaAllocation>   m_cameraParaBufferAllocs;
    std::vector<VkDescriptorSet> m_skyboxPipelineDescriptorSet0s;

    VkShaderModule        m_vsSkyboxShaderModule;
    VkShaderModule        m_psSkyboxShaderModule;
    VkDescriptorSetLayout m_skyboxPipelineDesSet0Layout;
    VkPipelineLayout      m_skyboxPipelineLayout;
    SharedLib::Pipeline   m_skyboxPipeline;

    // Sphere rendering
    VkShaderModule               m_vsIblShaderModule;
    VkShaderModule               m_psIblShaderModule;
    VkDescriptorSetLayout        m_iblPipelineDesSetsLayouts[3]; // 0 -- UBO; 1 -- background textures; 3 -- model textures.
    VkPipelineLayout             m_iblPipelineLayout;
    std::vector<VkDescriptorSet> m_iblPipelineModelTexDescriptorSets;
    VkDescriptorSet              m_iblPipelineBackgroundTexDescriptorSet;
    std::vector<VkDescriptorSet> m_iblPipelineUboDescriptorSets;
    SharedLib::Pipeline          m_iblPipeline;

    VkImage       m_diffuseIrradianceCubemap;
    VkImageView   m_diffuseIrradianceCubemapImgView;
    VkSampler     m_diffuseIrradianceCubemapSampler;
    VmaAllocation m_diffuseIrradianceCubemapAlloc;
    ImgInfo       m_diffuseIrradianceCubemapImgInfo;

    VkImage              m_prefilterEnvCubemap;
    VkImageView          m_prefilterEnvCubemapView;
    VkSampler            m_prefilterEnvCubemapSampler;
    VmaAllocation        m_prefilterEnvCubemapAlloc;
    std::vector<ImgInfo> m_prefilterEnvCubemapImgsInfo;

    VkImage       m_envBrdfImg;
    VkImageView   m_envBrdfImgView;
    VkSampler     m_envBrdfImgSampler;
    VmaAllocation m_envBrdfImgAlloc;
    ImgInfo       m_envBrdfImgInfo;

    std::vector<Mesh> m_gltfModeMeshes;
};
