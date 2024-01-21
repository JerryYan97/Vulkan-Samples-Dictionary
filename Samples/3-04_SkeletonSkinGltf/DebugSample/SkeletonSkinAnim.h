#pragma once
#include "../../../SharedLibrary/Application/GlfwApplication.h"
#include "../../../SharedLibrary/Pipeline/Pipeline.h"
#include <chrono>
#include <array>

VK_DEFINE_HANDLE(VmaAllocation);

namespace SharedLib
{
    class Camera;
}

struct ImgInfo
{
    SharedLib::GpuImg gpuImg;

    // It's possible that a gpu image has multiple layers or mipmaps.
    std::vector<uint32_t> pixWidths;
    std::vector<uint32_t> pixHeights;
    uint32_t componentCnt;
    std::vector<std::vector<uint8_t>> dataVec;
    std::vector<float*> pData;
};

struct Mesh
{
    std::vector<float>    vertData;
    std::vector<uint16_t> idxData;

    ImgInfo baseColorImg;

    SharedLib::GpuBuffer vertBuffer;
    SharedLib::GpuBuffer idxBuffer;
    SharedLib::GpuBuffer jointMatsBuffer;
    SharedLib::GpuBuffer weightBuffer;
};

struct Animation
{

};

struct Joint
{
    float localTranslation[3];
    float localRotatoin[4];
    float localScale[3];

    std::array<float, 16> inverseBindMatrix; // Transform a vert in the model space to this joint's local space.
    std::vector<Joint*> children;
};

// Skeleton and mesh are both in the model space.
// The concatnation of a joint's transformation and its parents transformation transforms this joint to the model
// space.
// After that, the global transformation in the `SkeletalMesh` transforms the mesh/joint to the world space.
struct Skeleton
{
    std::vector<Joint> joints; // joints[0] is the root joint.

    SharedLib::GpuBuffer jointsMatsBuffer;
};

struct SkeletalMesh
{
    Mesh mesh;
    Skeleton skeleton;
    Animation animSeqence;

    // Global transformation
    float translation[3];
    float rotation[4];
    float scale[3];
};

const uint32_t VpMatBytesCnt = 4 * 4 * sizeof(float);
const uint32_t IblMvpMatsBytesCnt = 2 * 4 * 4 * sizeof(float);
const float ModelWorldPos[3] = {0.f, -0.5f, 0.f};
const float Radius = 1.5f;
const float RotateRadiensPerSecond = 3.1415926 * 2.f / 10.f; // 10s -- a circle.

class SkinAnimGltfApp : public SharedLib::GlfwApplication
{
public:
    SkinAnimGltfApp(const std::string& iblPath, const std::string& gltfPathName);
    ~SkinAnimGltfApp();

    virtual void AppInit() override;

    void CmdPushSkeletonSkinRenderingDescriptors(VkCommandBuffer cmdBuffer, const Mesh& mesh);

    void UpdateCameraAndGpuBuffer();

    void GetCameraPos(float* pOut);

    VkFence GetFence(uint32_t i) { return m_inFlightFences[i]; }

    VkPipeline GetSkinAimPipeline() { return m_skinAnimPipeline.GetVkPipeline(); }
    VkPipelineLayout GetSkinAimPipelineLayout() { return m_skinAnimPipelineLayout; }
    
    std::vector<float> GetVertPushConsants();
    std::vector<float> GetFragPushConstants();

    // Update joints/skeleton's local transformation and the joints' matrices.
    // It's obstructive because I only want to keep one joints matrix gpu buffer. Thus GPU has to finish works after
    // the buffer updates.
    void UpdateJointsTransAndMats();

private:
    VkPipelineVertexInputStateCreateInfo CreatePipelineVertexInputInfo();
    VkPipelineDepthStencilStateCreateInfo CreateDepthStencilStateInfo();

    // Init scene info: Mesh, skeleton, animation.
    void ReadInInitGltf();
    void DestroyGltf();

    void ReadInInitIBL();

    // Skin animation pipeline resources init.
    void InitSkinAnimPipeline();
    void InitSkinAnimPipelineDescriptorSetLayout();
    void InitSkinAnimPipelineLayout();
    void InitSkinAnimShaderModules();
    void DestroySkinAnimPipelineRes();

    void DestroyHdrRenderObjs();

    SharedLib::Camera*    m_pCamera;

    VkShaderModule        m_vsSkinAnimShaderModule;
    VkShaderModule        m_psSkinAnimShaderModule;
    VkDescriptorSetLayout m_skinAnimPipelineDesSetLayout; // For a pipeline, it can only have one descriptor set layout.
    VkPipelineLayout      m_skinAnimPipelineLayout;
    SharedLib::Pipeline   m_skinAnimPipeline;

    ImgInfo m_diffuseIrradianceCubemap;
    ImgInfo m_prefilterEnvCubemap;
    ImgInfo m_envBrdfImg;

    SkeletalMesh m_skeletalMesh;
    float        m_currentAnimTime;

    float m_currentRadians;
    std::chrono::steady_clock::time_point m_lastTime;
    bool m_isFirstTimeRecord;

    std::string m_iblDir;
    std::string m_gltfPathName;
};
