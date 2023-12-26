#pragma once
#include "../../../SharedLibrary/Application/GlfwApplication.h"
#include "../../../SharedLibrary/Pipeline/Pipeline.h"
#include <array>

VK_DEFINE_HANDLE(VmaAllocation);

namespace SharedLib
{
    class Camera;
}

constexpr uint32_t SphereCounts = 4 * 4;
constexpr uint32_t PtLightsCounts = 6 * 6 * 2; // Upper 36 point lights and bottom 36 point lights.

struct GpuBuffer
{
    VkBuffer               buffer;
    VmaAllocation          bufferAlloc;
    VkDescriptorBufferInfo bufferDescInfo;
};

struct GpuImg
{
    VkImage               image;
    VmaAllocation         imageAllocation;
    VkDescriptorImageInfo imageDescInfo;
    VkImageView           imageView;
    VkSampler             imageSampler;
};

class PBRDeferredApp : public SharedLib::GlfwApplication
{
public:
    PBRDeferredApp();
    ~PBRDeferredApp();

    virtual void AppInit() override;

    VkFence GetFence(uint32_t i) { return m_inFlightFences[i]; }

    VkPipelineLayout GetGeoPassPipelineLayout() { return m_geoPassPipelineLayout; }
    VkPipelineLayout GetDeferredLightingPassPipelineLayout() { return m_deferredLightingPassPipelineLayout; }

    // The first layer is the desciptor ids.
    // The second layer is the bindings writes to that descriptor.
    std::vector<VkWriteDescriptorSet> GetGeoPassWriteDescriptorSets();

    std::vector<VkWriteDescriptorSet> GetDeferredLightingWriteDescriptorSets();

    VkPipeline GetGeoPassPipeline() { return m_geoPassPipeline.GetVkPipeline(); }
    VkPipeline GetDeferredLightingPassPipeline() { return m_deferredLightingPassPipeline.GetVkPipeline(); }

    uint32_t GetIdxCnt() { return m_idxData.size(); }

    VkBuffer GetIdxBuffer() { return m_idxBuffer.buffer; }
    VkBuffer GetVertBuffer() { return m_vertBuffer.buffer; }

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

private:
    void InitGeoPassPipeline();
    void InitGeoPassPipelineDescriptorSetLayout();
    void InitGeoPassPipelineLayout();
    void InitGeoPassShaderModules();

    void InitOffsetSSBO();
    void InitAlbedoSSBO();
    void InitMetallicRoughnessSSBO();
    void DestroyGeoPassSSBOs();

    void InitGBuffer();
    void DestroyGBuffer();
    
    void InitSphereVertexIndexBuffers(); // Read in sphere data, create Sphere's GPU buffer objects and transfer data 
                                         // to the GPU buffers.
    void ReadInSphereData();
    void DestroySphereVertexIndexBuffers();

    void InitVpUboObjects();
    void DestroyVpUboObjects();

    void InitLightPosRadianceSSBOs();
    void DestroyLightPosRadianceSSBOs();

    float PtLightVolumeRadius(const std::array<float, 3>& radiance);

    void InitDeferredLightingPassPipeline();
    void InitDeferredLightingPassPipelineDescriptorSetLayout();
    void InitDeferredLightingPassPipelineLayout();
    void InitDeferredLightingPassShaderModules();

    // NOTE: Light volumes are also spheres.
    VkPipelineVertexInputStateCreateInfo CreateGeoPassPipelineVertexInputInfo();
    VkPipelineVertexInputStateCreateInfo CreateDeferredLightingPassPipelineVertexInputInfo();

    // The color is additive, which makes the depth stencil tricky. Each point lights should have their own depth buffer?
    // Not really... We can just use the face culling... Thus, we need two deferred lighting pipeline... One for face
    // culling and another for none culling...
    // We need to draw point lights one by one and check whether a point light includes the camera. If it includes the
    // camera then we need to disable the face culling of the point light volume.
    VkPipelineDepthStencilStateCreateInfo CreateGeoLightingPassDepthStencilStateInfo();

    SharedLib::PipelineColorBlendInfo CreateGeoPassPipelineColorBlendAttachmentStates();
    SharedLib::PipelineColorBlendInfo CreateDeferredLightingPassPipelineColorBlendAttachmentStates();

    void SendCameraDataToBuffer(uint32_t i);

    SharedLib::Camera* m_pCamera;
    
    GpuBuffer m_lightPosStorageBuffer;
    GpuBuffer m_lightRadianceStorageBuffer;
    GpuBuffer m_lightVolumeRadiusStorageBuffer;

    uint32_t  m_vertBufferByteCnt;
    uint32_t  m_idxBufferByteCnt;

    std::vector<float>    m_vertData;
    std::vector<uint32_t> m_idxData;

    GpuBuffer m_vertBuffer;
    GpuBuffer m_idxBuffer;

    // Geometry pass pipeline resources
    VkShaderModule        m_geoPassVsShaderModule;
    VkShaderModule        m_geoPassPsShaderModule;
    VkDescriptorSetLayout m_geoPassPipelineDesSetLayout;
    VkPipelineLayout      m_geoPassPipelineLayout;
    SharedLib::Pipeline   m_geoPassPipeline;

    // Deferred lighting pipeline resources
    VkShaderModule        m_deferredLightingPassVsShaderModule;
    VkShaderModule        m_deferredLightingPassPsShaderModule;
    VkDescriptorSetLayout m_deferredLightingPassPipelineDesSetLayout;
    VkPipelineLayout      m_deferredLightingPassPipelineLayout;
    SharedLib::Pipeline   m_deferredLightingPassPipeline;

    // G-Buffer textures
    std::vector<GpuImg> m_worldPosTextures;
    std::vector<GpuImg> m_albedoTextures;
    std::vector<GpuImg> m_normalTextures;
    std::vector<GpuImg> m_metallicRoughnessTextures;
    std::vector<VkFormat> m_gBufferFormats;

    // Geo pass Gpu Rsrc inputs
    // NOTE: SSBO needs to have size of the multiple of 2. HLSL always takes 4 or 2 elements per entry.
    std::vector<GpuBuffer> m_vpUboBuffers;
    GpuBuffer m_offsetStorageBuffer;
    GpuBuffer m_albedoStorageBuffer;
    GpuBuffer m_metallicRoughnessStorageBuffer;
};
