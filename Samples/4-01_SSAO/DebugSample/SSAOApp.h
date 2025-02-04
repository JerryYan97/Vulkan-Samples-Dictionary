#pragma once
#include "../../../SharedLibrary/Application/DearImGuiApplication.h"
#include "../../../SharedLibrary/Pipeline/Pipeline.h"
#include <array>

VK_DEFINE_HANDLE(VmaAllocation);

namespace SharedLib
{
    class Camera;
    class GltfLoaderManager;
    class Level;
}

constexpr uint32_t SphereCounts = 4 * 4;
constexpr uint32_t PtLightsCounts = 6 * 6 * 2; // Upper 36 point lights and bottom 36 point lights.

enum PresentType
{
    DIFFUSE,
    NORMAL,
    SSAO_FACTOR_RAW,
    SSAO_FACTOR_BLUR,
    ROUGHNESS,
    METALLIC,
    FINAL
};

class SSAOApp : public SharedLib::ImGuiApplication
{
public:
    SSAOApp();
    ~SSAOApp();

    virtual void AppInit() override;

    VkFence GetFence(uint32_t i) { return m_inFlightFences[i]; }

    VkPipelineLayout GetGeoPassPipelineLayout() { return m_geoPassPipelineLayout; }
    // VkPipelineLayout GetDeferredLightingPassPipelineLayout() { return m_deferredLightingPassPipelineLayout; }

    // The first layer is the desciptor ids.
    // The second layer is the bindings writes to that descriptor.
    std::vector<VkWriteDescriptorSet> GetGeoPassWriteDescriptorSets();

    std::vector<VkWriteDescriptorSet> GetDeferredLightingWriteDescriptorSets();

    VkPipeline GetGeoPassPipeline() { return m_geoPassPipeline.GetVkPipeline(); }
    // VkPipeline GetDeferredLightingPassPipeline() { return m_deferredLightingPassPipeline.GetVkPipeline(); }
    // VkPipeline GetDeferredLightingPassDisableCullPipeline() { return m_deferredLightingPassDisableCullingPipeline.GetVkPipeline(); }

    uint32_t GetIdxCnt() { return m_idxData.size(); }

    VkBuffer GetIdxBuffer() { return m_idxBuffer.buffer; }
    VkBuffer GetVertBuffer() { return m_vertBuffer.buffer; }

    // SharedLib::GpuImg GetDeferredLightingRadianceTexture(uint32_t i) { return m_lightingPassRadianceTextures[i]; }

    void CmdSSAOFrameStartLayoutTrans(VkCommandBuffer cmdBuffer); // GBuffer and swapchain images layout transitions.

    void CmdGBufferLayoutTrans(VkCommandBuffer      cmdBuffer,
                               VkImageLayout        oldLayout,
                               VkImageLayout        newLayout,
                               VkAccessFlags        srcAccessMask,
                               VkAccessFlags        dstAccessMask,
                               VkPipelineStageFlags srcStageMask,
                               VkPipelineStageFlags dstStageMask);

    std::vector<VkRenderingAttachmentInfoKHR> GetGBufferAttachments();

    std::vector<float> GetDeferredLightingPushConstantData();

    void UpdateCameraAndGpuBuffer();

    void ImGuiFrame(VkCommandBuffer cmdBuffer) override;

    void CmdGeoPass(VkCommandBuffer cmdBuffer);
    // void CmdTransferGBuffersToShaderRsrc(VkCommandBuffer cmdBuffer); -- NOTE: I can use the TransferGBuffer func.
    void CmdSSAOAppMultiTypeRendering(VkCommandBuffer cmdBuffer);

private:
    void InitGeoPassPipeline();
    void InitGeoPassPipelineDescriptorSetLayout();
    void InitGeoPassPipelineLayout();
    void InitGeoPassShaderModules();

    void InitGBuffer();
    void DestroyGBuffer();
    
    void InitVpUboObjects();
    void DestroyVpUboObjects();

    void SetupInputHandler(); // Setup the command generator for the input handler.

    // void CreateGBufferTransBarriers(); NOTE: We cannot use this because GBuffer sizes can be different for each frame.

