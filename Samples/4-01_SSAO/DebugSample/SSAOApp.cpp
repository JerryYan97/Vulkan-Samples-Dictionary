#include "SSAOApp.h"
#include <glfw3.h>
#include <cstdlib>
#include <math.h>
#include <algorithm>
#include "../../../SharedLibrary/Utils/VulkanDbgUtils.h"
#include "../../../SharedLibrary/Camera/Camera.h"
#include "../../../SharedLibrary/Event/Event.h"
#include "../../../SharedLibrary/AssetsLoader/AssetsLoader.h"
#include "../../../SharedLibrary/Scene/Level.h"

#include "vk_mem_alloc.h"

// TODO: These static variables and functions can be put into a header file so that other projects can reuse them.
static bool g_isMiddleDown = false;
static bool g_isWDown = false;
static bool g_isSDown = false;
static bool g_isADown = false;
static bool g_isDDown = false;

static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS)
    {
        g_isMiddleDown = true;
    }

    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_RELEASE)
    {
        g_isMiddleDown = false;
    }
}

static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_W && action == GLFW_PRESS)
    {
        g_isWDown = true;
    }

    if (key == GLFW_KEY_W && action == GLFW_RELEASE)
    {
        g_isWDown = false;
    }

    if (key == GLFW_KEY_S && action == GLFW_PRESS)
    {
        g_isSDown = true;
    }

    if (key == GLFW_KEY_S && action == GLFW_RELEASE)
    {
        g_isSDown = false;
    }

    if (key == GLFW_KEY_A && action == GLFW_PRESS)
    {
        g_isADown = true;
    }

    if (key == GLFW_KEY_A && action == GLFW_RELEASE)
    {
        g_isADown = false;
    }

    if (key == GLFW_KEY_D && action == GLFW_PRESS)
    {
        g_isDDown = true;
    }

    if (key == GLFW_KEY_D && action == GLFW_RELEASE)
    {
        g_isDDown = false;
    }
}

// ================================================================================================================
SSAOApp::SSAOApp() :
    ImGuiApplication(),
    m_geoPassVsShaderModule(VK_NULL_HANDLE),
    m_geoPassPsShaderModule(VK_NULL_HANDLE),
    m_geoPassPipelineDesSetLayout(VK_NULL_HANDLE),
    m_geoPassPipelineLayout(VK_NULL_HANDLE),
    m_geoPassPipeline(),
    // m_deferredLightingPassVsShaderModule(VK_NULL_HANDLE),
    // m_deferredLightingPassPsShaderModule(VK_NULL_HANDLE),
    // m_deferredLightingPassPipelineDesSetLayout(VK_NULL_HANDLE),
    // m_deferredLightingPassPipelineLayout(VK_NULL_HANDLE),
    // m_deferredLightingPassPipeline(),
    m_idxBuffer(),
    m_vertBuffer(),
    m_vertBufferByteCnt(0),
    m_idxBufferByteCnt(0)
{
    m_pCamera = new SharedLib::Camera();
}

// ================================================================================================================
SSAOApp::~SSAOApp()
{
    vkDeviceWaitIdle(m_device);
    delete m_pCamera;
    delete m_pLevel;

    m_pGltfLoaderManager->FinializeEntities(m_device, m_pAllocator);
    delete m_pGltfLoaderManager;

    // DestroyDeferredLightingPassRadianceTextures();

    DestroyVpUboObjects();
    DestroyGBuffer();

    // Destroy shader modules
    vkDestroyShaderModule(m_device, m_geoPassVsShaderModule, nullptr);
    vkDestroyShaderModule(m_device, m_geoPassPsShaderModule, nullptr);
    // vkDestroyShaderModule(m_device, m_deferredLightingPassVsShaderModule, nullptr);
    // vkDestroyShaderModule(m_device, m_deferredLightingPassPsShaderModule, nullptr);

    // Destroy the pipeline layout
    vkDestroyPipelineLayout(m_device, m_geoPassPipelineLayout, nullptr);
    // vkDestroyPipelineLayout(m_device, m_deferredLightingPassPipelineLayout, nullptr);

    // Destroy the descriptor set layout
    vkDestroyDescriptorSetLayout(m_device, m_geoPassPipelineDesSetLayout, nullptr);
    // vkDestroyDescriptorSetLayout(m_device, m_deferredLightingPassPipelineDesSetLayout, nullptr);
}

// ================================================================================================================
void SSAOApp::DestroyVpUboObjects()
{
    for (auto buffer : m_vpUboBuffers)
    {
        vmaDestroyBuffer(*m_pAllocator, buffer.buffer, buffer.bufferAlloc);
    }
}

