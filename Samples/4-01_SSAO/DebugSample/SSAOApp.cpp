#include "SSAOApp.h"
#include <glfw3.h>
#include <cstdlib>
#include <math.h>
#include <algorithm>
#include <chrono>
#include "../../../ThirdPartyLibs/DearImGUI/imgui.h"
#include "../../../ThirdPartyLibs/DearImGUI/backends/imgui_impl_glfw.h"
#include "../../../ThirdPartyLibs/DearImGUI/backends/imgui_impl_vulkan.h"
#include "../../../SharedLibrary/Utils/VulkanDbgUtils.h"
#include "../../../SharedLibrary/Camera/Camera.h"
#include "../../../SharedLibrary/Event/Event.h"
#include "../../../SharedLibrary/AssetsLoader/AssetsLoader.h"
#include "../../../SharedLibrary/Scene/Level.h"

#include "vk_mem_alloc.h"

#define CUSTOM_DEBUGGING 1

// ================================================================================================================
SSAOApp::SSAOApp() :
    ImGuiApplication(),
    m_geoPassVsShaderModule(VK_NULL_HANDLE),
    m_geoPassPsShaderModule(VK_NULL_HANDLE),
    m_geoPassPipelineDesSetLayout(VK_NULL_HANDLE),
    m_geoPassPipelineLayout(VK_NULL_HANDLE),
    m_geoPassPipeline(),
    m_screenQuadVsShaderModule(VK_NULL_HANDLE),
    m_albedoRenderingPsShaderModule(VK_NULL_HANDLE),
    m_albedoRenderingPipelineDesSetLayout(VK_NULL_HANDLE),
    m_albedoRenderingPipelineLayout(VK_NULL_HANDLE),
    m_albedoRenderingPipeline(),
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
    vkDestroyShaderModule(m_device, m_screenQuadVsShaderModule, nullptr);
    vkDestroyShaderModule(m_device, m_albedoRenderingPsShaderModule, nullptr);
    // vkDestroyShaderModule(m_device, m_deferredLightingPassVsShaderModule, nullptr);
    // vkDestroyShaderModule(m_device, m_deferredLightingPassPsShaderModule, nullptr);

    // Destroy the pipeline layout
    vkDestroyPipelineLayout(m_device, m_geoPassPipelineLayout, nullptr);
    vkDestroyPipelineLayout(m_device, m_albedoRenderingPipelineLayout, nullptr);
    // vkDestroyPipelineLayout(m_device, m_deferredLightingPassPipelineLayout, nullptr);

    // Destroy the descriptor set layout
    vkDestroyDescriptorSetLayout(m_device, m_geoPassPipelineDesSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_albedoRenderingPipelineDesSetLayout, nullptr);
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
    float defaultPos[] = {0.f, 1.f, 0.f};
    m_pCamera->SetPos(defaultPos);
    m_pCamera->SetFar(3000.f);

    // The alignment of a vec3 is 4 floats and the element alignment of a struct is the largest element alignment,
    // which is also the 4 float. Therefore, we need 32 floats as the buffer to store the VP's parameters.
    VkBufferCreateInfo bufferInfo{};
    {
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = 32 * sizeof(float);
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

    float modelMat[16] = {
        0.008f, 0.f,    0.f,    0.f,
        0.f,    0.008f, 0.f,    0.f,
        0.f,    0.f,    0.008f, 0.f,
        0.f,    0.f,    0.f,    1.f
    };
    float vpMat[16] = {};
    float tmpViewMat[16] = {};
    float tmpPersMat[16] = {};
    m_pCamera->GenViewPerspectiveMatrices(tmpViewMat, tmpPersMat, vpMat);

    float combinedMat[32] = {};
    memcpy(combinedMat, modelMat, 16 * sizeof(float));
    memcpy(combinedMat + 16, vpMat, 16 * sizeof(float));

    for (uint32_t i = 0; i < m_swapchainImgCnt; i++)
    {
        vmaCreateBuffer(*m_pAllocator,
                        &bufferInfo,
                        &bufferAllocInfo,
                        &m_vpUboBuffers[i].buffer,
                        &m_vpUboBuffers[i].bufferAlloc,
                        nullptr);

        CopyRamDataToGpuBuffer(combinedMat,
                               m_vpUboBuffers[i].buffer,
                               m_vpUboBuffers[i].bufferAlloc,
                               32 * sizeof(float));

        // NOTE: For the push descriptors, the dstSet is ignored.
        //       This app doesn't have other resources so a fixed descriptor set is enough.
        {
            m_vpUboBuffers[i].bufferDescInfo.buffer = m_vpUboBuffers[i].buffer;
            m_vpUboBuffers[i].bufferDescInfo.offset = 0;
            m_vpUboBuffers[i].bufferDescInfo.range = sizeof(float) * 32;
        }
    }
}

