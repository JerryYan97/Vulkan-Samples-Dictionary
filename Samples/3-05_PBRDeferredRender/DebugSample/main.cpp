#include "vk_mem_alloc.h"

#include "PBRDeferredApp.h"
#include "../../../SharedLibrary/Utils/VulkanDbgUtils.h"

#include <vulkan/vulkan.h>

int main()
{
    PBRDeferredApp app;
    app.AppInit();

    app.GpuWaitForIdle();

    VkImageSubresourceRange swapchainPresentSubResRange{};
    {
        swapchainPresentSubResRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        swapchainPresentSubResRange.baseMipLevel = 0;
        swapchainPresentSubResRange.levelCount = 1;
        swapchainPresentSubResRange.baseArrayLayer = 0;
        swapchainPresentSubResRange.layerCount = 1;
    }

    // Main Loop
    while (!app.WindowShouldClose())
    {
        VkDevice device = app.GetVkDevice();
        VkFence inFlightFence = app.GetCurrentFrameFence();
        VkCommandBuffer currentCmdBuffer = app.GetCurrentFrameGfxCmdBuffer();

        VkExtent2D swapchainImageExtent = app.GetSwapchainImageExtent();

        app.FrameStart();

        // Wait for the resources from the possible on flight frame
        vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);

        // Get next available image from the swapchain
        uint32_t imageIndex;
        if (app.NextImgIdxOrNewSwapchain(imageIndex) == false)
        {
            continue;
        }

        // Reset unused previous frame's resource
        vkResetFences(device, 1, &inFlightFence);
        vkResetCommandBuffer(currentCmdBuffer, 0);

        app.UpdateCameraAndGpuBuffer();

        // Fill the command buffer
        VkCommandBufferBeginInfo beginInfo{};
        {
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        }
        VK_CHECK(vkBeginCommandBuffer(currentCmdBuffer, &beginInfo));
        
        app.CmdSwapchainColorImgLayoutTrans(
                                  currentCmdBuffer,
                                  VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                  VK_ACCESS_NONE,
                                  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        /* ------------- Geometry Pass ------------- */
        std::vector<VkRenderingAttachmentInfoKHR> gBufferAttachmentsInfos = app.GetGBufferAttachments();

        VkClearValue depthClearVal{};
        depthClearVal.depthStencil.depth = 0.f;
        VkRenderingAttachmentInfoKHR geoPassDepthAttachmentInfo{};
        {
            geoPassDepthAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
            geoPassDepthAttachmentInfo.imageView = app.GetSwapchainDepthImageView(imageIndex);
            geoPassDepthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
            geoPassDepthAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            geoPassDepthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            geoPassDepthAttachmentInfo.clearValue = depthClearVal;
        }

        VkRenderingInfoKHR geoPassRenderInfo{};
        {
            geoPassRenderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
            geoPassRenderInfo.renderArea.offset = { 0, 0 };
            geoPassRenderInfo.renderArea.extent = swapchainImageExtent;
            geoPassRenderInfo.layerCount = 1;
            geoPassRenderInfo.colorAttachmentCount = gBufferAttachmentsInfos.size();
            geoPassRenderInfo.pColorAttachments = gBufferAttachmentsInfos.data();
            geoPassRenderInfo.pDepthAttachment = &geoPassDepthAttachmentInfo;
        }

        vkCmdBeginRendering(currentCmdBuffer, &geoPassRenderInfo);

        void* pPushConstant = nullptr;
        uint32_t pushConstantBytesCnt = 0;
        app.GetDeferredLightingPushConstantData(&pPushConstant, pushConstantBytesCnt);

        vkCmdPushConstants(currentCmdBuffer,
                           app.GetGeoPassPipelineLayout(),
                           VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, pushConstantBytesCnt, pPushConstant);

        free(pPushConstant);

        // Bind the pipeline descriptor sets
        std::vector<VkWriteDescriptorSet> geoPassWriteDescriptorSet0 = app.GetGeoPassWriteDescriptorSets();
        app.m_vkCmdPushDescriptorSetKHR(currentCmdBuffer,
                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        app.GetGeoPassPipelineLayout(),
                                        0, geoPassWriteDescriptorSet0.size(), geoPassWriteDescriptorSet0.data());

        // Bind the graphics pipeline
        vkCmdBindPipeline(currentCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app.GetGeoPassPipeline());

        // Set the viewport
        VkViewport viewport{};
        {
            viewport.x = 0.f;
            viewport.y = 0.f;
            viewport.width = (float)swapchainImageExtent.width;
            viewport.height = (float)swapchainImageExtent.height;
            viewport.minDepth = 0.f;
            viewport.maxDepth = 1.f;
        }
        vkCmdSetViewport(currentCmdBuffer, 0, 1, &viewport);

        // Set the scissor
        VkRect2D scissor{};
        {
            scissor.offset = { 0, 0 };
            scissor.extent = swapchainImageExtent;
            vkCmdSetScissor(currentCmdBuffer, 0, 1, &scissor);
        }

        // Bind vertex and index buffer
        VkDeviceSize vbOffset = 0;
        VkBuffer vertBuffer = app.GetVertBuffer();
        VkBuffer idxBuffer = app.GetIdxBuffer();

        vkCmdBindVertexBuffers(currentCmdBuffer, 0, 1, &vertBuffer, &vbOffset);
        vkCmdBindIndexBuffer(currentCmdBuffer, idxBuffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(currentCmdBuffer, app.GetIdxCnt(), SphereCounts, 0, 0, 0);

        vkCmdEndRendering(currentCmdBuffer);

        app.CmdSwapchainColorImgToPresent(currentCmdBuffer);

        VK_CHECK(vkEndCommandBuffer(currentCmdBuffer));

        app.GfxCmdBufferFrameSubmitAndPresent();

        app.FrameEnd();
    }
}