// ================================================================================================================
void SSAOApp::InitVpUboObjects()
{
    float defaultPos[] = {-12.f, 0.f, 0.f};
    m_pCamera->SetPos(defaultPos);

    // The alignment of a vec3 is 4 floats and the element alignment of a struct is the largest element alignment,
    // which is also the 4 float. Therefore, we need 32 floats as the buffer to store the VP's parameters.
    VkBufferCreateInfo bufferInfo{};
    {
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = 16 * sizeof(float);
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo bufferAllocInfo{};
    {
        bufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        bufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
                                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    m_vpUboBuffers.resize(m_swapchainImgCnt);

    float vpMat[16] = {};
    float tmpViewMat[16] = {};
    float tmpPersMat[16] = {};
    m_pCamera->GenViewPerspectiveMatrices(tmpViewMat, tmpPersMat, vpMat);

    for (uint32_t i = 0; i < m_swapchainImgCnt; i++)
    {
        vmaCreateBuffer(*m_pAllocator,
                        &bufferInfo,
                        &bufferAllocInfo,
                        &m_vpUboBuffers[i].buffer,
                        &m_vpUboBuffers[i].bufferAlloc,
                        nullptr);

        CopyRamDataToGpuBuffer(vpMat,
                               m_vpUboBuffers[i].buffer,
                               m_vpUboBuffers[i].bufferAlloc,
                               16 * sizeof(float));

        // NOTE: For the push descriptors, the dstSet is ignored.
        //       This app doesn't have other resources so a fixed descriptor set is enough.
        {
            m_vpUboBuffers[i].bufferDescInfo.buffer = m_vpUboBuffers[i].buffer;
            m_vpUboBuffers[i].bufferDescInfo.offset = 0;
            m_vpUboBuffers[i].bufferDescInfo.range = sizeof(float) * 16;
        }
    }
}

// ================================================================================================================
void SSAOApp::SendCameraDataToBuffer(
    uint32_t i)
{
    float vpMat[16] = {};
    float tmpViewMat[16] = {};
    float tmpPersMat[16] = {};
    m_pCamera->GenViewPerspectiveMatrices(tmpViewMat, tmpPersMat, vpMat);

    CopyRamDataToGpuBuffer(vpMat,
                           m_vpUboBuffers[i].buffer,
                           m_vpUboBuffers[i].bufferAlloc,
                           16 * sizeof(float));
}

// ================================================================================================================
void SSAOApp::UpdateCameraAndGpuBuffer()
{
    SharedLib::HEvent midMouseDownEvent = CreateMiddleMouseEvent(g_isMiddleDown);
    m_pCamera->OnEvent(midMouseDownEvent);

    SharedLib::HEvent keyADownEvent = CreateKeyboardEvent(g_isADown, "KEY_A");
    m_pCamera->OnEvent(keyADownEvent);

    SharedLib::HEvent keyDDownEvent = CreateKeyboardEvent(g_isDDown, "KEY_D");
    m_pCamera->OnEvent(keyDDownEvent);

    SharedLib::HEvent keyWDownEvent = CreateKeyboardEvent(g_isWDown, "KEY_W");
    m_pCamera->OnEvent(keyWDownEvent);

    SharedLib::HEvent keySDownEvent = CreateKeyboardEvent(g_isSDown, "KEY_S");
    m_pCamera->OnEvent(keySDownEvent);

    // SendCameraDataToBuffer(m_currentFrame);
}

// ================================================================================================================
void SSAOApp::CmdGBufferLayoutTrans(
    VkCommandBuffer      cmdBuffer,
    VkImageLayout        oldLayout,
    VkImageLayout        newLayout,
    VkAccessFlags        srcAccessMask,
    VkAccessFlags        dstAccessMask,
    VkPipelineStageFlags srcStageMask,
    VkPipelineStageFlags dstStageMask)
{
    VkImageSubresourceRange colorOneMipOneLevelSubResRange{};
    {
        colorOneMipOneLevelSubResRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorOneMipOneLevelSubResRange.baseMipLevel = 0;
        colorOneMipOneLevelSubResRange.levelCount = 1;
        colorOneMipOneLevelSubResRange.baseArrayLayer = 0;
        colorOneMipOneLevelSubResRange.layerCount = 1;
    }

    std::vector<VkImageMemoryBarrier> gBufferToRenderTargetBarriers;

    // Transform the layout of the GBuffer textures from undefined to render target.
    VkImageMemoryBarrier gBufferRenderTargetTransBarrierTemplate{};
    {
        gBufferRenderTargetTransBarrierTemplate.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        gBufferRenderTargetTransBarrierTemplate.srcAccessMask = srcAccessMask;
        gBufferRenderTargetTransBarrierTemplate.dstAccessMask = dstAccessMask;
        gBufferRenderTargetTransBarrierTemplate.oldLayout = oldLayout;
        gBufferRenderTargetTransBarrierTemplate.newLayout = newLayout;
        gBufferRenderTargetTransBarrierTemplate.subresourceRange = colorOneMipOneLevelSubResRange;
    }

    /*
    gBufferRenderTargetTransBarrierTemplate.image = m_worldPosTextures[m_currentFrame].image;
    gBufferToRenderTargetBarriers.push_back(gBufferRenderTargetTransBarrierTemplate);

    gBufferRenderTargetTransBarrierTemplate.image = m_normalTextures[m_currentFrame].image;
    gBufferToRenderTargetBarriers.push_back(gBufferRenderTargetTransBarrierTemplate);

    gBufferRenderTargetTransBarrierTemplate.image = m_albedoTextures[m_currentFrame].image;
    gBufferToRenderTargetBarriers.push_back(gBufferRenderTargetTransBarrierTemplate);

    gBufferRenderTargetTransBarrierTemplate.image = m_metallicRoughnessTextures[m_currentFrame].image;
    gBufferToRenderTargetBarriers.push_back(gBufferRenderTargetTransBarrierTemplate);
    */

    vkCmdPipelineBarrier(
        cmdBuffer,
        srcStageMask,
        dstStageMask,
        0,
        0, nullptr,
        0, nullptr,
        gBufferToRenderTargetBarriers.size(),
        gBufferToRenderTargetBarriers.data());
}

// ================================================================================================================
std::vector<VkRenderingAttachmentInfoKHR> SSAOApp::GetGBufferAttachments()
{
    VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };

    std::vector<VkRenderingAttachmentInfoKHR> attachmentsInfos;

    VkRenderingAttachmentInfoKHR attachmentInfo{};
    {
        attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
        attachmentInfo.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
        attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachmentInfo.clearValue = clearColor;
    }

    // attachmentInfo.imageView = m_worldPosTextures[m_currentFrame].imageView;
    attachmentsInfos.push_back(attachmentInfo);

    // attachmentInfo.imageView = m_normalTextures[m_currentFrame].imageView;
    attachmentsInfos.push_back(attachmentInfo);

    // attachmentInfo.imageView = m_albedoTextures[m_currentFrame].imageView;
    attachmentsInfos.push_back(attachmentInfo);

    // attachmentInfo.imageView = m_metallicRoughnessTextures[m_currentFrame].imageView;
    attachmentsInfos.push_back(attachmentInfo);

    return attachmentsInfos;
}

// ================================================================================================================
std::vector<VkWriteDescriptorSet> SSAOApp::GetGeoPassWriteDescriptorSets()
{
    std::vector<VkWriteDescriptorSet> geoPassWriteDescSet;

    VkWriteDescriptorSet writeVpUboDesc{};
    {
        writeVpUboDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeVpUboDesc.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeVpUboDesc.descriptorCount = 1;
        writeVpUboDesc.dstBinding = 0;        
        // writeVpUboDesc.pBufferInfo = &m_vpUboBuffers[m_currentFrame].bufferDescInfo;
    }
    geoPassWriteDescSet.push_back(writeVpUboDesc);

    return geoPassWriteDescSet;
}

// ================================================================================================================
std::vector<VkWriteDescriptorSet> SSAOApp::GetDeferredLightingWriteDescriptorSets()
{
    std::vector<VkWriteDescriptorSet> writeDescriptorSet0;

    VkWriteDescriptorSet writeVpUboDesc{};
    {
        writeVpUboDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeVpUboDesc.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeVpUboDesc.descriptorCount = 1;
        writeVpUboDesc.dstBinding = 0;
        // writeVpUboDesc.pBufferInfo = &m_vpUboBuffers[m_currentFrame].bufferDescInfo;
    }
    writeDescriptorSet0.push_back(writeVpUboDesc);

    VkWriteDescriptorSet writeWorldPosTexDesc{};
    {
        writeWorldPosTexDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeWorldPosTexDesc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeWorldPosTexDesc.descriptorCount = 1;
        writeWorldPosTexDesc.dstBinding = 4;
        // writeWorldPosTexDesc.pImageInfo = &m_worldPosTextures[m_currentFrame].imageDescInfo;
    }
    writeDescriptorSet0.push_back(writeWorldPosTexDesc);

    VkWriteDescriptorSet writeNormalTexDesc{};
    {
        writeNormalTexDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeNormalTexDesc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeNormalTexDesc.descriptorCount = 1;
        writeNormalTexDesc.dstBinding = 5;
        // writeNormalTexDesc.pImageInfo = &m_normalTextures[m_currentFrame].imageDescInfo;
    }
    writeDescriptorSet0.push_back(writeNormalTexDesc);

    VkWriteDescriptorSet writeAlbedoTexDesc{};
    {
        writeAlbedoTexDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeAlbedoTexDesc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeAlbedoTexDesc.descriptorCount = 1;
        writeAlbedoTexDesc.dstBinding = 6;
        // writeAlbedoTexDesc.pImageInfo = &m_albedoTextures[m_currentFrame].imageDescInfo;
    }
    writeDescriptorSet0.push_back(writeAlbedoTexDesc);  

    VkWriteDescriptorSet writeMetallicRoughnessTexDesc{};
    {
        writeMetallicRoughnessTexDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeMetallicRoughnessTexDesc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeMetallicRoughnessTexDesc.descriptorCount = 1;
        writeMetallicRoughnessTexDesc.dstBinding = 7;
        // writeMetallicRoughnessTexDesc.pImageInfo = &m_metallicRoughnessTextures[m_currentFrame].imageDescInfo;
    }
    writeDescriptorSet0.push_back(writeMetallicRoughnessTexDesc);

    return writeDescriptorSet0;
}

// ================================================================================================================
SharedLib::PipelineColorBlendInfo SSAOApp::CreateDeferredLightingPassPipelineColorBlendAttachmentStates()
{
    SharedLib::PipelineColorBlendInfo pipelineColorBlendInfo{};

    pipelineColorBlendInfo.colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    pipelineColorBlendInfo.colorBlending.logicOpEnable = VK_FALSE;
    pipelineColorBlendInfo.colorBlending.logicOp = VK_LOGIC_OP_COPY;
    pipelineColorBlendInfo.colorBlending.blendConstants[0] = 0.0f;
    pipelineColorBlendInfo.colorBlending.blendConstants[1] = 0.0f;
    pipelineColorBlendInfo.colorBlending.blendConstants[2] = 0.0f;
    pipelineColorBlendInfo.colorBlending.blendConstants[3] = 0.0f;

    VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
    colorBlendAttachmentState.colorWriteMask = 0xf;
    colorBlendAttachmentState.blendEnable = VK_TRUE;
    colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;

    pipelineColorBlendInfo.colorBlendAttachments.push_back(colorBlendAttachmentState);

    return pipelineColorBlendInfo;
}

// ================================================================================================================
void SSAOApp::InitGeoPassPipelineLayout()
{
    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    {
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_geoPassPipelineDesSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
    }
    
    VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_geoPassPipelineLayout));
}