// ================================================================================================================
void SSAOApp::SendCameraDataToBuffer(
    uint32_t i)
{
    float modelMat[16] = {
        0.008f, 0.f,    0.f,    0.f,
        0.f,    0.008f, 0.f,    0.f,
        0.f,    0.f,    0.008f, 0.f,
        0.f,    0.f,    0.f,    1.f
    };

    float vpMat[16] = {};
    float tmpViewMat[16] = {};
    float tmpPersMat[16] = {};
    m_pCamera->GenViewPerspectiveMatrices(tmpViewMat, tmpPersMat, vpMat);

    float combinedMat[32] = {};
    memcpy(combinedMat, modelMat, 16 * sizeof(float));
    memcpy(combinedMat + 16, vpMat, 16 * sizeof(float));

    CopyRamDataToGpuBuffer(combinedMat,
                           m_vpUboBuffers[i].buffer,
                           m_vpUboBuffers[i].bufferAlloc,
                           32 * sizeof(float));
}

// ================================================================================================================
void SSAOApp::UpdateCameraAndGpuBuffer()
{
    std::vector<SharedLib::CustomizedCommand> commands = m_inputHandler.HandleInput();
    bool isCameraMiddleButtonDown = false;

    // Transfer the customized commands to the events to drive camera. This is not ideal but it's a quick solution
    // to reuse the control code.
    for(const auto& command : commands)
    {
        if (m_cameraMoveForwardCmdGen.CheckCmdTypeUID(command.m_commandTypeUID))
        {
            SharedLib::HEventArguments args;
            SharedLib::HEvent cameraForwardEvent(args, "CAMERA_MOVE_FORWARD");
            m_pCamera->OnEvent(cameraForwardEvent);
        }
        else if (m_cameraMoveBackwardCmdGen.CheckCmdTypeUID(command.m_commandTypeUID))
        {
            SharedLib::HEventArguments args;
            SharedLib::HEvent cameraBackwardEvent(args, "CAMERA_MOVE_BACKWARD");
            m_pCamera->OnEvent(cameraBackwardEvent);
        }
        else if (m_cameraMoveLeftCmdGen.CheckCmdTypeUID(command.m_commandTypeUID))
        {
            SharedLib::HEventArguments args;
            SharedLib::HEvent cameraLeftEvent(args, "CAMERA_MOVE_LEFT");
            m_pCamera->OnEvent(cameraLeftEvent);
        }
        else if (m_cameraMoveRightCmdGen.CheckCmdTypeUID(command.m_commandTypeUID))
        {
            SharedLib::HEventArguments args;
            SharedLib::HEvent cameraRightEvent(args, "CAMERA_MOVE_RIGHT");
            m_pCamera->OnEvent(cameraRightEvent);
        }
        else if (m_cameraRotateCmdGen.CheckCmdTypeUID(command.m_commandTypeUID))
        {
            SharedLib::HEventArguments args;
            SharedLib::HFVec2 mousePos;
            mousePos.ele[0] = command.m_payloadFloats[2];
            mousePos.ele[1] = command.m_payloadFloats[3];

            args[crc32("POS")] = mousePos;
            args[crc32("IS_DOWN")] = true;

            SharedLib::HEvent cameraRotateEvent(args, "MOUSE_MIDDLE_BUTTON");
            m_pCamera->OnEvent(cameraRotateEvent);

            isCameraMiddleButtonDown = true;
        }
    }

    if (isCameraMiddleButtonDown == false)
    {
        // Send Empty packet to Camera to reset the hold state.
        SharedLib::HEventArguments args;
        args[crc32("IS_DOWN")] = false;
        SharedLib::HEvent cameraRotateEvent(args, "MOUSE_MIDDLE_BUTTON");
        m_pCamera->OnEvent(cameraRotateEvent);
    }

    SendCameraDataToBuffer(m_acqSwapchainImgIdx);
}

