#include "GenIBL.h"
#include "vk_mem_alloc.h"
#include "../../SharedLibrary/Utils/VulkanDbgUtils.h"
#include "../../SharedLibrary/Utils/CmdBufUtils.h"

// ================================================================================================================
void GenIBL::DestroyPrefilterEnvMapPipelineResourses()
{
    vkDestroyShaderModule(m_device, m_preFilterEnvMapVsShaderModule, nullptr);
    vkDestroyShaderModule(m_device, m_preFilterEnvMapPsShaderModule, nullptr);

    // Destroy the pipeline layout
    vkDestroyPipelineLayout(m_device, m_preFilterEnvMapPipelineLayout, nullptr);

    vmaDestroyImage(*m_pAllocator, m_preFilterEnvMapCubemap, m_preFilterEnvMapCubemapAlloc);

    for (uint32_t i = 0; i < m_preFilterEnvMapCubemapImageViews.size(); i++)
    {
        vkDestroyImageView(m_device, m_preFilterEnvMapCubemapImageViews[i], nullptr);
    }
}

// ================================================================================================================
void GenIBL::InitPrefilterEnvMapPipelineLayout()
{
    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    {
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_diffIrrPreFilterEnvMapDesSet0Layout;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
    }

    VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_preFilterEnvMapPipelineLayout));
}

// ================================================================================================================
void GenIBL::InitPrefilterEnvMapShaderModules()
{
    m_preFilterEnvMapVsShaderModule = CreateShaderModule("/hlsl/prefilterEnvMap_vert.spv");
    m_preFilterEnvMapPsShaderModule = CreateShaderModule("/hlsl/prefilterEnvMap_frag.spv");
}

// ================================================================================================================
void GenIBL::InitPrefilterEnvMapPipeline()
{
    VkPipelineRenderingCreateInfoKHR prefilterEnvMapCreateInfo{};
    {
        prefilterEnvMapCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        prefilterEnvMapCreateInfo.viewMask = 0x3F;
        prefilterEnvMapCreateInfo.colorAttachmentCount = 1;
        prefilterEnvMapCreateInfo.pColorAttachmentFormats = &HdriRenderTargetFormat;
    }

    m_preFilterEnvMapPipeline.SetPNext(&prefilterEnvMapCreateInfo);

    VkPipelineShaderStageCreateInfo shaderStgsInfo[2] = {};
    shaderStgsInfo[0] = CreateDefaultShaderStgCreateInfo(m_preFilterEnvMapVsShaderModule,
        VK_SHADER_STAGE_VERTEX_BIT);
    shaderStgsInfo[1] = CreateDefaultShaderStgCreateInfo(m_preFilterEnvMapPsShaderModule,
        VK_SHADER_STAGE_FRAGMENT_BIT);

    m_preFilterEnvMapPipeline.SetShaderStageInfo(shaderStgsInfo, 2);
    m_preFilterEnvMapPipeline.SetPipelineLayout(m_preFilterEnvMapPipelineLayout);
    m_preFilterEnvMapPipeline.CreatePipeline(m_device);
}