// ================================================================================================================
void SSAOApp::InitGeoPassShaderModules()
{
    // Create Shader Modules.
    m_geoPassVsShaderModule = CreateShaderModule("/hlsl/geo_vert.spv");
    m_geoPassPsShaderModule = CreateShaderModule("/hlsl/geo_frag.spv");
}

// ================================================================================================================
void SSAOApp::InitGeoPassPipelineDescriptorSetLayout()
{
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    // Create pipeline binding and descriptor objects for the camera parameters
    VkDescriptorSetLayoutBinding cameraUboBinding{};
    {
        cameraUboBinding.binding = 0;
        cameraUboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        cameraUboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        cameraUboBinding.descriptorCount = 1;
    }
    bindings.push_back(cameraUboBinding);

    // Binding for the spheres' offsets
    VkDescriptorSetLayoutBinding offsetsSSBOBinding{};
    {
        offsetsSSBOBinding.binding = 1;
        offsetsSSBOBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        offsetsSSBOBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        offsetsSSBOBinding.descriptorCount = 1;
    }
    bindings.push_back(offsetsSSBOBinding);

    // Binding for the spheres' albedos
    VkDescriptorSetLayoutBinding albedoSSBOBinding{};
    {
        albedoSSBOBinding.binding = 2;
        albedoSSBOBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        albedoSSBOBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        albedoSSBOBinding.descriptorCount = 1;
    }
    bindings.push_back(albedoSSBOBinding);

    // Binding for the spheres' metallic roughness material parameters
    VkDescriptorSetLayoutBinding metallicRoughnessSSBOBinding{};
    {
        metallicRoughnessSSBOBinding.binding = 3;
        metallicRoughnessSSBOBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        metallicRoughnessSSBOBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        metallicRoughnessSSBOBinding.descriptorCount = 1;
    }
    bindings.push_back(metallicRoughnessSSBOBinding);

    // Create pipeline's descriptors layout
    // The Vulkan spec states: The VkDescriptorSetLayoutBinding::binding members of the elements of the pBindings array 
    // must each have different values 
    // (https://vulkan.lunarg.com/doc/view/1.3.236.0/windows/1.3-extensions/vkspec.html#VUID-VkDescriptorSetLayoutCreateInfo-binding-00279)
    VkDescriptorSetLayoutCreateInfo pipelineDesSetLayoutInfo{};
    {
        pipelineDesSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        // Setting this flag tells the descriptor set layouts that no actual descriptor sets are allocated but instead pushed at command buffer creation time
        pipelineDesSetLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
        pipelineDesSetLayoutInfo.bindingCount = bindings.size();
        pipelineDesSetLayoutInfo.pBindings = bindings.data();
    }

    VK_CHECK(vkCreateDescriptorSetLayout(m_device,
                                         &pipelineDesSetLayoutInfo,
                                         nullptr,
                                         &m_geoPassPipelineDesSetLayout));
}