// ================================================================================================================
void SSAOApp::CmdSSAOFrameStartLayoutTrans(VkCommandBuffer cmdBuffer)
{
    // GBuffer layout transites from undefined to render target.
    // Swapchain Depth Buffer from undefined to depth stencil attachment.
    // Swapchain Color Buffer from undefined to color attachment.
    VkImageSubresourceRange colorOneMipOneLevelSubResRange{};
    {
        colorOneMipOneLevelSubResRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorOneMipOneLevelSubResRange.baseMipLevel = 0;
        colorOneMipOneLevelSubResRange.levelCount = 1;
        colorOneMipOneLevelSubResRange.baseArrayLayer = 0;
        colorOneMipOneLevelSubResRange.layerCount = 1;
    }

    VkImageSubresourceRange depthOneMipOneLevelSubResRange{};
    {
        depthOneMipOneLevelSubResRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthOneMipOneLevelSubResRange.baseMipLevel = 0;
        depthOneMipOneLevelSubResRange.levelCount = 1;
        depthOneMipOneLevelSubResRange.baseArrayLayer = 0;
        depthOneMipOneLevelSubResRange.layerCount = 1;
    }

    std::vector<VkImageMemoryBarrier> ssaoPreGPassImgLayoutTransBarriers;

    // Transform the layout of the GBuffer textures from undefined to render target.
    VkImageMemoryBarrier gBufferRenderTargetTransBarrierTemplate{};
    {
        gBufferRenderTargetTransBarrierTemplate.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        gBufferRenderTargetTransBarrierTemplate.srcAccessMask = VK_ACCESS_NONE;
        gBufferRenderTargetTransBarrierTemplate.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        gBufferRenderTargetTransBarrierTemplate.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        gBufferRenderTargetTransBarrierTemplate.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        gBufferRenderTargetTransBarrierTemplate.subresourceRange = colorOneMipOneLevelSubResRange;
    }

    gBufferRenderTargetTransBarrierTemplate.image = m_worldPosTextures[m_acqSwapchainImgIdx].image;
    ssaoPreGPassImgLayoutTransBarriers.push_back(gBufferRenderTargetTransBarrierTemplate);

    gBufferRenderTargetTransBarrierTemplate.image = m_normalTextures[m_acqSwapchainImgIdx].image;
    ssaoPreGPassImgLayoutTransBarriers.push_back(gBufferRenderTargetTransBarrierTemplate);

    gBufferRenderTargetTransBarrierTemplate.image = m_albedoTextures[m_acqSwapchainImgIdx].image;
    ssaoPreGPassImgLayoutTransBarriers.push_back(gBufferRenderTargetTransBarrierTemplate);

    gBufferRenderTargetTransBarrierTemplate.image = m_roughnessMetallicOcclusionTextures[m_acqSwapchainImgIdx].image;
    ssaoPreGPassImgLayoutTransBarriers.push_back(gBufferRenderTargetTransBarrierTemplate);

    // Transform the layout of the swapchain from undefined to color render target.
    gBufferRenderTargetTransBarrierTemplate.image = GetSwapchainColorImage();
    ssaoPreGPassImgLayoutTransBarriers.push_back(gBufferRenderTargetTransBarrierTemplate);

    // Transform the layout of the swapchain depth buffer from undefined to depth stencil attachment.
    gBufferRenderTargetTransBarrierTemplate.image = GetSwapchainDepthImage();
    gBufferRenderTargetTransBarrierTemplate.subresourceRange = depthOneMipOneLevelSubResRange;
    gBufferRenderTargetTransBarrierTemplate.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    ssaoPreGPassImgLayoutTransBarriers.push_back(gBufferRenderTargetTransBarrierTemplate);

    vkCmdPipelineBarrier(
        cmdBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        0, nullptr,
        0, nullptr,
        ssaoPreGPassImgLayoutTransBarriers.size(),
        ssaoPreGPassImgLayoutTransBarriers.data());
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
    
    gBufferRenderTargetTransBarrierTemplate.image = m_worldPosTextures[m_acqSwapchainImgIdx].image;
    gBufferToRenderTargetBarriers.push_back(gBufferRenderTargetTransBarrierTemplate);

    gBufferRenderTargetTransBarrierTemplate.image = m_normalTextures[m_acqSwapchainImgIdx].image;
    gBufferToRenderTargetBarriers.push_back(gBufferRenderTargetTransBarrierTemplate);

    gBufferRenderTargetTransBarrierTemplate.image = m_albedoTextures[m_acqSwapchainImgIdx].image;
    gBufferToRenderTargetBarriers.push_back(gBufferRenderTargetTransBarrierTemplate);

    gBufferRenderTargetTransBarrierTemplate.image = m_roughnessMetallicOcclusionTextures[m_acqSwapchainImgIdx].image;
    gBufferToRenderTargetBarriers.push_back(gBufferRenderTargetTransBarrierTemplate);

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
        attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachmentInfo.clearValue = clearColor;
    }

    attachmentInfo.imageView = m_worldPosTextures[m_acqSwapchainImgIdx].imageView;
    attachmentsInfos.push_back(attachmentInfo);

    attachmentInfo.imageView = m_normalTextures[m_acqSwapchainImgIdx].imageView;
    attachmentsInfos.push_back(attachmentInfo);

    attachmentInfo.imageView = m_albedoTextures[m_acqSwapchainImgIdx].imageView;
    attachmentsInfos.push_back(attachmentInfo);

    attachmentInfo.imageView = m_roughnessMetallicOcclusionTextures[m_acqSwapchainImgIdx].imageView;
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

    // Binding for the base color texture
    VkDescriptorSetLayoutBinding baseColorTextureBinding{};
    {
        baseColorTextureBinding.binding = 1;
        baseColorTextureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        baseColorTextureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        baseColorTextureBinding.descriptorCount = 1;
    }
    bindings.push_back(baseColorTextureBinding);

    // Binding for the normal texture
    VkDescriptorSetLayoutBinding normalTextureBinding{};
    {
        normalTextureBinding.binding = 2;
        normalTextureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        normalTextureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        normalTextureBinding.descriptorCount = 1;
    }
    bindings.push_back(normalTextureBinding);

    // Binding for the roughness metallic texture
    VkDescriptorSetLayoutBinding roughnessMetallicTextureBinding{};
    {
        roughnessMetallicTextureBinding.binding = 3;
        roughnessMetallicTextureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        roughnessMetallicTextureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        roughnessMetallicTextureBinding.descriptorCount = 1;
    }
    bindings.push_back(roughnessMetallicTextureBinding);

    // Binding for the occlusion texture
    VkDescriptorSetLayoutBinding occlusionTextureBinding{};
    {
        occlusionTextureBinding.binding = 4;
        occlusionTextureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        occlusionTextureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        occlusionTextureBinding.descriptorCount = 1;
    }
    bindings.push_back(occlusionTextureBinding);

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
        pVertBindingDesc->stride = 12 * sizeof(float);
        pVertBindingDesc->inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    }
    m_heapMemPtrVec.push_back(pVertBindingDesc);

    VkVertexInputAttributeDescription* pVertAttrDescs = new VkVertexInputAttributeDescription[4];
    memset(pVertAttrDescs, 0, sizeof(VkVertexInputAttributeDescription) * 4);
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
        // Tangent
        pVertAttrDescs[2].location = 2;
        pVertAttrDescs[2].binding = 0;
        pVertAttrDescs[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        pVertAttrDescs[2].offset = 6 * sizeof(float);
        // UV
        pVertAttrDescs[3].location = 3;
        pVertAttrDescs[3].binding = 0;
        pVertAttrDescs[3].format = VK_FORMAT_R32G32_SFLOAT;
        pVertAttrDescs[3].offset = 10 * sizeof(float);
    }
    m_heapArrayMemPtrVec.push_back(pVertAttrDescs);

    VkPipelineVertexInputStateCreateInfo vertInputInfo{};
    {
        vertInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertInputInfo.pNext = nullptr;
        vertInputInfo.vertexBindingDescriptionCount = 1;
        vertInputInfo.pVertexBindingDescriptions = pVertBindingDesc;
        vertInputInfo.vertexAttributeDescriptionCount = 4;
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
/*
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
*/

// ================================================================================================================
void SSAOApp::InitGBuffer()
{
    // It looks like AMD doesn't support the R32G32B32_SFLOAT image format.
    const VkFormat GBuffersImgFormat = VK_FORMAT_R32G32B32A32_SFLOAT;

    VkImageCreateInfo gbufferImageInfo{};
    {
        gbufferImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        gbufferImageInfo.imageType = VK_IMAGE_TYPE_2D;
        gbufferImageInfo.format = GBuffersImgFormat;
        gbufferImageInfo.extent.width = m_swapchainImageExtent.width;
        gbufferImageInfo.extent.height = m_swapchainImageExtent.height;
        gbufferImageInfo.extent.depth = 1;
        gbufferImageInfo.mipLevels = 1;
        gbufferImageInfo.arrayLayers = 1;
        gbufferImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        gbufferImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        gbufferImageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        gbufferImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        gbufferImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    }

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

    VkImageViewCreateInfo gbufferImageViewInfo{};
    {
        gbufferImageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        // posNormalAlbedoImageViewInfo.image = colorImage;
        gbufferImageViewInfo.format = GBuffersImgFormat;
        gbufferImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        gbufferImageViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        gbufferImageViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        gbufferImageViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        gbufferImageViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        gbufferImageViewInfo.subresourceRange = oneMipOneLayerSubRsrcRange;
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

    m_gBufferFormats.push_back(GBuffersImgFormat);
    m_gBufferFormats.push_back(GBuffersImgFormat);
    m_gBufferFormats.push_back(GBuffersImgFormat);
    m_gBufferFormats.push_back(GBuffersImgFormat);

    m_worldPosTextures.resize(m_swapchainImgCnt);
    m_normalTextures.resize(m_swapchainImgCnt);
    m_albedoTextures.resize(m_swapchainImgCnt);
    m_roughnessMetallicOcclusionTextures.resize(m_swapchainImgCnt);

    for (uint32_t i = 0; i < m_swapchainImgCnt; i++)
    {
        vmaCreateImage(*m_pAllocator,
                       &gbufferImageInfo,
                       &gBufferImgAllocInfo,
                       &m_worldPosTextures[i].image,
                       &m_worldPosTextures[i].imageAllocation, nullptr);

        vmaCreateImage(*m_pAllocator,
                       &gbufferImageInfo,
                       &gBufferImgAllocInfo,
                       &m_normalTextures[i].image,
                       &m_normalTextures[i].imageAllocation, nullptr);

        vmaCreateImage(*m_pAllocator,
                       &gbufferImageInfo,
                       &gBufferImgAllocInfo,
                       &m_albedoTextures[i].image,
                       &m_albedoTextures[i].imageAllocation, nullptr);

        vmaCreateImage(*m_pAllocator,
                       &gbufferImageInfo,
                       &gBufferImgAllocInfo,
                       &m_roughnessMetallicOcclusionTextures[i].image,
                       &m_roughnessMetallicOcclusionTextures[i].imageAllocation, nullptr);
        
        gbufferImageViewInfo.image = m_worldPosTextures[i].image;
        VK_CHECK(vkCreateImageView(m_device, &gbufferImageViewInfo, nullptr, &m_worldPosTextures[i].imageView));

        gbufferImageViewInfo.image = m_normalTextures[i].image;
        VK_CHECK(vkCreateImageView(m_device, &gbufferImageViewInfo, nullptr, &m_normalTextures[i].imageView));

        gbufferImageViewInfo.image = m_albedoTextures[i].image;
        VK_CHECK(vkCreateImageView(m_device, &gbufferImageViewInfo, nullptr, &m_albedoTextures[i].imageView));

        gbufferImageViewInfo.image = m_roughnessMetallicOcclusionTextures[i].image;
        VK_CHECK(vkCreateImageView(m_device, &gbufferImageViewInfo, nullptr, &m_roughnessMetallicOcclusionTextures[i].imageView));

        VK_CHECK(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_worldPosTextures[i].imageSampler));
        VK_CHECK(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_normalTextures[i].imageSampler));
        VK_CHECK(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_albedoTextures[i].imageSampler));
        VK_CHECK(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_roughnessMetallicOcclusionTextures[i].imageSampler));

        m_worldPosTextures[i].imageDescInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        m_worldPosTextures[i].imageDescInfo.imageView = m_worldPosTextures[i].imageView;
        m_worldPosTextures[i].imageDescInfo.sampler = m_worldPosTextures[i].imageSampler;

        m_normalTextures[i].imageDescInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        m_normalTextures[i].imageDescInfo.imageView = m_normalTextures[i].imageView;
        m_normalTextures[i].imageDescInfo.sampler = m_normalTextures[i].imageSampler;

        m_albedoTextures[i].imageDescInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        m_albedoTextures[i].imageDescInfo.imageView = m_albedoTextures[i].imageView;
        m_albedoTextures[i].imageDescInfo.sampler = m_albedoTextures[i].imageSampler;

        m_roughnessMetallicOcclusionTextures[i].imageDescInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        m_roughnessMetallicOcclusionTextures[i].imageDescInfo.imageView = m_roughnessMetallicOcclusionTextures[i].imageView;
        m_roughnessMetallicOcclusionTextures[i].imageDescInfo.sampler = m_roughnessMetallicOcclusionTextures[i].imageSampler;
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
        vmaDestroyImage(*m_pAllocator, m_roughnessMetallicOcclusionTextures[i].image, m_roughnessMetallicOcclusionTextures[i].imageAllocation);

        vkDestroyImageView(m_device, m_worldPosTextures[i].imageView, nullptr);
        vkDestroyImageView(m_device, m_normalTextures[i].imageView, nullptr);
        vkDestroyImageView(m_device, m_albedoTextures[i].imageView, nullptr);
        vkDestroyImageView(m_device, m_roughnessMetallicOcclusionTextures[i].imageView, nullptr);

        vkDestroySampler(m_device, m_worldPosTextures[i].imageSampler, nullptr);
        vkDestroySampler(m_device, m_normalTextures[i].imageSampler, nullptr);
        vkDestroySampler(m_device, m_albedoTextures[i].imageSampler, nullptr);
        vkDestroySampler(m_device, m_roughnessMetallicOcclusionTextures[i].imageSampler, nullptr);
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
    VkClearValue depthClearVal{};
    depthClearVal.depthStencil.depth = 0.f;
    VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };

    VkViewport viewport = GetCurrentSwapchainViewport();
    VkRect2D scissor = GetCurrentSwapchainScissor();

    VkRenderingAttachmentInfoKHR geoPassDepthAttachmentInfo{};
    {
        geoPassDepthAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
        geoPassDepthAttachmentInfo.imageView = GetSwapchainDepthImageView();
        geoPassDepthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
        geoPassDepthAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // We need to reuse the depth render target.
        geoPassDepthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        // geoPassDepthAttachmentInfo.clearValue = depthClearVal;
    }

    std::vector<VkRenderingAttachmentInfoKHR> gBufferAttachmentsInfos = GetGBufferAttachments();

    VkRenderingInfoKHR geoPassRenderInfo{};
    {
        geoPassRenderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
        geoPassRenderInfo.renderArea.offset = { 0, 0 };
        geoPassRenderInfo.renderArea.extent = GetSwapchainImageExtent();
        geoPassRenderInfo.layerCount = 1;
        geoPassRenderInfo.colorAttachmentCount = gBufferAttachmentsInfos.size();
        geoPassRenderInfo.pColorAttachments = gBufferAttachmentsInfos.data();
        geoPassRenderInfo.pDepthAttachment = &geoPassDepthAttachmentInfo;
    }

    VkClearRect clearRect{};
    clearRect.rect = scissor;
    clearRect.baseArrayLayer = 0;
    clearRect.layerCount = 1;

    std::vector<VkClearAttachment> clearAttachments;
    std::vector<VkClearRect> clearRects;
    // Clear the G-Buffer attachments.
    for(int i = 0; i < gBufferAttachmentsInfos.size(); ++i)
    {
        VkClearAttachment clearAttachment{};
        clearAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        clearAttachment.colorAttachment = i;
        clearAttachment.clearValue = clearColor;
        clearAttachments.push_back(clearAttachment);

        clearRects.push_back(clearRect);
    }
    // Clear the depth attachment.
    VkClearAttachment clearDepthAttachment{};
    clearDepthAttachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    clearDepthAttachment.clearValue = depthClearVal;
    clearAttachments.push_back(clearDepthAttachment);
    clearRects.push_back(clearRect);

    vkCmdBeginRendering(cmdBuffer, &geoPassRenderInfo);

    vkCmdClearAttachments(cmdBuffer, clearAttachments.size(), clearAttachments.data(), clearRects.size(), clearRects.data());

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_geoPassPipeline.GetVkPipeline());

    int meshEntityCnt = 0;
    for (const auto& meshEntity : m_pLevel->m_meshEntities)
    {
        for (int i = 0; i < meshEntity.second->m_meshPrimitives.size(); i++)
        {
            // NOTE: We cannot put any barriers in a render pass.
            auto& meshPrimitive = meshEntity.second->m_meshPrimitives[i];

            VkDeviceSize offsets[] = { 0 };
            vkCmdBindVertexBuffers(cmdBuffer, 0, 1, meshPrimitive.GetVertBuffer(), offsets);
            vkCmdBindIndexBuffer(cmdBuffer, meshPrimitive.GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT16);

            vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
            vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

            // Bind the descriptor set for the geometry pass rendering.
            std::vector<SharedLib::PushDescriptorInfo> pushDescriptors;
            pushDescriptors.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &m_vpUboBuffers[m_acqSwapchainImgIdx].bufferDescInfo });
            pushDescriptors.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, meshPrimitive.GetBaseColorImgDescInfo() });
            pushDescriptors.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, meshPrimitive.GetNormalImgDescInfo() });
            pushDescriptors.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, meshPrimitive.GetMetallicRoughnessImgDescInfo() });
            pushDescriptors.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, meshPrimitive.GetOcclusionImgDescInfo() });

            CmdAutoPushDescriptors(cmdBuffer, m_geoPassPipelineLayout, pushDescriptors);

            vkCmdDrawIndexed(cmdBuffer, meshPrimitive.m_idxDataUint16.size(), 1, 0, 0, 0);
        }
        meshEntityCnt++;
    }

    vkCmdEndRendering(cmdBuffer);
}

