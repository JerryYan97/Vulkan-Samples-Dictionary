#include "vk_mem_alloc.h"

#include "PBRIBLApp.h"
#include "../../../SharedLibrary/Utils/VulkanDbgUtils.h"
#include "../../../SharedLibrary/Utils/CmdBufUtils.h"

#include <vulkan/vulkan.h>
#include <Windows.h>
#include <cassert>

int main()
{
    PBRIBLApp app;
    app.AppInit();

    VkImageSubresourceRange swapchainPresentSubResRange{};
    {
        swapchainPresentSubResRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        swapchainPresentSubResRange.baseMipLevel = 0;
        swapchainPresentSubResRange.levelCount = 1;
        swapchainPresentSubResRange.baseArrayLayer = 0;
        swapchainPresentSubResRange.layerCount = 1;
    }

    VkBuffer vertBuffer = app.GetIblVertBuffer();
    VkBuffer idxBuffer = app.GetIblIdxBuffer();

    // Main Loop
    // Two draws. First draw draws triangle into an image with window 1 window size.
    // Second draw draws GUI. GUI would use the image drawn from the first draw.
    while (!app.WindowShouldClose())
    {
        VkDevice device = app.GetVkDevice();
        
        app.FrameStart();

        // Get next available image from the swapchain
        if (app.WaitNextImgIdxOrNewSwapchain() == false)
        {
            continue;
        }

        VkCommandBuffer currentCmdBuffer = app.GetCurrentFrameGfxCmdBuffer();
        VkExtent2D swapchainImageExtent = app.GetSwapchainImageExtent();

        // Fill the command buffer
        VkCommandBufferBeginInfo beginInfo{};
        {
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        }
        VK_CHECK(vkBeginCommandBuffer(currentCmdBuffer, &beginInfo));

        // Update the camera according to mouse input and sent camera data to the UBO
        app.UpdateCameraAndGpuBuffer();

        // Transform the layout of the swapchain from undefined to render target.
        VkImageMemoryBarrier swapchainRenderTargetTransBarrier{};
        {
            swapchainRenderTargetTransBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            swapchainRenderTargetTransBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            swapchainRenderTargetTransBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            swapchainRenderTargetTransBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            swapchainRenderTargetTransBarrier.image = app.GetSwapchainColorImage();
            swapchainRenderTargetTransBarrier.subresourceRange = swapchainPresentSubResRange;
        }

        vkCmdPipelineBarrier(
            currentCmdBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &swapchainRenderTargetTransBarrier);

        // Draw the scene
        VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };

        VkRenderingAttachmentInfoKHR renderBackgroundAttachmentInfo{};
        {
            renderBackgroundAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
            renderBackgroundAttachmentInfo.imageView = app.GetSwapchainColorImageView();
            renderBackgroundAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
            renderBackgroundAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            renderBackgroundAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            renderBackgroundAttachmentInfo.clearValue = clearColor;
        }

        VkRenderingAttachmentInfoKHR renderSpheresAttachmentInfo{};
        {
            renderSpheresAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
            renderSpheresAttachmentInfo.imageView = app.GetSwapchainColorImageView();
            renderSpheresAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
            renderSpheresAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            renderSpheresAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            renderSpheresAttachmentInfo.clearValue = clearColor;
        }

        VkClearValue depthClearVal{};
        depthClearVal.depthStencil.depth = 0.f;
        VkRenderingAttachmentInfoKHR depthAttachmentInfo{};
        {
            depthAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
            depthAttachmentInfo.imageView = app.GetSwapchainDepthImageView();
            depthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
            depthAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            depthAttachmentInfo.clearValue = depthClearVal;
        }

        VkRenderingInfoKHR renderBackgroundInfo{};
        {
            renderBackgroundInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
            renderBackgroundInfo.renderArea.offset = { 0, 0 };
            renderBackgroundInfo.renderArea.extent = swapchainImageExtent;
            renderBackgroundInfo.layerCount = 1;
            renderBackgroundInfo.colorAttachmentCount = 1;
            renderBackgroundInfo.pColorAttachments = &renderBackgroundAttachmentInfo;
        }

        vkCmdBeginRendering(currentCmdBuffer, &renderBackgroundInfo);

        // Render background cubemap
        // Bind the skybox pipeline descriptor sets
        /*
        vkCmdBindDescriptorSets(currentCmdBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS, 
                                app.GetSkyboxPipelineLayout(), 
                                0, 1, &currentSkyboxPipelineDesSet0, 0, NULL);
        */

        app.CmdPushSkyboxDescriptors(currentCmdBuffer);

        // Bind the graphics pipeline
        vkCmdBindPipeline(currentCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app.GetSkyboxPipeline());

        // Set the viewport
        VkViewport viewport{};
        {
            viewport.x = 0.f;
            viewport.y = 0.f;
            viewport.width  = (float)swapchainImageExtent.width;
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
        }
        vkCmdSetScissor(currentCmdBuffer, 0, 1, &scissor);

        vkCmdDraw(currentCmdBuffer, 6, 1, 0, 0);

        vkCmdEndRendering(currentCmdBuffer);

        // Render spheres
        // Let IBL render in the color attachment after the skybox rendering completes.
        vkCmdPipelineBarrier(currentCmdBuffer,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0,
            0, nullptr,
            0, nullptr,
            0, nullptr);

        /*
        vkCmdBindDescriptorSets(currentCmdBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                app.GetIblPipelineLayout(),
                                0, 1, &currentIblPipelineDesSet0, 0, NULL);
        */

        app.CmdPushSphereIBLDescriptors(currentCmdBuffer);

        VkRenderingInfoKHR renderSpheresInfo{};
        {
            renderSpheresInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
            renderSpheresInfo.renderArea.offset = { 0, 0 };
            renderSpheresInfo.renderArea.extent = swapchainImageExtent;
            renderSpheresInfo.layerCount = 1;
            renderSpheresInfo.colorAttachmentCount = 1;
            renderSpheresInfo.pColorAttachments = &renderSpheresAttachmentInfo;
            renderSpheresInfo.pDepthAttachment = &depthAttachmentInfo;
        }

        vkCmdBeginRendering(currentCmdBuffer, &renderSpheresInfo);

        

        // Bind the graphics pipeline
        vkCmdBindPipeline(currentCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app.GetIblPipeline());

        vkCmdSetViewport(currentCmdBuffer, 0, 1, &viewport);
        vkCmdSetScissor(currentCmdBuffer, 0, 1, &scissor);

        VkDeviceSize vbOffset = 0;
        vkCmdBindVertexBuffers(currentCmdBuffer, 0, 1, &vertBuffer, &vbOffset);
        vkCmdBindIndexBuffer(currentCmdBuffer, idxBuffer, 0, VK_INDEX_TYPE_UINT32);

        float maxMipLevels = static_cast<float>(app.GetMaxMipLevel());
        vkCmdPushConstants(currentCmdBuffer,
                           app.GetIblPipelineLayout(),
                           VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(float), &maxMipLevels);

        vkCmdDrawIndexed(currentCmdBuffer, app.GetIdxCnt(), 14, 0, 0, 0);

        vkCmdEndRendering(currentCmdBuffer);

        // Transform the swapchain image layout from render target to present.
        // Transform the layout of the swapchain from undefined to render target.
        VkImageMemoryBarrier swapchainPresentTransBarrier{};
        {
            swapchainPresentTransBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            swapchainPresentTransBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            swapchainPresentTransBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            swapchainPresentTransBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            swapchainPresentTransBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            swapchainPresentTransBarrier.image = app.GetSwapchainColorImage();
            swapchainPresentTransBarrier.subresourceRange = swapchainPresentSubResRange;
        }

        vkCmdPipelineBarrier(currentCmdBuffer,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &swapchainPresentTransBarrier);

        VK_CHECK(vkEndCommandBuffer(currentCmdBuffer));

        app.GfxCmdBufferFrameSubmitAndPresent();

        app.FrameEnd();
    }
}