// ================================================================================================================
VkPipelineVertexInputStateCreateInfo SSAOApp::CreateGeoPassPipelineVertexInputInfo()
{
    // Specifying all kinds of pipeline states
    // Vertex input state
    VkVertexInputBindingDescription* pVertBindingDesc = new VkVertexInputBindingDescription();
    memset(pVertBindingDesc, 0, sizeof(VkVertexInputBindingDescription));
    {
        pVertBindingDesc->binding = 0;
        pVertBindingDesc->stride = 6 * sizeof(float);
        pVertBindingDesc->inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    }
    m_heapMemPtrVec.push_back(pVertBindingDesc);

    VkVertexInputAttributeDescription* pVertAttrDescs = new VkVertexInputAttributeDescription[2];
    memset(pVertAttrDescs, 0, sizeof(VkVertexInputAttributeDescription) * 2);
    {
        // Position
        pVertAttrDescs[0].location = 0;
        pVertAttrDescs[0].binding = 0;
        pVertAttrDescs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        pVertAttrDescs[0].offset = 0;
        // Normal
        pVertAttrDescs[1].location = 1;
        pVertAttrDescs[1].binding = 0;
        pVertAttrDescs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        pVertAttrDescs[1].offset = 3 * sizeof(float);
    }
    m_heapArrayMemPtrVec.push_back(pVertAttrDescs);

    VkPipelineVertexInputStateCreateInfo vertInputInfo{};
    {
        vertInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertInputInfo.pNext = nullptr;
        vertInputInfo.vertexBindingDescriptionCount = 1;
        vertInputInfo.pVertexBindingDescriptions = pVertBindingDesc;
        vertInputInfo.vertexAttributeDescriptionCount = 2;
        vertInputInfo.pVertexAttributeDescriptions = pVertAttrDescs;
    }

    return vertInputInfo;
}

// ================================================================================================================
// In the deferred lighting pass, we don't need the light volumes' normals.
VkPipelineVertexInputStateCreateInfo SSAOApp::CreateDeferredLightingPassPipelineVertexInputInfo()
{
    // Specifying all kinds of pipeline states
    // Vertex input state
    VkVertexInputBindingDescription* pVertBindingDesc = new VkVertexInputBindingDescription();
    memset(pVertBindingDesc, 0, sizeof(VkVertexInputBindingDescription));
    {
        pVertBindingDesc->binding = 0;
        pVertBindingDesc->stride = 6 * sizeof(float);
        pVertBindingDesc->inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    }
    m_heapMemPtrVec.push_back(pVertBindingDesc);

    VkVertexInputAttributeDescription* pVertAttrDescs = new VkVertexInputAttributeDescription();
    memset(pVertAttrDescs, 0, sizeof(VkVertexInputAttributeDescription));
    {
        // Position
        pVertAttrDescs[0].location = 0;
        pVertAttrDescs[0].binding = 0;
        pVertAttrDescs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        pVertAttrDescs[0].offset = 0;
    }
    m_heapArrayMemPtrVec.push_back(pVertAttrDescs);

    VkPipelineVertexInputStateCreateInfo vertInputInfo{};
    {
        vertInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertInputInfo.pNext = nullptr;
        vertInputInfo.vertexBindingDescriptionCount = 1;
        vertInputInfo.pVertexBindingDescriptions = pVertBindingDesc;
        vertInputInfo.vertexAttributeDescriptionCount = 1;
        vertInputInfo.pVertexAttributeDescriptions = pVertAttrDescs;
    }

    return vertInputInfo;
}

// ================================================================================================================
VkPipelineDepthStencilStateCreateInfo SSAOApp::CreateGeoPassDepthStencilStateInfo()
{
    VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};
    {
        depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencilInfo.depthTestEnable = VK_TRUE;
        depthStencilInfo.depthWriteEnable = VK_TRUE;
        depthStencilInfo.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL; // Reverse depth for higher precision. 
        depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
        depthStencilInfo.stencilTestEnable = VK_FALSE;
    }

    return depthStencilInfo;
}

// ================================================================================================================
VkPipelineDepthStencilStateCreateInfo SSAOApp::CreateDeferredLightingPassDepthStencilStateInfo()
{
    VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};
    {
        depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        // depthStencilInfo.depthTestEnable = VK_TRUE;
        // depthStencilInfo.depthWriteEnable = VK_TRUE;
        // depthStencilInfo.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL; // Reverse depth for higher precision. 
        // depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
        // depthStencilInfo.stencilTestEnable = VK_FALSE;
    }

    return depthStencilInfo;
}

// ================================================================================================================
SharedLib::PipelineColorBlendInfo SSAOApp::CreateGeoPassPipelineColorBlendAttachmentStates()
{
    SharedLib::PipelineColorBlendInfo pipelineColorBlendInfo{};

    pipelineColorBlendInfo.colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    pipelineColorBlendInfo.colorBlending.logicOpEnable = VK_FALSE;
    pipelineColorBlendInfo.colorBlending.logicOp = VK_LOGIC_OP_COPY;
    pipelineColorBlendInfo.colorBlending.blendConstants[0] = 0.0f;
    pipelineColorBlendInfo.colorBlending.blendConstants[1] = 0.0f;
    pipelineColorBlendInfo.colorBlending.blendConstants[2] = 0.0f;
    pipelineColorBlendInfo.colorBlending.blendConstants[3] = 0.0f;

    VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
    colorBlendAttachmentState.colorWriteMask = 0xf;
    colorBlendAttachmentState.blendEnable = VK_FALSE;

    pipelineColorBlendInfo.colorBlendAttachments.push_back(colorBlendAttachmentState);
    pipelineColorBlendInfo.colorBlendAttachments.push_back(colorBlendAttachmentState);
    pipelineColorBlendInfo.colorBlendAttachments.push_back(colorBlendAttachmentState);
    pipelineColorBlendInfo.colorBlendAttachments.push_back(colorBlendAttachmentState);

    return pipelineColorBlendInfo;
}