    void InitScreenQuadVsShaderModule();

    void InitAlbedoRenderingPipeline();
    void InitAlbedoRenderingPipelineDescriptorSetLayout();
    void InitAlbedoRenderingPipelineLayout();
    void InitAlbedoRenderingShaderModules();

    /*
    void InitDeferredLightingPassPipeline();
    void InitDeferredLightingPassPipelineDescriptorSetLayout();
    void InitDeferredLightingPassPipelineLayout();
    void InitDeferredLightingPassShaderModules();
    void InitDeferredLightingPassRadianceTextures();

    void DestroyDeferredLightingPassRadianceTextures();
    */
    // NOTE: Light volumes are also spheres.
    VkPipelineVertexInputStateCreateInfo CreateGeoPassPipelineVertexInputInfo();
    VkPipelineVertexInputStateCreateInfo CreateDeferredLightingPassPipelineVertexInputInfo();

    // The color is additive, which makes the depth stencil tricky. Each point lights should have their own depth buffer?
    // Not really... We can just use the face culling... Thus, we need two deferred lighting pipeline... One for face
    // culling and another for none culling...
    // We need to draw point lights one by one and check whether a point light includes the camera. If it includes the
    // camera then we need to disable the face culling of the point light volume.
    VkPipelineDepthStencilStateCreateInfo CreateGeoPassDepthStencilStateInfo();
    VkPipelineDepthStencilStateCreateInfo CreateDeferredLightingPassDepthStencilStateInfo();
    
    SharedLib::PipelineColorBlendInfo CreateGeoPassPipelineColorBlendAttachmentStates();
    SharedLib::PipelineColorBlendInfo CreateDeferredLightingPassPipelineColorBlendAttachmentStates();
    
    VkPipelineRasterizationStateCreateInfo CreateDeferredLightingPassDisableCullingRasterizationInfoStateInfo();

    void SendCameraDataToBuffer(uint32_t i);

    SharedLib::Camera* m_pCamera;
    
    uint32_t  m_vertBufferByteCnt;
    uint32_t  m_idxBufferByteCnt;

    std::vector<float>    m_vertData;
    std::vector<uint32_t> m_idxData;

    SharedLib::GpuBuffer m_vertBuffer;
    SharedLib::GpuBuffer m_idxBuffer;

    // Geometry pass pipeline resources
    VkShaderModule        m_geoPassVsShaderModule;
    VkShaderModule        m_geoPassPsShaderModule;
    VkDescriptorSetLayout m_geoPassPipelineDesSetLayout;
    VkPipelineLayout      m_geoPassPipelineLayout;
    SharedLib::Pipeline   m_geoPassPipeline;

    // The Screen Quad VS is shared between different rendering pipelines.
    VkShaderModule        m_screenQuadVsShaderModule;

    // Albedo rendering pipeline resources
    VkShaderModule        m_albedoRenderingPsShaderModule;
    VkDescriptorSetLayout m_albedoRenderingPipelineDesSetLayout;
    VkPipelineLayout      m_albedoRenderingPipelineLayout;
    SharedLib::Pipeline   m_albedoRenderingPipeline;

    // Deferred lighting pipeline resources
    /*
    VkShaderModule        m_deferredLightingPassVsShaderModule;
    VkShaderModule        m_deferredLightingPassPsShaderModule;
    VkDescriptorSetLayout m_deferredLightingPassPipelineDesSetLayout;
    VkPipelineLayout      m_deferredLightingPassPipelineLayout;
    SharedLib::Pipeline   m_deferredLightingPassPipeline;
    SharedLib::Pipeline   m_deferredLightingPassDisableCullingPipeline;
    */

    // G-Buffer textures
    std::vector<SharedLib::GpuImg> m_worldPosTextures;
    std::vector<SharedLib::GpuImg> m_albedoTextures;
    std::vector<SharedLib::GpuImg> m_normalTextures;
    std::vector<SharedLib::GpuImg> m_roughnessMetallicOcclusionTextures;
    std::vector<VkFormat> m_gBufferFormats;

