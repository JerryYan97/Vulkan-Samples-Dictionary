#pragma once
#include "../../../SharedLibrary/Application/GlfwApplication.h"
#include "../../../SharedLibrary/Pipeline/Pipeline.h"
#include <chrono>

VK_DEFINE_HANDLE(VmaAllocation);

namespace SharedLib
{
    class Camera;
}

struct ImgInfo
{
    VkImage gpuImg;
    VmaAllocation gpuImgAlloc;
    VkImageView gpuImgView;
    VkSampler gpuImgSampler;
    VkDescriptorImageInfo gpuImgDescriptorInfo;

    // It's possible that a gpu image has multiple layers or mipmaps.
    std::vector<uint32_t> pixWidth;
    std::vector<uint32_t> pixHeight;
    uint32_t componentCnt;
    std::vector<std::vector<uint8_t>> dataVec;
    std::vector<float*> pData;
};

struct BufferInfo
{
    VkBuffer               gpuBuffer;
    VmaAllocation          gpuBufferAlloc;
    VkDescriptorBufferInfo gpuBufferDescriptorInfo;
};

struct Mesh
{
    float worldPos[4];
    std::vector<float>    vertData;
    std::vector<uint16_t> idxData;

    ImgInfo baseColorImg;

    VkBuffer      modelVertBuffer;
    VmaAllocation modelVertBufferAlloc;
    VkBuffer      modelIdxBuffer;
    VmaAllocation modelIdxBufferAlloc;
};

const uint32_t VpMatBytesCnt = 4 * 4 * sizeof(float);
const uint32_t IblMvpMatsBytesCnt = 2 * 4 * 4 * sizeof(float);
const float ModelWorldPos[3] = {0.f, -0.5f, 0.f};
const float Radius = 1.5f;
const float RotateRadiensPerSecond = 3.1415926 * 2.f / 10.f; // 10s -- a circle.

class SkinAnimGltfApp : public SharedLib::GlfwApplication
{
public:
    SkinAnimGltfApp();
    ~SkinAnimGltfApp();

    virtual void AppInit() override;

    void CmdPushSkeletonSkinRenderingDescriptors(VkCommandBuffer cmdBuffer, const Mesh& mesh);

    void UpdateCameraAndGpuBuffer();

    void GetCameraPos(float* pOut);

    void ReadInIBL();

    VkFence GetFence(uint32_t i) { return m_inFlightFences[i]; }

    VkPipeline GetSkinAimPipeline() { return m_skinAnimPipeline.GetVkPipeline(); }
    VkPipelineLayout GetIblPipelineLayout() { return m_skinAnimPipelineLayout; }
    
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

    SharedLib::Camera*    m_pCamera;

    // Sphere rendering
    VkShaderModule        m_vsSkinAnimShaderModule;
    VkShaderModule        m_psSkinAnimShaderModule;
    VkDescriptorSetLayout m_skinAnimPipelineDesSetLayout; // For a pipeline, it can only have one descriptor set layout.
    VkPipelineLayout      m_skinAnimPipelineLayout;
    SharedLib::Pipeline   m_skinAnimPipeline;

    ImgInfo m_diffuseIrradianceCubemap;
    ImgInfo m_prefilterEnvCubemap;
    ImgInfo m_envBrdfImg;

    std::vector<Mesh> m_gltfModeMeshes;

    float m_currentRadians;
    std::chrono::steady_clock::time_point m_lastTime;
    bool m_isFirstTimeRecord;
};