// ================================================================================================================
void SSAOApp::InitGeoPassPipeline()
{
    VkPipelineRenderingCreateInfoKHR pipelineRenderCreateInfo{};
    {
        pipelineRenderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        pipelineRenderCreateInfo.colorAttachmentCount = m_gBufferFormats.size();
        pipelineRenderCreateInfo.pColorAttachmentFormats = m_gBufferFormats.data();
        pipelineRenderCreateInfo.depthAttachmentFormat = VK_FORMAT_D16_UNORM;
    }

    m_geoPassPipeline.SetPNext(&pipelineRenderCreateInfo);
    m_geoPassPipeline.SetPipelineLayout(m_geoPassPipelineLayout);

    VkPipelineVertexInputStateCreateInfo vertInputInfo = CreateGeoPassPipelineVertexInputInfo();
    m_geoPassPipeline.SetVertexInputInfo(&vertInputInfo);

    VkPipelineDepthStencilStateCreateInfo depthStencilInfo = CreateGeoPassDepthStencilStateInfo();
    m_geoPassPipeline.SetDepthStencilStateInfo(&depthStencilInfo);

    SharedLib::PipelineColorBlendInfo colorBlendInfo = CreateGeoPassPipelineColorBlendAttachmentStates();
    m_geoPassPipeline.SetPipelineColorBlendInfo(colorBlendInfo);

    VkPipelineShaderStageCreateInfo shaderStgsInfo[2] = {};
    shaderStgsInfo[0] = CreateDefaultShaderStgCreateInfo(m_geoPassVsShaderModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStgsInfo[1] = CreateDefaultShaderStgCreateInfo(m_geoPassPsShaderModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    m_geoPassPipeline.SetShaderStageInfo(shaderStgsInfo, 2);

    m_geoPassPipeline.CreatePipeline(m_device);
}

// ================================================================================================================
std::vector<float> SSAOApp::GetDeferredLightingPushConstantData()
{
    std::vector<float> data;

    // Camera pos:
    float cameraPos[3];
    m_pCamera->GetPos(cameraPos);
    
    data.push_back(cameraPos[0]);
    data.push_back(cameraPos[1]);
    data.push_back(cameraPos[2]);

    VkExtent2D extent = GetSwapchainImageExtent();
    data.push_back(extent.width);
    data.push_back(extent.height);

    return data;
}

// ================================================================================================================
void SSAOApp::InitGBuffer()
{
    // It looks like AMD doesn't support the R32G32B32_SFLOAT image format.
    const VkFormat PosNormalAlbedoImgFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
    const VkFormat MetallicRoughnessImgFormat = VK_FORMAT_R32G32_SFLOAT;

    VkImageCreateInfo posNormalAlbedoImageInfo{};
    {
        posNormalAlbedoImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        posNormalAlbedoImageInfo.imageType = VK_IMAGE_TYPE_2D;
        posNormalAlbedoImageInfo.format = PosNormalAlbedoImgFormat;
        posNormalAlbedoImageInfo.extent.width = m_swapchainImageExtent.width;
        posNormalAlbedoImageInfo.extent.height = m_swapchainImageExtent.height;
        posNormalAlbedoImageInfo.extent.depth = 1;
        posNormalAlbedoImageInfo.mipLevels = 1;
        posNormalAlbedoImageInfo.arrayLayers = 1;
        posNormalAlbedoImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        posNormalAlbedoImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        posNormalAlbedoImageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        posNormalAlbedoImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        posNormalAlbedoImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    }

    VkImageCreateInfo metallicRoughnessImageInfo = posNormalAlbedoImageInfo;
    metallicRoughnessImageInfo.format = MetallicRoughnessImgFormat;

    VmaAllocationCreateInfo gBufferImgAllocInfo{};
    {
        gBufferImgAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        gBufferImgAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    VkImageSubresourceRange oneMipOneLayerSubRsrcRange{};
    {
        oneMipOneLayerSubRsrcRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        oneMipOneLayerSubRsrcRange.baseMipLevel = 0;
        oneMipOneLayerSubRsrcRange.levelCount = 1;
        oneMipOneLayerSubRsrcRange.baseArrayLayer = 0;
        oneMipOneLayerSubRsrcRange.layerCount = 1;
    }

    VkImageViewCreateInfo posNormalAlbedoImageViewInfo{};
    {
        posNormalAlbedoImageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        // posNormalAlbedoImageViewInfo.image = colorImage;
        posNormalAlbedoImageViewInfo.format = PosNormalAlbedoImgFormat;
        posNormalAlbedoImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        posNormalAlbedoImageViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        posNormalAlbedoImageViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        posNormalAlbedoImageViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        posNormalAlbedoImageViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        posNormalAlbedoImageViewInfo.subresourceRange = oneMipOneLayerSubRsrcRange;
    }

    VkImageViewCreateInfo metallicRoughnessImageViewInfo = posNormalAlbedoImageViewInfo;
    metallicRoughnessImageViewInfo.format = MetallicRoughnessImgFormat;

    VkSamplerCreateInfo samplerInfo{};
    {
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; // outside image bounds just use border color
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.minLod = -1000;
        samplerInfo.maxLod = 1000;
        samplerInfo.maxAnisotropy = 1.0f;
    }

    m_gBufferFormats.push_back(PosNormalAlbedoImgFormat);
    m_gBufferFormats.push_back(PosNormalAlbedoImgFormat);
    m_gBufferFormats.push_back(PosNormalAlbedoImgFormat);
    m_gBufferFormats.push_back(MetallicRoughnessImgFormat);

    m_worldPosTextures.resize(m_swapchainImgCnt);
    m_normalTextures.resize(m_swapchainImgCnt);
    m_albedoTextures.resize(m_swapchainImgCnt);
    m_metallicRoughnessTextures.resize(m_swapchainImgCnt);

    for (uint32_t i = 0; i < m_swapchainImgCnt; i++)
    {
        vmaCreateImage(*m_pAllocator,
                       &posNormalAlbedoImageInfo,
                       &gBufferImgAllocInfo,
                       &m_worldPosTextures[i].image,
                       &m_worldPosTextures[i].imageAllocation, nullptr);

        vmaCreateImage(*m_pAllocator,
                       &posNormalAlbedoImageInfo,
                       &gBufferImgAllocInfo,
                       &m_normalTextures[i].image,
                       &m_normalTextures[i].imageAllocation, nullptr);

        vmaCreateImage(*m_pAllocator,
                       &posNormalAlbedoImageInfo,
                       &gBufferImgAllocInfo,
                       &m_albedoTextures[i].image,
                       &m_albedoTextures[i].imageAllocation, nullptr);

        vmaCreateImage(*m_pAllocator,
                       &metallicRoughnessImageInfo,
                       &gBufferImgAllocInfo,
                       &m_metallicRoughnessTextures[i].image,
                       &m_metallicRoughnessTextures[i].imageAllocation, nullptr);
        
        posNormalAlbedoImageViewInfo.image = m_worldPosTextures[i].image;
        VK_CHECK(vkCreateImageView(m_device, &posNormalAlbedoImageViewInfo, nullptr, &m_worldPosTextures[i].imageView));

        posNormalAlbedoImageViewInfo.image = m_normalTextures[i].image;
        VK_CHECK(vkCreateImageView(m_device, &posNormalAlbedoImageViewInfo, nullptr, &m_normalTextures[i].imageView));

        posNormalAlbedoImageViewInfo.image = m_albedoTextures[i].image;
        VK_CHECK(vkCreateImageView(m_device, &posNormalAlbedoImageViewInfo, nullptr, &m_albedoTextures[i].imageView));

        metallicRoughnessImageViewInfo.image = m_metallicRoughnessTextures[i].image;
        VK_CHECK(vkCreateImageView(m_device, &metallicRoughnessImageViewInfo, nullptr, &m_metallicRoughnessTextures[i].imageView));

        VK_CHECK(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_worldPosTextures[i].imageSampler));
        VK_CHECK(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_normalTextures[i].imageSampler));
        VK_CHECK(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_albedoTextures[i].imageSampler));
        VK_CHECK(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_metallicRoughnessTextures[i].imageSampler));

        m_worldPosTextures[i].imageDescInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        m_worldPosTextures[i].imageDescInfo.imageView = m_worldPosTextures[i].imageView;
        m_worldPosTextures[i].imageDescInfo.sampler = m_worldPosTextures[i].imageSampler;

        m_normalTextures[i].imageDescInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        m_normalTextures[i].imageDescInfo.imageView = m_normalTextures[i].imageView;
        m_normalTextures[i].imageDescInfo.sampler = m_normalTextures[i].imageSampler;

        m_albedoTextures[i].imageDescInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        m_albedoTextures[i].imageDescInfo.imageView = m_albedoTextures[i].imageView;
        m_albedoTextures[i].imageDescInfo.sampler = m_albedoTextures[i].imageSampler;

        m_metallicRoughnessTextures[i].imageDescInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        m_metallicRoughnessTextures[i].imageDescInfo.imageView = m_metallicRoughnessTextures[i].imageView;
        m_metallicRoughnessTextures[i].imageDescInfo.sampler = m_metallicRoughnessTextures[i].imageSampler;
    }
}

