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

        // Fill the command buffer
        VkCommandBufferBeginInfo beginInfo{};
        {
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        }
        VK_CHECK(vkBeginCommandBuffer(currentCmdBuffer, &beginInfo));

        app.CmdSwapchainColorImgToRenderTarget(currentCmdBuffer);
        app.CmdGBufferToRenderTarget(currentCmdBuffer, VK_IMAGE_LAYOUT_UNDEFINED);

        // Geometry pass
        std::vector<VkRenderingAttachmentInfoKHR> gBufferAttachmentsInfos = app.GetGBufferAttachments();

        VkClearValue depthClearVal{};
        depthClearVal.depthStencil.depth = 0.f;
        VkRenderingAttachmentInfoKHR depthAttachmentInfo{};
        {
            depthAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
            depthAttachmentInfo.imageView = app.GetSwapchainDepthImageView(imageIndex);
            depthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
            depthAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            depthAttachmentInfo.clearValue = depthClearVal;
        }

        VkRenderingInfoKHR renderInfo{};
        {
            renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
            renderInfo.renderArea.offset = { 0, 0 };
            renderInfo.renderArea.extent = swapchainImageExtent;
            renderInfo.layerCount = 1;
            renderInfo.colorAttachmentCount = gBufferAttachmentsInfos.size();
            renderInfo.pColorAttachments = gBufferAttachmentsInfos.data();
            renderInfo.pDepthAttachment = &depthAttachmentInfo;
        }

        vkCmdBeginRendering(currentCmdBuffer, &renderInfo);

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