// ================================================================================================================
void SSAOApp::CmdSSAOAppMultiTypeRendering(VkCommandBuffer cmdBuffer)
{
    // Only direct light and ambient light.
    if (m_presentType == PresentType::DIFFUSE)
    {
        VkRenderingAttachmentInfo diffuseRenderColorAttachment = GetSwapchainColorAttachmentWithClearInfo();

        VkRenderingInfoKHR albedoRenderInfo{};
        {
            albedoRenderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
            albedoRenderInfo.renderArea.offset = { 0, 0 };
            albedoRenderInfo.renderArea.extent = GetSwapchainImageExtent();
            albedoRenderInfo.layerCount = 1;
            albedoRenderInfo.colorAttachmentCount = 1;
            albedoRenderInfo.pColorAttachments = &diffuseRenderColorAttachment;
        }

        vkCmdBeginRendering(cmdBuffer, &albedoRenderInfo);
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_albedoRenderingPipeline.GetVkPipeline());

        // Bind the descriptor set for the geometry pass rendering.
        std::vector<SharedLib::PushDescriptorInfo> pushDescriptors;
        pushDescriptors.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &m_albedoTextures[m_acqSwapchainImgIdx].imageDescInfo });
        CmdAutoPushDescriptors(cmdBuffer, m_albedoRenderingPipelineLayout, pushDescriptors);

        vkCmdDraw(cmdBuffer, 6, 1, 0, 0);

        vkCmdEndRendering(cmdBuffer);
    }
}