    // Deferred lighting pass radiance textures
    // std::vector<SharedLib::GpuImg> m_lightingPassRadianceTextures;
    const VkFormat m_radianceTexturesFormat = VK_FORMAT_R32G32B32A32_SFLOAT;

    // Geo pass Gpu Rsrc inputs
    // NOTE: SSBO's entry needs to have size of the multiple of 2. HLSL always takes 4 or 2 elements per entry.
    std::vector<SharedLib::GpuBuffer> m_vpUboBuffers;

    SharedLib::GltfLoaderManager* m_pGltfLoaderManager;
    SharedLib::Level*             m_pLevel;

    PresentType m_presentType = PresentType::DIFFUSE;


    // Input Command Generator
    class CameraRotateCommandGenerator : public SharedLib::CommandGenerator
    {
    public:
        CameraRotateCommandGenerator() {}
        ~CameraRotateCommandGenerator() {}

        SharedLib::CustomizedCommand GenerateCommand(const std::vector<SharedLib::ImGuiInput> inputs) override
        {
            SharedLib::ImGuiInput mouseMoveInput = FindQualifiedInput(SharedLib::MOUSE_MOVE, inputs);
            SharedLib::CustomizedCommand cameraRotateCommand;
            cameraRotateCommand.m_commandTypeUID = m_cmdGenCmdTypeUID;
            cameraRotateCommand.m_payloadFloats = mouseMoveInput.GetFloats();
            return cameraRotateCommand;
        }
    };

    class CameraMoveForwardCommandGenerator : public SharedLib::CommandGenerator
    {
    public:
        CameraMoveForwardCommandGenerator() {}
        ~CameraMoveForwardCommandGenerator() {}

        SharedLib::CustomizedCommand GenerateCommand(const std::vector<SharedLib::ImGuiInput> inputs) override
        {
            SharedLib::CustomizedCommand cameraMoveForwardCommand;
            cameraMoveForwardCommand.m_commandTypeUID = m_cmdGenCmdTypeUID;
            return cameraMoveForwardCommand;
        }
    };

    class CameraMoveBackwardCommandGenerator : public SharedLib::CommandGenerator
    {
    public:
        CameraMoveBackwardCommandGenerator() {}
        ~CameraMoveBackwardCommandGenerator() {}

        SharedLib::CustomizedCommand GenerateCommand(const std::vector<SharedLib::ImGuiInput> inputs) override
        {
            SharedLib::CustomizedCommand cameraMoveBackwardCommand;
            cameraMoveBackwardCommand.m_commandTypeUID = m_cmdGenCmdTypeUID;
            return cameraMoveBackwardCommand;
        }
    };

    class CameraMoveLeftCommandGenerator : public SharedLib::CommandGenerator
    {
    public:
        CameraMoveLeftCommandGenerator() {}
        ~CameraMoveLeftCommandGenerator() {}

        SharedLib::CustomizedCommand GenerateCommand(const std::vector<SharedLib::ImGuiInput> inputs) override
        {
            SharedLib::CustomizedCommand cameraMoveLeftCommand;
            cameraMoveLeftCommand.m_commandTypeUID = m_cmdGenCmdTypeUID;
            return cameraMoveLeftCommand;
        }
    };

    class CameraMoveRightCommandGenerator : public SharedLib::CommandGenerator
    {
    public:
        CameraMoveRightCommandGenerator() {}
        ~CameraMoveRightCommandGenerator() {}

        SharedLib::CustomizedCommand GenerateCommand(const std::vector<SharedLib::ImGuiInput> inputs) override
        {
            SharedLib::CustomizedCommand cameraMoveRightCommand;
            cameraMoveRightCommand.m_commandTypeUID = m_cmdGenCmdTypeUID;
            return cameraMoveRightCommand;
        }
    };

    CameraRotateCommandGenerator       m_cameraRotateCmdGen;
    CameraMoveForwardCommandGenerator  m_cameraMoveForwardCmdGen;
    CameraMoveBackwardCommandGenerator m_cameraMoveBackwardCmdGen;
    CameraMoveLeftCommandGenerator     m_cameraMoveLeftCmdGen;
    CameraMoveRightCommandGenerator    m_cameraMoveRightCmdGen;
};
