#pragma once
#include "../../../SharedLibrary/Application/GlfwApplication.h"
#include "../../../SharedLibrary/Pipeline/Pipeline.h"

VK_DEFINE_HANDLE(VmaAllocation);

namespace SharedLib
{
    class Camera;
}

const uint32_t VpMatBytesCnt = 4 * 4 * sizeof(float);

class PBRIBLApp : public SharedLib::GlfwApplication
{
public:
    PBRIBLApp();
    ~PBRIBLApp();

    virtual void AppInit() override;

    void UpdateCameraAndGpuBuffer();

    void GetCameraPos(float* pOut);

    VkPipelineLayout GetSkyboxPipelineLayout() { return m_skyboxPipelineLayout; }

    uint32_t GetMaxMipLevel() { return m_prefilterEnvMipsCnt; }

    void CmdPushSkyboxDescriptors(VkCommandBuffer cmdBuffer);
    void CmdPushSphereIBLDescriptors(VkCommandBuffer cmdBuffer);

    VkPipeline GetSkyboxPipeline() { return m_skyboxPipeline.GetVkPipeline(); }

    VkBuffer GetIblVertBuffer() { return m_vertBuffer; }
    VkBuffer GetIblIdxBuffer() { return m_idxBuffer; }
    uint32_t GetIdxCnt() { return m_idxBufferData.size(); }

    VkPipeline GetIblPipeline() { return m_iblPipeline.GetVkPipeline(); }
    VkPipelineLayout GetIblPipelineLayout() { return m_iblPipelineLayout; }
    
    void SendCameraDataToBuffer();

private:
    VkPipelineVertexInputStateCreateInfo CreatePipelineVertexInputInfo();
    VkPipelineDepthStencilStateCreateInfo CreateDepthStencilStateInfo();

    // Init mesh data
    void InitSphereVertexIndexBuffers();
    void DestroySphereVertexIndexBuffers();

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

    // Shared resources init and destroy.
    void InitHdrRenderObjects();
    void InitCameraUboObjects();

    void DestroyHdrRenderObjs();
    void DestroyCameraUboObjects();

    SharedLib::GpuImg m_hdrCubeMap;

    std::vector<float>    m_vertBufferData;
    std::vector<uint32_t> m_idxBufferData;
    VkBuffer              m_vertBuffer;
    VmaAllocation         m_vertBufferAlloc;
    VkBuffer              m_idxBuffer;
    VmaAllocation         m_idxBufferAlloc;

    std::vector<SharedLib::GpuBuffer> m_vpMatUboBuffer;

    SharedLib::Camera*                m_pCamera;
    std::vector<SharedLib::GpuBuffer> m_cameraParaBuffers;

    VkShaderModule        m_vsSkyboxShaderModule;
    VkShaderModule        m_psSkyboxShaderModule;
    VkDescriptorSetLayout m_skyboxPipelineDesSet0Layout;
    VkPipelineLayout      m_skyboxPipelineLayout;
    SharedLib::Pipeline   m_skyboxPipeline;

    // Sphere rendering
    VkShaderModule        m_vsIblShaderModule;
    VkShaderModule        m_psIblShaderModule;
    VkDescriptorSetLayout m_iblPipelineDesSet0Layout;
    VkPipelineLayout      m_iblPipelineLayout;
    SharedLib::Pipeline   m_iblPipeline;

    SharedLib::GpuImg m_diffuseIrradianceCubemap;
    SharedLib::GpuImg m_prefilterEnvCubemap;
    SharedLib::GpuImg m_envBrdfImg;

    uint32_t m_prefilterEnvMipsCnt;
};