// ================================================================================================================
void SSAOApp::DestroyGBuffer()
{
    for (uint32_t i = 0; i < m_swapchainImgCnt; i++)
    {
        vmaDestroyImage(*m_pAllocator, m_worldPosTextures[i].image, m_worldPosTextures[i].imageAllocation);
        vmaDestroyImage(*m_pAllocator, m_normalTextures[i].image, m_normalTextures[i].imageAllocation);
        vmaDestroyImage(*m_pAllocator, m_albedoTextures[i].image, m_albedoTextures[i].imageAllocation);
        vmaDestroyImage(*m_pAllocator, m_metallicRoughnessTextures[i].image, m_metallicRoughnessTextures[i].imageAllocation);

        vkDestroyImageView(m_device, m_worldPosTextures[i].imageView, nullptr);
        vkDestroyImageView(m_device, m_normalTextures[i].imageView, nullptr);
        vkDestroyImageView(m_device, m_albedoTextures[i].imageView, nullptr);
        vkDestroyImageView(m_device, m_metallicRoughnessTextures[i].imageView, nullptr);

        vkDestroySampler(m_device, m_worldPosTextures[i].imageSampler, nullptr);
        vkDestroySampler(m_device, m_normalTextures[i].imageSampler, nullptr);
        vkDestroySampler(m_device, m_albedoTextures[i].imageSampler, nullptr);
        vkDestroySampler(m_device, m_metallicRoughnessTextures[i].imageSampler, nullptr);
    }
}

// ================================================================================================================
VkPipelineRasterizationStateCreateInfo SSAOApp::CreateDeferredLightingPassDisableCullingRasterizationInfoStateInfo()
{
    VkPipelineRasterizationStateCreateInfo rasterizationStateInfo{};
    {
        rasterizationStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizationStateInfo.depthClampEnable = VK_FALSE;
        rasterizationStateInfo.rasterizerDiscardEnable = VK_FALSE;
        rasterizationStateInfo.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizationStateInfo.lineWidth = 1.0f;
        rasterizationStateInfo.cullMode = VK_CULL_MODE_NONE;
        rasterizationStateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizationStateInfo.depthBiasEnable = VK_FALSE;
    }

    return rasterizationStateInfo;
}

