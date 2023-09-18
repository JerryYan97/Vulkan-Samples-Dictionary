#include "vk_mem_alloc.h"

#include "PBRIBLApp.h"
#include "../../../SharedLibrary/Utils/VulkanDbgUtils.h"
#include "../../../SharedLibrary/Utils/CmdBufUtils.h"

#include <vulkan/vulkan.h>

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

    // Send image and buffer data to GPU:
    // - Copy background cubemap to vulkan image;
    // - Copy Camera parameters to the GPU buffer;
    // - Copy IBL images to vulkan images;
    {
        // Shared resources
        VmaAllocator* pAllocator = app.GetVmaAllocator();
        VkCommandBuffer stagingCmdBuffer = app.GetGfxCmdBuffer(0);
        VkQueue gfxQueue = app.GetGfxQueue();
        VkExtent2D backgroundImgExtent = app.GetHdrImgExtent();
        VkDevice device = app.GetVkDevice();

        // Background cubemap image info
        ImgInfo backgroundCubemapImgInfo = app.GetBackgroundCubemapInfo();
        VkExtent2D backgroundCubemapExtent = app.GetHdrImgExtent();
        VkImage backgroundCubemapImage = app.GetCubeMapImage();
        uint32_t backgroundCubemapDwords = 3 * backgroundCubemapExtent.width * backgroundCubemapExtent.height;

        // Cubemap's 6 layers SubresourceRange
        VkImageSubresourceRange backgroundCubemapSubResRange{};
        {
            backgroundCubemapSubResRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            backgroundCubemapSubResRange.baseMipLevel = 0;
            backgroundCubemapSubResRange.levelCount = 1;
            backgroundCubemapSubResRange.baseArrayLayer = 0;
            backgroundCubemapSubResRange.layerCount = 6;
        }

        // Copy the data from buffer to the image
        // - Our tool outputs vStrip, which is more convenient for IO. This example also uses vStrip.
        VkBufferImageCopy backgroundBufToImgCopy{};
        {
            VkExtent3D extent{};
            {
                extent.width = backgroundImgExtent.width;
                extent.height = backgroundImgExtent.height / 6;
                extent.depth = 1;
            }

            backgroundBufToImgCopy.bufferRowLength = backgroundImgExtent.width;
            backgroundBufToImgCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            backgroundBufToImgCopy.imageSubresource.mipLevel = 0;
            backgroundBufToImgCopy.imageSubresource.baseArrayLayer = 0;
            backgroundBufToImgCopy.imageSubresource.layerCount = 6;
            backgroundBufToImgCopy.imageExtent = extent;
        }

        SharedLib::SendImgDataToGpu(stagingCmdBuffer, 
                                    device,
                                    gfxQueue,
                                    backgroundCubemapImgInfo.pData,
                                    backgroundCubemapDwords * sizeof(float),
                                    app.GetCubeMapImage(),
                                    backgroundCubemapSubResRange,
                                    VK_IMAGE_LAYOUT_UNDEFINED,
                                    backgroundBufToImgCopy,
                                    *pAllocator);

        // - In the `cmftStudio`, you can choose hStrip. The code below is an example of using the hStrip.
        // - The buffer data of the image cannot be interleaved (The data of a separate image should be continues in the buffer address space.)
        // - However, our cubemap data (hStrip) is interleaved. 
        // - So, we have multiple choices to put them into the cubemap image. Here, I choose to offset the buffer starting point, specify the
        // -     long row length and copy that for 6 times.
        /*
        VkBufferImageCopy hdrBufToImgCopies[6];
        memset(hdrBufToImgCopies, 0, sizeof(hdrBufToImgCopies));
        for (uint32_t i = 0; i < 6; i++)
        {
            VkExtent3D extent{};
            {
                extent.width = hdrImgExtent.width / 6;
                extent.height = hdrImgExtent.height;
                extent.depth = 1;
            }

            hdrBufToImgCopies[i].bufferRowLength = hdrImgExtent.width;
            // hdrBufToImgCopies[i].bufferImageHeight = hdrImgExtent.height;
            hdrBufToImgCopies[i].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            hdrBufToImgCopies[i].imageSubresource.mipLevel = 0;
            hdrBufToImgCopies[i].imageSubresource.baseArrayLayer = i;
            hdrBufToImgCopies[i].imageSubresource.layerCount = 1;

            hdrBufToImgCopies[i].imageExtent = extent;
            // In the unit of bytes:
            hdrBufToImgCopies[i].bufferOffset = i * (hdrImgExtent.width / 6) * sizeof(float) * 3;
        }

        vkCmdCopyBufferToImage(
            stagingCmdBuffer,
            stagingBuffer,
            cubeMapImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            6, hdrBufToImgCopies);
        */

        VkCommandBufferBeginInfo beginInfo{};
        {
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        }
        VK_CHECK(vkBeginCommandBuffer(stagingCmdBuffer, &beginInfo));


        // Transform the layout of the image to shader access resource
        VkImageMemoryBarrier hdrDstToShaderBarrier{};
        {
            hdrDstToShaderBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            hdrDstToShaderBarrier.image = app.GetCubeMapImage();
            hdrDstToShaderBarrier.subresourceRange = backgroundCubemapSubResRange;
            hdrDstToShaderBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            hdrDstToShaderBarrier.dstAccessMask = VK_ACCESS_NONE;
            hdrDstToShaderBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            hdrDstToShaderBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        vkCmdPipelineBarrier(
            stagingCmdBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &hdrDstToShaderBarrier);

        // End the command buffer and submit the packets
        vkEndCommandBuffer(stagingCmdBuffer);

        SharedLib::SubmitCmdBufferAndWait(device, gfxQueue, stagingCmdBuffer);

        // Copy camera data to ubo buffer
        for (uint32_t i = 0; i < SharedLib::MAX_FRAMES_IN_FLIGHT; i++)
        {
            app.SendCameraDataToBuffer(i);
        }

        // Copy IBL images to VkImage
    }

    // Main Loop
    // Two draws. First draw draws triangle into an image with window 1 window size.
    // Second draw draws GUI. GUI would use the image drawn from the first draw.
    while (!app.WindowShouldClose())
    {
        VkDevice device = app.GetVkDevice();
        VkFence inFlightFence = app.GetCurrentFrameFence();
        VkCommandBuffer currentCmdBuffer = app.GetCurrentFrameGfxCmdBuffer();
        VkDescriptorSet currentSkyboxPipelineDesSet0 = app.GetSkyboxCurrentFrameDescriptorSet0();
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

        VkRenderingAttachmentInfoKHR renderAttachmentInfo{};
        {
            renderAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
            renderAttachmentInfo.imageView = app.GetSwapchainColorImageView(imageIndex);
            renderAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
            renderAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            renderAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            renderAttachmentInfo.clearValue = clearColor;
        }

        VkRenderingInfoKHR renderInfo{};
        {
            renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
            renderInfo.renderArea.offset = { 0, 0 };
            renderInfo.renderArea.extent = swapchainImageExtent;
            renderInfo.layerCount = 1;
            renderInfo.colorAttachmentCount = 1;
            renderInfo.pColorAttachments = &renderAttachmentInfo;
        }

        vkCmdBeginRendering(currentCmdBuffer, &renderInfo);

        // Bind the skybox pipeline descriptor sets
        vkCmdBindDescriptorSets(currentCmdBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS, 
                                app.GetSkyboxPipelineLayout(), 
                                0, 1, &currentSkyboxPipelineDesSet0, 0, NULL);

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
            vkCmdSetScissor(currentCmdBuffer, 0, 1, &scissor);
        }

        vkCmdDraw(currentCmdBuffer, 6, 1, 0, 0);

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

        app.FrameEnd();
    }
}