// ================================================================================================================
void SSAOApp::ImGuiFrame(VkCommandBuffer cmdBuffer)
{
    // Start the Dear ImGui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::ShowDemoWindow();

    ImGui::Render();

    ImDrawData* drawData = ImGui::GetDrawData();
    const bool is_minimized = (drawData->DisplaySize.x <= 0.0f || drawData->DisplaySize.y <= 0.0f);

    // Begin the render pass and record relevant commands
    // Link framebuffer into the render pass
    VkRenderPassBeginInfo renderPassInfo{};
    {
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_guiRenderPass;
        renderPassInfo.framebuffer = m_imGuiFramebuffers[m_acqSwapchainImgIdx];
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = m_swapchainImageExtent;
        // renderPassInfo.clearValueCount = 1;
        // renderPassInfo.pClearValues = &clearColor;
    }
    vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Record the GUI rendering commands.
    ImGui_ImplVulkan_RenderDrawData(drawData, cmdBuffer);

    vkCmdEndRenderPass(cmdBuffer);
}

// ================================================================================================================
void SSAOApp::InitScreenQuadVsShaderModule()
{
    m_screenQuadVsShaderModule = CreateShaderModule("/hlsl/screen_quad_vert.spv");
}

// ================================================================================================================
void SSAOApp::InitAlbedoRenderingShaderModules()
{
    m_albedoRenderingPsShaderModule = CreateShaderModule("/hlsl/ambient_lighting_frag.spv");
}