/*
// ================================================================================================================
void SSAOApp::InitDeferredLightingPassPipeline()
{
    VkPipelineRenderingCreateInfoKHR pipelineRenderCreateInfo{};
    {
        pipelineRenderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        pipelineRenderCreateInfo.colorAttachmentCount = 1;
        pipelineRenderCreateInfo.pColorAttachmentFormats = &m_radianceTexturesFormat;
        pipelineRenderCreateInfo.depthAttachmentFormat = VK_FORMAT_D16_UNORM;
    }

    m_deferredLightingPassPipeline.SetPNext(&pipelineRenderCreateInfo);
    m_deferredLightingPassPipeline.SetPipelineLayout(m_deferredLightingPassPipelineLayout);

    VkPipelineVertexInputStateCreateInfo vertInputInfo = CreateDeferredLightingPassPipelineVertexInputInfo();
    m_deferredLightingPassPipeline.SetVertexInputInfo(&vertInputInfo);

    VkPipelineDepthStencilStateCreateInfo depthStencilInfo = CreateDeferredLightingPassDepthStencilStateInfo();
    m_deferredLightingPassPipeline.SetDepthStencilStateInfo(&depthStencilInfo);

    SharedLib::PipelineColorBlendInfo colorBlendInfo = CreateDeferredLightingPassPipelineColorBlendAttachmentStates();
    m_deferredLightingPassPipeline.SetPipelineColorBlendInfo(colorBlendInfo);

    VkPipelineShaderStageCreateInfo shaderStgsInfo[2] = {};
    shaderStgsInfo[0] = CreateDefaultShaderStgCreateInfo(m_deferredLightingPassVsShaderModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStgsInfo[1] = CreateDefaultShaderStgCreateInfo(m_deferredLightingPassPsShaderModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    m_deferredLightingPassPipeline.SetShaderStageInfo(shaderStgsInfo, 2);

    m_deferredLightingPassPipeline.CreatePipeline(m_device);

    // When the camera is in the light volume, we need to disable the culling of the pipeline.
    m_deferredLightingPassDisableCullingPipeline.SetPNext(&pipelineRenderCreateInfo);
    m_deferredLightingPassDisableCullingPipeline.SetPipelineLayout(m_deferredLightingPassPipelineLayout);
    m_deferredLightingPassDisableCullingPipeline.SetVertexInputInfo(&vertInputInfo);
    m_deferredLightingPassDisableCullingPipeline.SetDepthStencilStateInfo(&depthStencilInfo);
    m_deferredLightingPassDisableCullingPipeline.SetPipelineColorBlendInfo(colorBlendInfo);
    m_deferredLightingPassDisableCullingPipeline.SetShaderStageInfo(shaderStgsInfo, 2);

    VkPipelineRasterizationStateCreateInfo rasterizationInfo = CreateDeferredLightingPassDisableCullingRasterizationInfoStateInfo();
    m_deferredLightingPassDisableCullingPipeline.SetRasterizerInfo(&rasterizationInfo);

    m_deferredLightingPassDisableCullingPipeline.CreatePipeline(m_device);
}

// ================================================================================================================
void SSAOApp::InitDeferredLightingPassPipelineDescriptorSetLayout()
{
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    // Create pipeline binding and descriptor objects for the camera parameters
    VkDescriptorSetLayoutBinding cameraUboBinding{};
    {
        cameraUboBinding.binding = 0;
        cameraUboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        cameraUboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        cameraUboBinding.descriptorCount = 1;
    }
    bindings.push_back(cameraUboBinding);

    // Binding for the point lights' positions
    VkDescriptorSetLayoutBinding lightPosSSBOBinding{};
    {
        lightPosSSBOBinding.binding = 1;
        lightPosSSBOBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        lightPosSSBOBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        lightPosSSBOBinding.descriptorCount = 1;
    }
    bindings.push_back(lightPosSSBOBinding);

    // Binding for the point lights volumes' radius.
    VkDescriptorSetLayoutBinding lightVolumeRadiusSSBOBinding{};
    {
        lightVolumeRadiusSSBOBinding.binding = 2;
        lightVolumeRadiusSSBOBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        lightVolumeRadiusSSBOBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        lightVolumeRadiusSSBOBinding.descriptorCount = 1;
    }
    bindings.push_back(lightVolumeRadiusSSBOBinding);

    // Binding for the point lights radiance.
    VkDescriptorSetLayoutBinding lightRadianceSSBOBinding{};
    {
        lightRadianceSSBOBinding.binding = 3;
        lightRadianceSSBOBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        lightRadianceSSBOBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        lightRadianceSSBOBinding.descriptorCount = 1;
    }
    bindings.push_back(lightRadianceSSBOBinding);

    VkDescriptorSetLayoutBinding worldPosTexBinding{};
    {
        worldPosTexBinding.binding = 4;
        worldPosTexBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        worldPosTexBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        worldPosTexBinding.descriptorCount = 1;
    }
    bindings.push_back(worldPosTexBinding);

    VkDescriptorSetLayoutBinding worldNormalTexBinding{};
    {
        worldNormalTexBinding.binding = 5;
        worldNormalTexBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        worldNormalTexBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        worldNormalTexBinding.descriptorCount = 1;
    }
    bindings.push_back(worldNormalTexBinding);

    VkDescriptorSetLayoutBinding albedoTexBinding{};
    {
        albedoTexBinding.binding = 6;
        albedoTexBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        albedoTexBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        albedoTexBinding.descriptorCount = 1;
    }
    bindings.push_back(albedoTexBinding);

    VkDescriptorSetLayoutBinding metallicRoughnessTexBinding{};
    {
        metallicRoughnessTexBinding.binding = 7;
        metallicRoughnessTexBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        metallicRoughnessTexBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        metallicRoughnessTexBinding.descriptorCount = 1;
    }
    bindings.push_back(metallicRoughnessTexBinding);

    // Create pipeline's descriptors layout
    // The Vulkan spec states: The VkDescriptorSetLayoutBinding::binding members of the elements of the pBindings array 
    // must each have different values 
    // (https://vulkan.lunarg.com/doc/view/1.3.236.0/windows/1.3-extensions/vkspec.html#VUID-VkDescriptorSetLayoutCreateInfo-binding-00279)
    VkDescriptorSetLayoutCreateInfo pipelineDesSetLayoutInfo{};
    {
        pipelineDesSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        // Setting this flag tells the descriptor set layouts that no actual descriptor sets are allocated but instead pushed at command buffer creation time
        pipelineDesSetLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
        pipelineDesSetLayoutInfo.bindingCount = bindings.size();
        pipelineDesSetLayoutInfo.pBindings = bindings.data();
    }

    VK_CHECK(vkCreateDescriptorSetLayout(m_device,
                                         &pipelineDesSetLayoutInfo,
                                         nullptr,
                                         &m_deferredLightingPassPipelineDesSetLayout));
}

// ================================================================================================================
void SSAOApp::InitDeferredLightingPassPipelineLayout()
{
    // Create pipeline layout
    std::vector<float> pushConstantData = GetDeferredLightingPushConstantData();

    VkPushConstantRange pushConstantInfo{};
    {
        pushConstantInfo.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantInfo.offset = 0;
        pushConstantInfo.size = sizeof(float) * pushConstantData.size();
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    {
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_deferredLightingPassPipelineDesSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantInfo;
    }

    VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_deferredLightingPassPipelineLayout));
}

// ================================================================================================================
bool SSAOApp::IsCameraInThisLight(
    uint32_t lightIdx)
{
    float cameraPos[3] = {};
    m_pCamera->GetPos(cameraPos);

    float lightX = m_lightsPos[lightIdx * 4];
    float lightY = m_lightsPos[lightIdx * 4 + 1];
    float lightZ = m_lightsPos[lightIdx * 4 + 2];

    float offsetX = lightX - cameraPos[0];
    float offsetY = lightY - cameraPos[1];
    float offsetZ = lightZ - cameraPos[2];

    float distSquare = offsetX * offsetX + offsetY * offsetY + offsetZ * offsetZ;

    float lightRadiusSquare = m_lightsRadius[lightIdx];
    lightRadiusSquare *= lightRadiusSquare;

    return (lightRadiusSquare > distSquare);
}

// ================================================================================================================
void SSAOApp::InitDeferredLightingPassShaderModules()
{
    // Create Shader Modules.
    m_deferredLightingPassVsShaderModule = CreateShaderModule("/hlsl/lighting_vert.spv");
    m_deferredLightingPassPsShaderModule = CreateShaderModule("/hlsl/lighting_frag.spv");
}

// ================================================================================================================
void SSAOApp::InitDeferredLightingPassRadianceTextures()
{
    m_lightingPassRadianceTextures.resize(m_swapchainImgCnt);
    
    VkImageCreateInfo radianceImageInfo{};
    {
        radianceImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        radianceImageInfo.imageType = VK_IMAGE_TYPE_2D;
        radianceImageInfo.format = m_radianceTexturesFormat;
        radianceImageInfo.extent.width = m_swapchainImageExtent.width;
        radianceImageInfo.extent.height = m_swapchainImageExtent.height;
        radianceImageInfo.extent.depth = 1;
        radianceImageInfo.mipLevels = 1;
        radianceImageInfo.arrayLayers = 1;
        radianceImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        radianceImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        radianceImageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        radianceImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        radianceImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    }

    VmaAllocationCreateInfo imgAllocInfo{};
    {
        imgAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        imgAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    VkImageSubresourceRange oneMipOneLayerSubRsrcRange{};
    {
        oneMipOneLayerSubRsrcRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        oneMipOneLayerSubRsrcRange.baseMipLevel = 0;
        oneMipOneLayerSubRsrcRange.levelCount = 1;
        oneMipOneLayerSubRsrcRange.baseArrayLayer = 0;
        oneMipOneLayerSubRsrcRange.layerCount = 1;
    }

    VkImageViewCreateInfo imageViewInfo{};
    {
        imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        // imageViewInfo.image = ;
        imageViewInfo.format = m_radianceTexturesFormat;
        imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        imageViewInfo.subresourceRange = oneMipOneLayerSubRsrcRange;
    }

    VkSamplerCreateInfo samplerInfo{};
    {
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; // outside image bounds just use border color
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.minLod = -1000;
        samplerInfo.maxLod = 1000;
        samplerInfo.maxAnisotropy = 1.0f;
    }

    for (uint32_t i = 0; i < m_swapchainImgCnt; i++)
    {
        vmaCreateImage(*m_pAllocator,
                       &radianceImageInfo,
                       &imgAllocInfo,
                       &m_lightingPassRadianceTextures[i].image,
                       &m_lightingPassRadianceTextures[i].imageAllocation, nullptr);

        imageViewInfo.image = m_lightingPassRadianceTextures[i].image;
        VK_CHECK(vkCreateImageView(m_device, &imageViewInfo, nullptr, &m_lightingPassRadianceTextures[i].imageView));

        VK_CHECK(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_lightingPassRadianceTextures[i].imageSampler));
    }
}

// ================================================================================================================
void SSAOApp::DestroyDeferredLightingPassRadianceTextures()
{
    for (uint32_t i = 0; i < m_swapchainImgCnt; i++)
    {
        vkDestroyImageView(m_device, m_lightingPassRadianceTextures[i].imageView, nullptr);
        vkDestroySampler(m_device, m_lightingPassRadianceTextures[i].imageSampler, nullptr);

        vmaDestroyImage(*m_pAllocator,
                        m_lightingPassRadianceTextures[i].image,
                        m_lightingPassRadianceTextures[i].imageAllocation);
    }
}
*/

