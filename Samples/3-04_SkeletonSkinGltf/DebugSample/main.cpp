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
    // std::string gltfPath = sourcePath + "/../data/QuadSkin/QuadSkin.gltf";
    // std::string gltfPath = sourcePath + "/../data/SimpleSkin/SimpleSkin.gltf";
    // std::string gltfPath = sourcePath + "/../data/CesiumMan/CesiumMan.gltf";
    // std::string gltfPath = sourcePath + "/../data/RiggedSimple/RiggedSimple.gltf";
    std::string gltfPath = sourcePath + "/../data/RiggedFigure/RiggedFigure.gltf";

    SkinAnimGltfApp app(iblPath, gltfPath, true, 2.f, 2.f, 1.f);
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

        // Poll Event, update camera data.
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
            swapchainDepthTargetTransBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
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

        VkRenderingAttachmentInfoKHR renderSpheresAttachmentInfo{};
        {
            renderSpheresAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
            renderSpheresAttachmentInfo.imageView = app.GetSwapchainColorImageView();
            renderSpheresAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
            renderSpheresAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
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
            depthModelAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthModelAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            depthModelAttachmentInfo.clearValue = depthClearVal;
        }

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

        // Set the scissor
        VkRect2D scissor{};
        {
            scissor.offset = { 0, 0 };
            scissor.extent = swapchainImageExtent;
        }

        // Render models' meshes
        SkeletalMesh* pSkeletalMesh = app.GetSkeletalMeshPtr();

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

        app.CmdPushSkeletonSkinRenderingDescriptors(currentCmdBuffer);

        // Bind the graphics pipeline
        vkCmdBindPipeline(currentCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app.GetSkinAimPipeline());

        vkCmdSetViewport(currentCmdBuffer, 0, 1, &viewport);
        vkCmdSetScissor(currentCmdBuffer, 0, 1, &scissor);

        VkDeviceSize vbOffset = 0;
        vkCmdBindVertexBuffers(currentCmdBuffer, 0, 1, &pSkeletalMesh->mesh.vertBuffer.buffer, &vbOffset);
        // Assume uint16_t input idx data
        vkCmdBindIndexBuffer(currentCmdBuffer, pSkeletalMesh->mesh.idxBuffer.buffer, 0, VK_INDEX_TYPE_UINT16);

        std::vector<float> pushVertConstantData = app.GetSkinAnimVertPushConsant();
        std::vector<float> pushFragConstantData = app.GetSkinAnimFragPushConstant();

        vkCmdPushConstants(currentCmdBuffer,
            app.GetSkinAimPipelineLayout(),
            VK_SHADER_STAGE_VERTEX_BIT,
            0, 16 * sizeof(float), pushVertConstantData.data());

        vkCmdPushConstants(currentCmdBuffer,
            app.GetSkinAimPipelineLayout(),
            VK_SHADER_STAGE_FRAGMENT_BIT,
            16 * sizeof(float), 4 * sizeof(float), pushFragConstantData.data());

        vkCmdDrawIndexed(currentCmdBuffer, app.GetMeshIdxCnt(), 1, 0, 0, 0);

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