// ================================================================================================================
void SSAOApp::InitAlbedoRenderingPipelineLayout()
{
    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    {
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_albedoRenderingPipelineDesSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
    }

    VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_albedoRenderingPipelineLayout));
}

// ================================================================================================================
void SSAOApp::InitAlbedoRenderingPipelineDescriptorSetLayout()
{
    // Binding for the albedo gbuffer texture
    VkDescriptorSetLayoutBinding albedoGBufferTextureBinding{};
    {
        albedoGBufferTextureBinding.binding = 0;
        albedoGBufferTextureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        albedoGBufferTextureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        albedoGBufferTextureBinding.descriptorCount = 1;
    }

    // Create pipeline's descriptors layout
    // The Vulkan spec states: The VkDescriptorSetLayoutBinding::binding members of the elements of the pBindings array 
    // must each have different values 
    // (https://vulkan.lunarg.com/doc/view/1.3.236.0/windows/1.3-extensions/vkspec.html#VUID-VkDescriptorSetLayoutCreateInfo-binding-00279)
    VkDescriptorSetLayoutCreateInfo pipelineDesSetLayoutInfo{};
    {
        pipelineDesSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        // Setting this flag tells the descriptor set layouts that no actual descriptor sets are allocated but instead pushed at command buffer creation time
        pipelineDesSetLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
        pipelineDesSetLayoutInfo.bindingCount = 1;
        pipelineDesSetLayoutInfo.pBindings = &albedoGBufferTextureBinding;
    }

    VK_CHECK(vkCreateDescriptorSetLayout(m_device,
                                         &pipelineDesSetLayoutInfo,
                                         nullptr,
                                         &m_albedoRenderingPipelineDesSetLayout));
}

