#include "vk_mem_alloc.h"

#include "PBRIBLApp.h"
#include "../../../SharedLibrary/Utils/VulkanDbgUtils.h"
#include "../../../SharedLibrary/Utils/CmdBufUtils.h"

#include "renderdoc_app.h"
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

    // RenderDoc debug starts
    RENDERDOC_API_1_6_0* rdoc_api = NULL;
    {
        if (HMODULE mod = GetModuleHandleA("renderdoc.dll"))
        {
            pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
            int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0, (void**)&rdoc_api);
            assert(ret == 1);
        }

        if (rdoc_api)
        {
            std::cout << "Frame capture starts." << std::endl;
            rdoc_api->StartFrameCapture(NULL, NULL);
        }
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
        const uint32_t backgroundCubemapDwords = 3 * backgroundCubemapExtent.width * backgroundCubemapExtent.height;

        // Cubemap's 6 layers SubresourceRange
        VkImageSubresourceRange cubemap1MipSubResRange{};
        {
            cubemap1MipSubResRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            cubemap1MipSubResRange.baseMipLevel = 0;
            cubemap1MipSubResRange.levelCount = 1;
            cubemap1MipSubResRange.baseArrayLayer = 0;
            cubemap1MipSubResRange.layerCount = 6;
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
                                    cubemap1MipSubResRange,
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

        // Copy IBL images to VkImage
        // Diffuse Irradiance
        ImgInfo diffIrrImgInfo = app.GetDiffuseIrradianceImgInfo();
        VkImage diffIrrCubemap = app.GetDiffuseIrradianceCubemap();
        const uint32_t diffIrrDwords = diffIrrImgInfo.pixWidth * diffIrrImgInfo.pixHeight * 3;

        VkBufferImageCopy diffIrrBufToImgCopy{};
        {
            VkExtent3D extent{};
            {
                extent.width = diffIrrImgInfo.pixWidth;
                extent.height = diffIrrImgInfo.pixWidth;
                extent.depth = 1;
            }

            diffIrrBufToImgCopy.bufferRowLength = extent.width;
            diffIrrBufToImgCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            diffIrrBufToImgCopy.imageSubresource.mipLevel = 0;
            diffIrrBufToImgCopy.imageSubresource.baseArrayLayer = 0;
            diffIrrBufToImgCopy.imageSubresource.layerCount = 6;
            diffIrrBufToImgCopy.imageExtent = extent;
        }

        SharedLib::SendImgDataToGpu(stagingCmdBuffer, 
                                    device,
                                    gfxQueue,
                                    diffIrrImgInfo.pData,
                                    diffIrrDwords * sizeof(float),
                                    diffIrrCubemap,
                                    cubemap1MipSubResRange,
                                    VK_IMAGE_LAYOUT_UNDEFINED,
                                    diffIrrBufToImgCopy,
                                    *pAllocator);

        // Prefilter environment
        std::vector<ImgInfo> prefilterEnvMips = app.GetPrefilterEnvImgsInfo();
        VkImage prefilterEnvCubemap = app.GetPrefilterEnvCubemap();
        const uint32_t mipLevelCnt = prefilterEnvMips.size();

        for (uint32_t i = 0; i < prefilterEnvMips.size(); i++)
        {
            ImgInfo mipImgInfo = prefilterEnvMips[i];
            uint32_t mipDwordsCnt = 3 * mipImgInfo.pixWidth * mipImgInfo.pixHeight;

            VkImageSubresourceRange prefilterEnvMipISubResRange{};
            {
                prefilterEnvMipISubResRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                prefilterEnvMipISubResRange.baseMipLevel = i;
                prefilterEnvMipISubResRange.levelCount = 1;
                prefilterEnvMipISubResRange.baseArrayLayer = 0;
                prefilterEnvMipISubResRange.layerCount = 6;
            }

            VkBufferImageCopy prefilterEnvMipIBufToImgCopy{};
            {
                VkExtent3D extent{};
                {
                    extent.width = mipImgInfo.pixWidth;
                    extent.height = mipImgInfo.pixWidth;
                    extent.depth = 1;
                }

                prefilterEnvMipIBufToImgCopy.bufferRowLength = mipImgInfo.pixWidth;
                prefilterEnvMipIBufToImgCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                prefilterEnvMipIBufToImgCopy.imageSubresource.mipLevel = i;
                prefilterEnvMipIBufToImgCopy.imageSubresource.baseArrayLayer = 0;
                prefilterEnvMipIBufToImgCopy.imageSubresource.layerCount = 6;
                prefilterEnvMipIBufToImgCopy.imageExtent = extent;
            }

            SharedLib::SendImgDataToGpu(stagingCmdBuffer,
                                        device,
                                        gfxQueue,
                                        mipImgInfo.pData,
                                        mipDwordsCnt * sizeof(float),
                                        prefilterEnvCubemap,
                                        prefilterEnvMipISubResRange,
                                        VK_IMAGE_LAYOUT_UNDEFINED,
                                        prefilterEnvMipIBufToImgCopy,
                                        *pAllocator);
        }

        // Environment BRDF
        ImgInfo envBrdfImgInfo = app.GetEnvBrdfImgInfo();
        VkImage envBrdfImg = app.GetEnvBrdf();
        const uint32_t envBrdfDwordsCnt = 3 * envBrdfImgInfo.pixHeight * envBrdfImgInfo.pixWidth;

        // The envBrdf 2D texture SubresourceRange
        VkImageSubresourceRange tex2dSubResRange{};
        {
            tex2dSubResRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            tex2dSubResRange.baseMipLevel = 0;
            tex2dSubResRange.levelCount = 1;
            tex2dSubResRange.baseArrayLayer = 0;
            tex2dSubResRange.layerCount = 1;
        }

        VkBufferImageCopy envBrdfBufToImgCopy{};
        {
            VkExtent3D extent{};
            {
                extent.width = envBrdfImgInfo.pixWidth;
                extent.height = envBrdfImgInfo.pixWidth;
                extent.depth = 1;
            }

            envBrdfBufToImgCopy.bufferRowLength = extent.width;
            envBrdfBufToImgCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            envBrdfBufToImgCopy.imageSubresource.mipLevel = 0;
            envBrdfBufToImgCopy.imageSubresource.baseArrayLayer = 0;
            envBrdfBufToImgCopy.imageSubresource.layerCount = 1;
            envBrdfBufToImgCopy.imageExtent = extent;
        }

        SharedLib::SendImgDataToGpu(stagingCmdBuffer, 
                                    device,
                                    gfxQueue,
                                    envBrdfImgInfo.pData,
                                    envBrdfDwordsCnt * sizeof(float),
                                    envBrdfImg,
                                    tex2dSubResRange,
                                    VK_IMAGE_LAYOUT_UNDEFINED,
                                    envBrdfBufToImgCopy,
                                    *pAllocator);


        // Transform all images layout to shader read optimal.
        VkCommandBufferBeginInfo beginInfo{};
        {
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        }
        VK_CHECK(vkBeginCommandBuffer(stagingCmdBuffer, &beginInfo));

        // Transform the layout of the image to shader access resource
        VkImageMemoryBarrier imgResMemBarriers[4] = {};
        {
            // Background cubemap
            imgResMemBarriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imgResMemBarriers[0].image = app.GetCubeMapImage();
            imgResMemBarriers[0].subresourceRange = cubemap1MipSubResRange;
            imgResMemBarriers[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imgResMemBarriers[0].dstAccessMask = VK_ACCESS_NONE;
            imgResMemBarriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imgResMemBarriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            // Diffuse irradiance
            imgResMemBarriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imgResMemBarriers[1].image = diffIrrCubemap;
            imgResMemBarriers[1].subresourceRange = cubemap1MipSubResRange;
            imgResMemBarriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imgResMemBarriers[1].dstAccessMask = VK_ACCESS_NONE;
            imgResMemBarriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imgResMemBarriers[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            // Prefilter environment map
            imgResMemBarriers[2].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imgResMemBarriers[2].image = prefilterEnvCubemap;
            {
                imgResMemBarriers[2].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                imgResMemBarriers[2].subresourceRange.baseArrayLayer = 0;
                imgResMemBarriers[2].subresourceRange.layerCount = 6;
                imgResMemBarriers[2].subresourceRange.baseMipLevel = 0;
                imgResMemBarriers[2].subresourceRange.levelCount = mipLevelCnt;
            }
            imgResMemBarriers[2].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imgResMemBarriers[2].dstAccessMask = VK_ACCESS_NONE;
            imgResMemBarriers[2].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imgResMemBarriers[2].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            // Environment brdf
            imgResMemBarriers[3].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imgResMemBarriers[3].image = envBrdfImg;
            imgResMemBarriers[3].subresourceRange = tex2dSubResRange;
            imgResMemBarriers[3].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imgResMemBarriers[3].dstAccessMask = VK_ACCESS_NONE;
            imgResMemBarriers[3].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imgResMemBarriers[3].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        vkCmdPipelineBarrier(
            stagingCmdBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, nullptr,
            0, nullptr,
            4, imgResMemBarriers);

        // End the command buffer and submit the packets
        vkEndCommandBuffer(stagingCmdBuffer);

        SharedLib::SubmitCmdBufferAndWait(device, gfxQueue, stagingCmdBuffer);

        // Copy camera data to ubo buffer
        for (uint32_t i = 0; i < SharedLib::MAX_FRAMES_IN_FLIGHT; i++)
        {
            app.SendCameraDataToBuffer(i);
        }
    }

    // End RenderDoc debug
    if (rdoc_api)
    {
        std::cout << "Frame capture ends." << std::endl;
        rdoc_api->EndFrameCapture(NULL, NULL);
    }

    /**/

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

        // Render background cubemap
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

        // Render spheres


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
