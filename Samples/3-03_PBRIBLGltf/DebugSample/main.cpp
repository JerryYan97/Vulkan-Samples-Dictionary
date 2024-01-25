#include "vk_mem_alloc.h"

#include "PBRIBLGltfApp.h"
#include "../../../SharedLibrary/Utils/VulkanDbgUtils.h"
#include "../../../SharedLibrary/Utils/CmdBufUtils.h"

#include "renderdoc_app.h"
#include <vulkan/vulkan.h>
#include <Windows.h>
#include <cassert>

// TODO: We can design a queue to hold all transfer barriers and do them all-together.
int main()
{
    PBRIBLGltfApp app;
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

    // RenderDoc debug starts
    RENDERDOC_API_1_6_0* rdoc_api = NULL;
    {
        if (HMODULE mod = GetModuleHandleA("renderdoc.dll"))
        {
            pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
            int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0, (void**)&rdoc_api);
            assert(ret == 1);
        }
    }

    // Send image and buffer data to GPU:
    // - Copy background cubemap to vulkan image;
    // - Copy Camera parameters to the GPU buffer;
    // - Copy IBL images to vulkan images;
    // - Copy model textures to vulkan images;
    const std::vector<Mesh>& gltfMeshes = app.GetModelMeshes();
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

        // Send model's textures to GPU
        for (const auto& mesh : gltfMeshes)
        {
            // Base color
            VkBufferImageCopy baseColorBufToImgCopy{};
            {
                VkExtent3D extent{};
                {
                    extent.width = mesh.baseColorTex.pixWidth;
                    extent.height = mesh.baseColorTex.pixWidth;
                    extent.depth = 1;
                }

                baseColorBufToImgCopy.bufferRowLength = extent.width;
                baseColorBufToImgCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                baseColorBufToImgCopy.imageSubresource.mipLevel = 0;
                baseColorBufToImgCopy.imageSubresource.baseArrayLayer = 0;
                baseColorBufToImgCopy.imageSubresource.layerCount = 1;
                baseColorBufToImgCopy.imageExtent = extent;
            }

            SharedLib::SendImgDataToGpu(stagingCmdBuffer,
                                        device,
                                        gfxQueue,
                                        (void*) mesh.baseColorTex.dataVec.data(),
                                        mesh.baseColorTex.dataVec.size(),
                                        mesh.baseColorImg,
                                        tex2dSubResRange,
                                        VK_IMAGE_LAYOUT_UNDEFINED,
                                        baseColorBufToImgCopy,
                                        *pAllocator);

            // Normal
            VkBufferImageCopy normalBufToImgCopy{};
            {
                VkExtent3D extent{};
                {
                    extent.width = mesh.normalTex.pixWidth;
                    extent.height = mesh.normalTex.pixWidth;
                    extent.depth = 1;
                }

                normalBufToImgCopy.bufferRowLength = extent.width;
                normalBufToImgCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                normalBufToImgCopy.imageSubresource.mipLevel = 0;
                normalBufToImgCopy.imageSubresource.baseArrayLayer = 0;
                normalBufToImgCopy.imageSubresource.layerCount = 1;
                normalBufToImgCopy.imageExtent = extent;
            }

            SharedLib::SendImgDataToGpu(stagingCmdBuffer,
                                        device,
                                        gfxQueue,
                                        (void*) mesh.normalTex.dataVec.data(),
                                        mesh.normalTex.dataVec.size(),
                                        mesh.normalImg,
                                        tex2dSubResRange,
                                        VK_IMAGE_LAYOUT_UNDEFINED,
                                        normalBufToImgCopy,
                                        *pAllocator);

            // Roughness metallic
            VkBufferImageCopy roughnessMetallicBufToImgCopy{};
            {
                VkExtent3D extent{};
                {
                    extent.width = mesh.metallicRoughnessTex.pixWidth;
                    extent.height = mesh.metallicRoughnessTex.pixWidth;
                    extent.depth = 1;
                }

                roughnessMetallicBufToImgCopy.bufferRowLength = extent.width;
                roughnessMetallicBufToImgCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                roughnessMetallicBufToImgCopy.imageSubresource.mipLevel = 0;
                roughnessMetallicBufToImgCopy.imageSubresource.baseArrayLayer = 0;
                roughnessMetallicBufToImgCopy.imageSubresource.layerCount = 1;
                roughnessMetallicBufToImgCopy.imageExtent = extent;
            }

            SharedLib::SendImgDataToGpu(stagingCmdBuffer,
                                        device,
                                        gfxQueue,
                                        (void*) mesh.metallicRoughnessTex.dataVec.data(),
                                        mesh.metallicRoughnessTex.dataVec.size(),
                                        mesh.metallicRoughnessImg,
                                        tex2dSubResRange,
                                        VK_IMAGE_LAYOUT_UNDEFINED,
                                        roughnessMetallicBufToImgCopy,
                                        *pAllocator);

            // Occlusion
            VkBufferImageCopy occlusionBufToImgCopy{};
            {
                VkExtent3D extent{};
                {
                    extent.width = mesh.occlusionTex.pixWidth;
                    extent.height = mesh.occlusionTex.pixWidth;
                    extent.depth = 1;
                }

                occlusionBufToImgCopy.bufferRowLength = extent.width;
                occlusionBufToImgCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                occlusionBufToImgCopy.imageSubresource.mipLevel = 0;
                occlusionBufToImgCopy.imageSubresource.baseArrayLayer = 0;
                occlusionBufToImgCopy.imageSubresource.layerCount = 1;
                occlusionBufToImgCopy.imageExtent = extent;
            }

            SharedLib::SendImgDataToGpu(stagingCmdBuffer,
                                        device,
                                        gfxQueue,
                                        (void*) mesh.occlusionTex.dataVec.data(),
                                        mesh.occlusionTex.dataVec.size(),
                                        mesh.occlusionImg,
                                        tex2dSubResRange,
                                        VK_IMAGE_LAYOUT_UNDEFINED,
                                        occlusionBufToImgCopy,
                                        *pAllocator);
        }

        // Copy camera data to ubo buffer
        for (uint32_t i = 0; i < app.GetSwapchainImgCnt(); i++)
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

        // NOTE: This barrier is only needed at the beginning of the swapchain creation.
        VkImageMemoryBarrier swapchainDepthTargetTransBarrier{};
        {
            swapchainDepthTargetTransBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            swapchainDepthTargetTransBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            swapchainDepthTargetTransBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            swapchainDepthTargetTransBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            swapchainDepthTargetTransBarrier.image = app.GetSwapchainDepthImage();
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
        VkRenderingAttachmentInfoKHR depthModelAttachmentInfo{};
        {
            depthModelAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
            depthModelAttachmentInfo.imageView = app.GetSwapchainDepthImageView();
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
                                    app.GetSwapchainDepthImage(),
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
            swapchainTransDstToDepthTargetBarrier.image = app.GetSwapchainDepthImage();
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

        if (rdoc_api)
        {
            std::cout << "Frame capture starts." << std::endl;
            rdoc_api->StartFrameCapture(NULL, NULL);
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

        // app.DumpRenderedFrame(currentCmdBuffer);

        app.FrameEnd();
    }

    // End RenderDoc debug
    if (rdoc_api)
    {
        std::cout << "Frame capture ends." << std::endl;
        rdoc_api->EndFrameCapture(NULL, NULL);
    }
}