// ================================================================================================================
void SSAOApp::InitAlbedoRenderingPipeline()
{
    VkPipelineRenderingCreateInfoKHR pipelineRenderCreateInfo{};
    {
        pipelineRenderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        pipelineRenderCreateInfo.colorAttachmentCount = 1;
        pipelineRenderCreateInfo.pColorAttachmentFormats = &m_choisenSurfaceFormat.format;
    }

    m_albedoRenderingPipeline.SetPNext(&pipelineRenderCreateInfo);
    m_albedoRenderingPipeline.SetPipelineLayout(m_albedoRenderingPipelineLayout);

    VkPipelineShaderStageCreateInfo shaderStgsInfo[2] = {};
    shaderStgsInfo[0] = CreateDefaultShaderStgCreateInfo(m_screenQuadVsShaderModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStgsInfo[1] = CreateDefaultShaderStgCreateInfo(m_albedoRenderingPsShaderModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    m_albedoRenderingPipeline.SetShaderStageInfo(shaderStgsInfo, 2);

    m_albedoRenderingPipeline.CreatePipeline(m_device);
}

// ================================================================================================================
void SSAOApp::SetupInputHandler()
{
    std::unordered_set<SharedLib::InputEnum> camRotateKeyCombs = {SharedLib::InputEnum::PRESS_MOUSE_MIDDLE_BUTTON,
                                                                  SharedLib::InputEnum::MOUSE_MOVE};
    m_cameraRotateCmdGen.SetKeyCombination(camRotateKeyCombs);

    std::unordered_set<SharedLib::InputEnum> camMoveForwardKeyCombs = {SharedLib::InputEnum::PRESS_W };
    m_cameraMoveForwardCmdGen.SetKeyCombination(camMoveForwardKeyCombs);

    std::unordered_set<SharedLib::InputEnum> camMoveBackwardKeyCombs = {SharedLib::InputEnum::PRESS_S };
    m_cameraMoveBackwardCmdGen.SetKeyCombination(camMoveBackwardKeyCombs);

    std::unordered_set<SharedLib::InputEnum> camMoveLeftKeyCombs = {SharedLib::InputEnum::PRESS_A };
    m_cameraMoveLeftCmdGen.SetKeyCombination(camMoveLeftKeyCombs);

    std::unordered_set<SharedLib::InputEnum> camMoveRightKeyCombs = {SharedLib::InputEnum::PRESS_D };
    m_cameraMoveRightCmdGen.SetKeyCombination(camMoveRightKeyCombs);

    m_inputHandler.AddOrUpdateCommandGenerator(&m_cameraRotateCmdGen);
    m_inputHandler.AddOrUpdateCommandGenerator(&m_cameraMoveForwardCmdGen);
    m_inputHandler.AddOrUpdateCommandGenerator(&m_cameraMoveBackwardCmdGen);
    m_inputHandler.AddOrUpdateCommandGenerator(&m_cameraMoveLeftCmdGen);
    m_inputHandler.AddOrUpdateCommandGenerator(&m_cameraMoveRightCmdGen);
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
    // glfwSetMouseButtonCallback(m_pWindow, MouseButtonCallback);
    // glfwSetKeyCallback(m_pWindow, KeyCallback);

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

    InitSwapchain();
    InitGfxCommandPool();
    InitGfxCommandBuffers(m_swapchainImgCnt);
    SwapchainColorImgsLayoutTrans(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    SwapchainDepthImgsLayoutTrans(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    InitImGui();

    InitVpUboObjects();
    
    // InitAlbedoSSBO();
    // InitMetallicRoughnessSSBO();
    InitGBuffer();
    // InitLightPosRadianceSSBOs();
    SetupInputHandler();

    // Load in gltf scene.
    m_pGltfLoaderManager = new SharedLib::GltfLoaderManager();
    m_pLevel = new SharedLib::Level();

    std::string sceneLoadPathAbs = SOURCE_PATH;
    sceneLoadPathAbs += +"/../data/Sponza/Sponza.gltf";
    // sceneLoadPathAbs += +"/../data/Box/Box.gltf";

    m_pGltfLoaderManager->Load(sceneLoadPathAbs, *m_pLevel);
    m_pGltfLoaderManager->InitEntitesGpuRsrc(m_device, m_pAllocator, GetGfxCmdBuffer(0), m_graphicsQueue);

    InitScreenQuadVsShaderModule();

    InitGeoPassShaderModules();
    InitGeoPassPipelineDescriptorSetLayout();
    InitGeoPassPipelineLayout();
    InitGeoPassPipeline();
    
    // Pipeline for the Albedo rendering.
    InitAlbedoRenderingShaderModules();
    InitAlbedoRenderingPipelineDescriptorSetLayout();
    InitAlbedoRenderingPipelineLayout();
    InitAlbedoRenderingPipeline();

    /*
    InitDeferredLightingPassShaderModules();
    InitDeferredLightingPassPipelineDescriptorSetLayout();
    InitDeferredLightingPassPipelineLayout();
    InitDeferredLightingPassPipeline();
    InitDeferredLightingPassRadianceTextures();
    */
    InitSwapchainSyncObjects();
    // InitGammaCorrectionPipelineAndRsrc();

    // TODO: I need to transfer all GPU Textures to the Shader Read Format.

}