// ================================================================================================================
void GenIBL::InitPrefilterEnvMapOutputObjects()
{
    VmaAllocationCreateInfo preFilterEnvMapAllocInfo{};
    {
        preFilterEnvMapAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        preFilterEnvMapAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    VkExtent3D extent{};
    {
        extent.width = m_hdrCubeMapInfo.width;
        extent.height = m_hdrCubeMapInfo.width;
        extent.depth = 1;
    }

    VkImageCreateInfo cubeMapImgInfo{};
    {
        cubeMapImgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        cubeMapImgInfo.imageType = VK_IMAGE_TYPE_2D;
        cubeMapImgInfo.format = HdriRenderTargetFormat;
        cubeMapImgInfo.extent = extent;
        cubeMapImgInfo.mipLevels = RoughnessLevels;
        cubeMapImgInfo.arrayLayers = 6;
        cubeMapImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        cubeMapImgInfo.tiling = VK_IMAGE_TILING_LINEAR;
        cubeMapImgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        // cubeMapImgInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT; // It's just an output. We don't need a cubemap sampler.
        cubeMapImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    VK_CHECK(vmaCreateImage(*m_pAllocator,
        &cubeMapImgInfo,
        &preFilterEnvMapAllocInfo,
        &m_preFilterEnvMapCubemap,
        &m_preFilterEnvMapCubemapAlloc,
        nullptr));

    m_preFilterEnvMapCubemapImageViews.resize(RoughnessLevels);
    for (uint32_t i = 0; i < RoughnessLevels; i++)
    {
        VkImageViewCreateInfo info{};
        {
            info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            info.image = m_preFilterEnvMapCubemap;
            info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            info.format = HdriRenderTargetFormat;
            info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            info.subresourceRange.baseMipLevel = i;
            info.subresourceRange.levelCount = 1;
            info.subresourceRange.layerCount = 6;
        }
        VK_CHECK(vkCreateImageView(m_device, &info, nullptr, &m_preFilterEnvMapCubemapImageViews[i]));
    }
}

// ================================================================================================================
void GenIBL::UpdateRoughnessInUbo(
    float roughness,
    float imgDim)
{
    float near = 1.f;
    float nearWidthHeight[2] = { 2.f, 2.f };
    float viewportWidthHeight[2] = { imgDim, imgDim };

    m_screenCameraData[72] = near;
    m_screenCameraData[73] = roughness;
    memcpy(&m_screenCameraData[74], nearWidthHeight, sizeof(nearWidthHeight));
    memcpy(&m_screenCameraData[76], viewportWidthHeight, sizeof(viewportWidthHeight));

    // Send data to the GPU buffer
    CopyRamDataToGpuBuffer(m_screenCameraData,
                           m_uboCameraScreenBuffer,
                           m_uboCameraScreenAlloc,
                           sizeof(m_screenCameraData));
}

// ================================================================================================================
void GenIBL::GenPrefilterEnvMap()
{
    VkCommandBuffer cmdBuffer = GetGfxCmdBuffer(0);

    // Transfer the environment filter cubemap from undefined to color attachment
    {
        // Fill the command buffer
        VkCommandBufferBeginInfo beginInfo{};
        {
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        }
        VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo));

        VkImageSubresourceRange prefilterEnvMapSubresource{};
        {
            prefilterEnvMapSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            prefilterEnvMapSubresource.baseMipLevel = 0;
            prefilterEnvMapSubresource.levelCount = RoughnessLevels;
            prefilterEnvMapSubresource.baseArrayLayer = 0;
            prefilterEnvMapSubresource.layerCount = 6;
        }

        VkImageMemoryBarrier cubemapRenderTargetTransBarrier{};
        {
            cubemapRenderTargetTransBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            cubemapRenderTargetTransBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            cubemapRenderTargetTransBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            cubemapRenderTargetTransBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            cubemapRenderTargetTransBarrier.image = m_preFilterEnvMapCubemap;
            cubemapRenderTargetTransBarrier.subresourceRange = prefilterEnvMapSubresource;
        }

        vkCmdPipelineBarrier(cmdBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &cubemapRenderTargetTransBarrier);

        // Submit all the works recorded before
        VK_CHECK(vkEndCommandBuffer(cmdBuffer));

        SharedLib::SubmitCmdBufferAndWait(m_device, m_graphicsQueue, cmdBuffer);

        vkResetCommandBuffer(cmdBuffer, 0);
    }

    // Shared information
    VkClearValue clearColor = { {{1.0f, 0.0f, 0.0f, 1.0f}} };

    // Render the prefilter environment map into different mip maps
    for (uint32_t i = 0; i < RoughnessLevels; i++)
    {
        // Fill the command buffer
        VkCommandBufferBeginInfo beginInfo{};
        {
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        }
        VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo));

        float currentRoughness = float(i) / float(RoughnessLevels - 1);
        uint32_t divFactor = 1 << i;
        uint32_t currentRenderDim = m_hdrCubeMapInfo.width / divFactor;
        VkExtent2D colorRenderTargetExtent{};
        {
            colorRenderTargetExtent.width = currentRenderDim;
            colorRenderTargetExtent.height = currentRenderDim;
        }

        // BUG! The buffer change happens but packets are not submitted to GPU!
        UpdateRoughnessInUbo(currentRoughness, float(currentRenderDim));

        VkRenderingAttachmentInfoKHR renderAttachmentInfo{};
        {
            renderAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
            renderAttachmentInfo.imageView = m_preFilterEnvMapCubemapImageViews[i];
            renderAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            renderAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            renderAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            renderAttachmentInfo.clearValue = clearColor;
        }

        VkRenderingInfoKHR renderInfo{};
        {
            renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
            renderInfo.renderArea.offset = { 0, 0 };
            renderInfo.renderArea.extent = colorRenderTargetExtent;
            renderInfo.layerCount = 6;
            renderInfo.colorAttachmentCount = 1;
            renderInfo.viewMask = 0x3F;
            renderInfo.pColorAttachments = &renderAttachmentInfo;
        }

        vkCmdBeginRendering(cmdBuffer, &renderInfo);

        // Bind the graphics pipeline
        vkCmdBindDescriptorSets(cmdBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_preFilterEnvMapPipelineLayout,
            0, 1, &m_diffIrrPreFilterEnvMapDesSet0,
            0, NULL);

        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_preFilterEnvMapPipeline.GetVkPipeline());

        // Set the viewport
        VkViewport viewport{};
        {
            viewport.x = 0.f;
            viewport.y = 0.f;
            viewport.width = (float)colorRenderTargetExtent.width;
            viewport.height = (float)colorRenderTargetExtent.height;
            viewport.minDepth = 0.f;
            viewport.maxDepth = 1.f;
        }
        vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

        // Set the scissor
        VkRect2D scissor{};
        {
            scissor.offset = { 0, 0 };
            scissor.extent = colorRenderTargetExtent;
            vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
        }

        vkCmdDraw(cmdBuffer, 6, 1, 0, 0);

        vkCmdEndRendering(cmdBuffer);

        // Submit all the works recorded before
        VK_CHECK(vkEndCommandBuffer(cmdBuffer));

        SharedLib::SubmitCmdBufferAndWait(m_device, m_graphicsQueue, cmdBuffer);

        vkResetCommandBuffer(cmdBuffer, 0);
    }
}