// ================================================================================================================
void SSAOApp::CmdGeoPass(VkCommandBuffer cmdBuffer)
{
    // Loop through all the meshes in the scene and render them to G-Buffer.
    
}

// ================================================================================================================
void SSAOApp::CmdSSAOAppMultiTypeRendering(VkCommandBuffer cmdBuffer)
{
    // Only direct light and ambient light.

}

// ================================================================================================================
void SSAOApp::ImGuiFrame()
{

}

// ================================================================================================================
void SSAOApp::AppInit()
{
    glfwInit();
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> instExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    InitInstance(instExtensions, glfwExtensionCount);

    // Init glfw window.
    InitGlfwWindowAndCallbacks();
    glfwSetMouseButtonCallback(m_pWindow, MouseButtonCallback);
    glfwSetKeyCallback(m_pWindow, KeyCallback);

    // Create vulkan surface from the glfw window.
    VK_CHECK(glfwCreateWindowSurface(m_instance, m_pWindow, nullptr, &m_surface));

    InitPhysicalDevice();
    InitGfxQueueFamilyIdx();
    InitPresentQueueFamilyIdx();

    // Queue family index should be unique in vk1.2:
    // https://vulkan.lunarg.com/doc/view/1.2.198.0/windows/1.2-extensions/vkspec.html#VUID-VkDeviceCreateInfo-queueFamilyIndex-02802
    std::vector<VkDeviceQueueCreateInfo> deviceQueueInfos = CreateDeviceQueueInfos({ m_graphicsQueueFamilyIdx,
                                                                                     m_presentQueueFamilyIdx });
    // Dummy device extensions vector. Swapchain, dynamic rendering and push descriptors are enabled by default.
    // We have tools that don't need the swapchain extension and the swapchain extension requires surface instance extensions.
    const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    InitDevice(deviceExtensions, deviceQueueInfos, nullptr);
    InitKHRFuncPtrs();
    InitVmaAllocator();
    InitGraphicsQueue();
    InitPresentQueue();

    InitGfxCommandPool();
    // InitGfxCommandBuffers(SharedLib::MAX_FRAMES_IN_FLIGHT);

    InitSwapchain();

    InitVpUboObjects();
    
    // InitAlbedoSSBO();
    // InitMetallicRoughnessSSBO();
    InitGBuffer();
    // InitLightPosRadianceSSBOs();

    // Load in gltf scene.
    m_pGltfLoaderManager = new SharedLib::GltfLoaderManager();
    m_pLevel = new SharedLib::Level();

    std::string sceneLoadPathAbs = SOURCE_PATH;
    sceneLoadPathAbs += +"/../data/Sponza/Sponza.gltf";

    m_pGltfLoaderManager->Load(sceneLoadPathAbs, *m_pLevel);
    m_pGltfLoaderManager->InitEntitesGpuRsrc(m_device, m_pAllocator);

    /*
    InitGeoPassShaderModules();
    InitGeoPassPipelineDescriptorSetLayout();
    InitGeoPassPipelineLayout();
    InitGeoPassPipeline();
    */

    /*
    InitDeferredLightingPassShaderModules();
    InitDeferredLightingPassPipelineDescriptorSetLayout();
    InitDeferredLightingPassPipelineLayout();
    InitDeferredLightingPassPipeline();
    InitDeferredLightingPassRadianceTextures();
    */

    InitSwapchainSyncObjects();
    InitGammaCorrectionPipelineAndRsrc();
}