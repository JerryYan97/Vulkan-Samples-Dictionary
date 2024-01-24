#include "vk_mem_alloc.h"

#include "SkeletonSkinAnim.h"
#include "../../../SharedLibrary/Utils/VulkanDbgUtils.h"
#include "../../../SharedLibrary/Utils/CmdBufUtils.h"

#include <vulkan/vulkan.h>
#include <Windows.h>
#include <cassert>

int main()
{
    // Path initialization.
    std::string sourcePath = SOURCE_PATH;
    std::string iblPath = sourcePath + "/../data/ibl";
    std::string gltfPath = sourcePath + "/../data/SimpleSkin/SimpleSkin.gltf";
    // std::string gltfPath = sourcePath + "/../data/CesiumMan/CesiumMan.gltf";

    SkinAnimGltfApp app(iblPath, gltfPath);
    app.AppInit();

    VkImageSubresourceRange swapchainPresentSubResRange{};
    {
        swapchainPresentSubResRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        swapchainPresentSubResRange.baseMipLevel = 0;
        swapchainPresentSubResRange.levelCount = 1;
        swapchainPresentSubResRange.baseArrayLayer = 0;
        swapchainPresentSubResRange.layerCount = 1;
    }

    VkImageSubresourceRange swapchainDepthSubResRange{};
    {
        swapchainDepthSubResRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        swapchainDepthSubResRange.baseMipLevel = 0;
        swapchainDepthSubResRange.levelCount = 1;
        swapchainDepthSubResRange.baseArrayLayer = 0;
        swapchainDepthSubResRange.layerCount = 1;
    }

    // Main Loop
    // Two draws. First draw draws triangle into an image with window 1 window size.
    // Second draw draws GUI. GUI would use the image drawn from the first draw.
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

        // Update the camera according to mouse input and sent camera data to the UBO
        app.UpdateCameraAndGpuBuffer();

        // Transform the layout of the swapchain from undefined to render target.
        VkImageMemoryBarrier swapchainRenderTargetTransBarrier{};
        {
            swapchainRenderTargetTransBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            swapchainRenderTargetTransBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            swapchainRenderTargetTransBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            swapchainRenderTargetTransBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            swapchainRenderTargetTransBarrier.image = app.GetSwapchainColorImage(imageIndex);
            swapchainRenderTargetTransBarrier.subresourceRange = swapchainPresentSubResRange;
        }

        // NOTE: This barrier is only needed at the beginning of the swapchain creation.
        VkImageMemoryBarrier swapchainDepthTargetTransBarrier{};
        {
            swapchainDepthTargetTransBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            swapchainDepthTargetTransBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            swapchainDepthTargetTransBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            swapchainDepthTargetTransBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            swapchainDepthTargetTransBarrier.image = app.GetSwapchainDepthImage(imageIndex);
            swapchainDepthTargetTransBarrier.subresourceRange = swapchainDepthSubResRange;
        }

        VkImageMemoryBarrier swapchainImgTrans[2] = {
            swapchainRenderTargetTransBarrier, swapchainDepthTargetTransBarrier
        };

        vkCmdPipelineBarrier(
            currentCmdBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            0,
            0, nullptr,
            0, nullptr,
            2, swapchainImgTrans);

        // Draw the scene
        VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };

        VkRenderingAttachmentInfoKHR renderBackgroundAttachmentInfo{};
        {
            renderBackgroundAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
            renderBackgroundAttachmentInfo.imageView = app.GetSwapchainColorImageView(imageIndex);
            renderBackgroundAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
            renderBackgroundAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            renderBackgroundAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            renderBackgroundAttachmentInfo.clearValue = clearColor;
        }

        VkRenderingAttachmentInfoKHR renderSpheresAttachmentInfo{};
        {
            renderSpheresAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
            renderSpheresAttachmentInfo.imageView = app.GetSwapchainColorImageView(imageIndex);
            renderSpheresAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
            renderSpheresAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            renderSpheresAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            renderSpheresAttachmentInfo.clearValue = clearColor;
        }

        VkClearValue depthClearVal{};
        depthClearVal.depthStencil.depth = 0.f;
        VkRenderingAttachmentInfoKHR depthModelAttachmentInfo{};
        {
            depthModelAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
            depthModelAttachmentInfo.imageView = app.GetSwapchainDepthImageView(imageIndex);
            depthModelAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
            depthModelAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            depthModelAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            depthModelAttachmentInfo.clearValue = depthClearVal;
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
        app.CmdPushCubemapDescriptors(currentCmdBuffer);

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

        VkClearDepthStencilValue clearDepthStencilVal{};
        {
            clearDepthStencilVal.depth = 0;
            clearDepthStencilVal.stencil = 0;
        }

        vkCmdClearDepthStencilImage(currentCmdBuffer,
                                    app.GetSwapchainDepthImage(imageIndex),
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    &clearDepthStencilVal, 1, &swapchainDepthSubResRange);

        // Transfer the depth stencil swapchain image back to depth attachment
        VkImageMemoryBarrier swapchainTransDstToDepthTargetBarrier{};
        {
            swapchainTransDstToDepthTargetBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            swapchainTransDstToDepthTargetBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                                                  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            swapchainTransDstToDepthTargetBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            swapchainTransDstToDepthTargetBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            swapchainTransDstToDepthTargetBarrier.image = app.GetSwapchainDepthImage(imageIndex);
            swapchainTransDstToDepthTargetBarrier.subresourceRange = swapchainDepthSubResRange;
        }

        // Render models' meshes
        // Let IBL render in the color attachment after the skybox rendering completes.
        vkCmdPipelineBarrier(currentCmdBuffer,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &swapchainTransDstToDepthTargetBarrier);

        // for (const auto& mesh : gltfMeshes)
        for(uint32_t i = 0; i < gltfMeshes.size(); i++)
        {
            const auto& mesh = gltfMeshes[i];

            app.CmdPushIblModelRenderingDescriptors(currentCmdBuffer, mesh);

            VkRenderingInfoKHR renderSpheresInfo{};
            {
                renderSpheresInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
                renderSpheresInfo.renderArea.offset = { 0, 0 };
                renderSpheresInfo.renderArea.extent = swapchainImageExtent;
                renderSpheresInfo.layerCount = 1;
                renderSpheresInfo.colorAttachmentCount = 1;
                renderSpheresInfo.pColorAttachments = &renderSpheresAttachmentInfo;
                renderSpheresInfo.pDepthAttachment = &depthModelAttachmentInfo;
            }

            vkCmdBeginRendering(currentCmdBuffer, &renderSpheresInfo);

            // Bind the graphics pipeline
            vkCmdBindPipeline(currentCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app.GetIblPipeline());

            vkCmdSetViewport(currentCmdBuffer, 0, 1, &viewport);
            vkCmdSetScissor(currentCmdBuffer, 0, 1, &scissor);

            VkDeviceSize vbOffset = 0;
            vkCmdBindVertexBuffers(currentCmdBuffer, 0, 1, &mesh.modelVertBuffer, &vbOffset);
            // Assume uint16_t input idx data
            vkCmdBindIndexBuffer(currentCmdBuffer, mesh.modelIdxBuffer, 0, VK_INDEX_TYPE_UINT16);

            float maxMipLevels = static_cast<float>(app.GetMaxMipLevel());
            float cameraPos[3] = {};
            app.GetCameraPos(cameraPos);

            float pushConst[4] = { cameraPos[0], cameraPos[1], cameraPos[2], maxMipLevels };

            vkCmdPushConstants(currentCmdBuffer,
                app.GetIblPipelineLayout(),
                VK_SHADER_STAGE_FRAGMENT_BIT,
                0, 4 * sizeof(float), pushConst);

            vkCmdDrawIndexed(currentCmdBuffer, mesh.idxData.size(), 1, 0, 0, 0);

            vkCmdEndRendering(currentCmdBuffer);
        }

        // app.CmdCopyPresentImgToLogAnim(currentCmdBuffer, imageIndex);

        // Transform the swapchain image layout from render target to present.
        // Transform the layout of the swapchain from undefined to render target.
        VkImageMemoryBarrier swapchainPresentTransBarrier{};
        {
            swapchainPresentTransBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            swapchainPresentTransBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            swapchainPresentTransBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            swapchainPresentTransBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            swapchainPresentTransBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            swapchainPresentTransBarrier.image = app.GetSwapchainColorImage(imageIndex);
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

        // app.DumpRenderedFrame(currentCmdBuffer);

        app.FrameEnd();
    }
}
