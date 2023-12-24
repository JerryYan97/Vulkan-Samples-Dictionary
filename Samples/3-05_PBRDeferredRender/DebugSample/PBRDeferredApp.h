#pragma once
#include "../../../SharedLibrary/Application/GlfwApplication.h"
#include "../../../SharedLibrary/Pipeline/Pipeline.h"


VK_DEFINE_HANDLE(VmaAllocation);

namespace SharedLib
{
    class Camera;
}

constexpr uint32_t SphereCounts = 4 * 4;
constexpr uint32_t PtLightsCounts = 6 * 6;

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

    // The first layer is the desciptor ids.
    // The second layer is the bindings writes to that descriptor.
    std::vector<VkWriteDescriptorSet> GetGeoPassWriteDescriptorSets();

    VkPipeline GetGeoPassPipeline() { return m_geoPassPipeline.GetVkPipeline(); }

    uint32_t GetIdxCnt() { return m_idxData.size(); }

    VkBuffer GetIdxBuffer() { return m_idxBuffer.buffer; }
    VkBuffer GetVertBuffer() { return m_vertBuffer.buffer; }

    void CmdGBufferToRenderTarget(VkCommandBuffer cmdBuffer, VkImageLayout oldLayout);
    void CmdGBufferToShaderInput(VkCommandBuffer cmdBuffer, VkImageLayout oldLayout);
    std::vector<VkRenderingAttachmentInfoKHR> GetGBufferAttachments();

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

    // void Init

    // void InitFragUboObjects();
    // void DestroyFragUboObjects();

    VkPipelineVertexInputStateCreateInfo CreateGeoPassPipelineVertexInputInfo();
    VkPipelineDepthStencilStateCreateInfo CreateGeoPassDepthStencilStateInfo();
    std::vector<VkPipelineColorBlendAttachmentState> CreateGeoPassPipelineColorBlendAttachmentStates();

    SharedLib::Camera* m_pCamera;
    
    GpuBuffer m_lightPosStorageBuffer;
    GpuBuffer m_lightRadianceStorageBuffer;

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
