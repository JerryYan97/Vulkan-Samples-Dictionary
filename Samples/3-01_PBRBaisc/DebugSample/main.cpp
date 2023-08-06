// #define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "PBRBasicApp.h"
#include "../../3-00_SharedLibrary/VulkanDbgUtils.h"

#include <vulkan/vulkan.h>

// TODO1: Make the application, realtime swapchain application class for the Level 3 examples.

int main()
{
    PBRBasicApp app;
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

        VkDescriptorSet currentSkyboxPipelineDesSet0 = app.GetCurrentFrameDescriptorSet0();
        VkExtent2D swapchainImageExtent = app.GetSwapchainImageExtent();

        std::cout << "swapchain width: " << swapchainImageExtent.width << "; swapchain height: " << swapchainImageExtent.height << std::endl;

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

        VkRenderingAttachmentInfoKHR colorAttachmentInfo{};
        {
            colorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
            colorAttachmentInfo.imageView = app.GetSwapchainColorImageView(imageIndex);
            colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
            colorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachmentInfo.clearValue = clearColor;
        }

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
            renderInfo.colorAttachmentCount = 1;
            renderInfo.pColorAttachments = &colorAttachmentInfo;
            renderInfo.pDepthAttachment = &depthAttachmentInfo;
        }

        vkCmdBeginRendering(currentCmdBuffer, &renderInfo);

        // Bind the pipeline descriptor sets
        vkCmdBindDescriptorSets(currentCmdBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                app.GetPipelineLayout(),
                                0, 1, &currentSkyboxPipelineDesSet0, 0, NULL);

        // Bind the graphics pipeline
        vkCmdBindPipeline(currentCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app.GetPipeline());

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

        vkCmdDrawIndexed(currentCmdBuffer, app.GetIdxCnt(), 4, 0, 0, 0);

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

    /*
    
    VkImageSubresourceRange swapchainPresentSubResRange{};
    {
        swapchainPresentSubResRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        swapchainPresentSubResRange.baseMipLevel = 0;
        swapchainPresentSubResRange.levelCount = 1;
        swapchainPresentSubResRange.baseArrayLayer = 0;
        swapchainPresentSubResRange.layerCount = 1;
    }

    // Send the HDR cubemap image to GPU:
    // - Copy RAM to GPU staging buffer;
    // - Copy buffer to image;
    // - Copy Camera parameters to the GPU buffer;
    {
        // Create the staging buffer
        VkBuffer      stagingBuffer;
        VmaAllocation stagingBufAlloc;
        VmaAllocator* pAllocator = app.GetVmaAllocator();
        VkCommandBuffer stagingCmdBuffer = app.GetGfxCmdBuffer(0);
        VkExtent2D hdrImgExtent = app.GetHdrImgExtent();
        VkImage cubeMapImage = app.GetCubeMapImage();
        VkFence stagingFence = app.GetFence(0);

        app.CreateVmaVkBuffer(VMA_MEMORY_USAGE_AUTO, 
                              VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                              VK_SHARING_MODE_EXCLUSIVE,
                              VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                              app.GetHdrByteNum(),
                              &stagingBuffer,
                              &stagingBufAlloc);

        // Copy the RAM data to the staging buffer
        app.CopyRamDataToGpuBuffer(app.GetHdrDataPointer(), stagingBuffer, stagingBufAlloc, app.GetHdrByteNum());

        // Send staging buffer data to the GPU image.
        VkCommandBufferBeginInfo beginInfo{};
        {
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        }
        VK_CHECK(vkBeginCommandBuffer(stagingCmdBuffer, &beginInfo));

        // Cubemap's 6 layers SubresourceRange
        VkImageSubresourceRange cubemapSubResRange{};
        {
            cubemapSubResRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            cubemapSubResRange.baseMipLevel = 0;
            cubemapSubResRange.levelCount = 1;
            cubemapSubResRange.baseArrayLayer = 0;
            cubemapSubResRange.layerCount = 6;
        }

        // Transform the layout of the image to copy destination
        VkImageMemoryBarrier hdrUndefToDstBarrier{};
        {
            hdrUndefToDstBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            hdrUndefToDstBarrier.image = cubeMapImage;
            hdrUndefToDstBarrier.subresourceRange = cubemapSubResRange;
            hdrUndefToDstBarrier.srcAccessMask = 0;
            hdrUndefToDstBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            hdrUndefToDstBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            hdrUndefToDstBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        }
        
        vkCmdPipelineBarrier(
            stagingCmdBuffer,
            VK_PIPELINE_STAGE_HOST_BIT, 
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &hdrUndefToDstBarrier);

        // Copy the data from buffer to the image
        // - The buffer data of the image cannot be interleaved (The data of a separate image should be continues in the buffer address space.)
        // - However, our cubemap data (hStrip) is interleaved. 
        // - So, we have multiple choices to put them into the cubemap image. Here, I choose to offset the buffer starting point, specify the
        // -     long row length and copy that for 6 times.
        // - We are using the hStrip skybox here. In the `cmftStudio`, we can also choose the vStrip here, which is more convenient, but we just
        // -     use the hStrip here since it's more educational.
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
            hdrBufToImgCopies[i].bufferImageHeight = hdrImgExtent.height;
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
        
        // Transform the layout of the image to shader access resource
        VkImageMemoryBarrier hdrDstToShaderBarrier{};
        {
            hdrDstToShaderBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            hdrDstToShaderBarrier.image = cubeMapImage;
            hdrDstToShaderBarrier.subresourceRange = cubemapSubResRange;
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

        app.SubmitCmdBufToGfxQueue(stagingCmdBuffer, stagingFence);

        // Destroy temp resources
        vmaDestroyBuffer(*pAllocator, stagingBuffer, stagingBufAlloc);

        // Copy camera data to ubo buffer
        for (uint32_t i = 0; i < SharedLib::MAX_FRAMES_IN_FLIGHT; i++)
        {
            app.SendCameraDataToBuffer(i);
        }
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
            swapchainRenderTargetTransBarrier.image = app.GetSwapchainImage(imageIndex);
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
            renderAttachmentInfo.imageView = app.GetSwapchainImageView(imageIndex);
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
            swapchainPresentTransBarrier.image = app.GetSwapchainImage(imageIndex);
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
    */
}
