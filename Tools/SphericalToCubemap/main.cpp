#include "SphericalToCubemap.h"
#include "args.hxx"
#include "../../SharedLibrary/Utils/CmdBufUtils.h"
#include "../../SharedLibrary/Utils/VulkanDbgUtils.h"

#include "renderdoc_app.h"
#include <Windows.h>
#include <cassert>

bool CheckImgValAbove1(
    float* pData,
    uint32_t width,
    uint32_t height)
{
    for (uint32_t i = 0; i < width * height * 3; i++)
    {
        if (pData[i] > 1.f)
        {
            return true;
        }
    }
    return false;
}

int main(
    int argc, 
    char** argv)
{
    args::ArgumentParser parser("This tool takes an equirectangular image as input and output the cubemap. If the input is ./img.hdr. The output will be ./img_cubemap.hdr", 
                                "E.g. SphericalToCubemap.exe --srcPath ./img.hdr");
    args::HelpFlag help(parser, "help", "Display this help menu", { 'h', "help" });
    args::CompletionFlag completion(parser, { "complete" });

    args::ValueFlag<std::string> inputPath(parser, "", "The input equirectangular image path.", { 'i', "srcPath"});

    try
    {
        parser.ParseCLI(argc, argv);
    }
    catch (const args::Completion& e)
    {
        std::cout << e.what();
        return 0;
    }
    catch (const args::Help&)
    {
        std::cout << parser;
        return 0;
    }
    catch (const args::ParseError& e)
    {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
        return 1;
    }

    // RenderDoc debug starts
    RENDERDOC_API_1_6_0* rdoc_api = NULL;
    if (HMODULE mod = GetModuleHandleA("renderdoc.dll"))
    {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
        int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0, (void**)&rdoc_api);
        assert(ret == 1);
    }

    if (rdoc_api)
    {
        rdoc_api->StartFrameCapture(NULL, NULL);
    }

    SphericalToCubemap app;
    app.ReadInHdri("C:\\JiaruiYan\\Projects\\OneFileVulkans\\Tools\\SphericalToCubemap\\data\\little_paris_eiffel_tower_4k.hdr");
    app.AppInit();

    if (CheckImgValAbove1(app.GetInputHdriData(), app.GetInputHdriWidth(), app.GetInputHdriHeight()))
    {
        std::cout << "The image has elements that are larger than 1.f." << std::endl;
    }
    else
    {
        std::cout << "The image doesn't have elements that are larger than 1.f." << std::endl;
    }

    // Common data used in the CmdBuffer filling process.
    VkCommandBuffer cmdBuffer = app.GetGfxCmdBuffer(0);
    VkQueue gfxQueue = app.GetGfxQueue();
    VkDevice device = app.GetVkDevice();
    VmaAllocator allocator = *app.GetVmaAllocator();
    VkDescriptorSet pipelineDescriptorSet = app.GetDescriptorSet();

    VkImageSubresourceRange cubemapSubResRange{};
    {
        cubemapSubResRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        cubemapSubResRange.baseMipLevel = 0;
        cubemapSubResRange.levelCount = 1;
        cubemapSubResRange.baseArrayLayer = 0;
        cubemapSubResRange.layerCount = 6;
    }

    // Send hdri data to its gpu objects through a staging buffer.
    {
        VkImageSubresourceRange copySubRange{};
        {
            copySubRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copySubRange.baseMipLevel = 0;
            copySubRange.levelCount = 1;
            copySubRange.baseArrayLayer = 0;
            copySubRange.layerCount = 1;
        }

        VkBufferImageCopy hdrBufToImgCopy{};
        {
            VkExtent3D extent{};
            {
                extent.width = app.GetInputHdriWidth();
                extent.height = app.GetInputHdriHeight();
                extent.depth = 1;
            }

            hdrBufToImgCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            hdrBufToImgCopy.imageSubresource.mipLevel = 0;
            hdrBufToImgCopy.imageSubresource.baseArrayLayer = 0;
            hdrBufToImgCopy.imageSubresource.layerCount = 1;

            hdrBufToImgCopy.imageExtent = extent;
        }

        SharedLib::CmdSendImgDataToGpu(cmdBuffer, 
                                       device,
                                       gfxQueue,
                                       app.GetInputHdriData(),
                                       3 * sizeof(float) * app.GetInputHdriWidth() * app.GetInputHdriHeight(),
                                       app.GetHdriImg(),
                                       copySubRange,
                                       hdrBufToImgCopy,
                                       allocator);
    }

    // Draw the Front, Back, Top, Bottom, Right, Left faces to the cubemap.
    {
        // Fill the command buffer
        VkCommandBufferBeginInfo beginInfo{};
        {
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        }
        VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo));

        // Transform the layout of the swapchain from undefined to render target.
        VkImageMemoryBarrier cubemapRenderTargetTransBarrier{};
        {
            cubemapRenderTargetTransBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            cubemapRenderTargetTransBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            cubemapRenderTargetTransBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            cubemapRenderTargetTransBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            cubemapRenderTargetTransBarrier.image = app.GetOutputCubemapImg();
            cubemapRenderTargetTransBarrier.subresourceRange = cubemapSubResRange;
        }

        vkCmdPipelineBarrier(cmdBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &cubemapRenderTargetTransBarrier);

        VkClearValue clearColor = { {{1.0f, 0.0f, 0.0f, 1.0f}} };

        VkRenderingAttachmentInfoKHR renderAttachmentInfo{};
        {
            renderAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
            renderAttachmentInfo.imageView = app.GetOutputCubemapImgView();
            renderAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            renderAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            renderAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            renderAttachmentInfo.clearValue = clearColor;
        }

        VkExtent2D colorRenderTargetExtent{};
        {
            colorRenderTargetExtent.width = app.GetOutputCubemapExtent().width;
            colorRenderTargetExtent.height = app.GetOutputCubemapExtent().height;
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
                                app.GetPipelineLayout(),
                                0, 1, &pipelineDescriptorSet,
                                0, NULL);

        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app.GetPipeline());

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

        VkFence submitFence;
        VkFenceCreateInfo fenceInfo{};
        {
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        }
        VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &submitFence));
        VK_CHECK(vkResetFences(device, 1, &submitFence));

        VkSubmitInfo submitInfo{};
        {
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &cmdBuffer;
        }
        VK_CHECK(vkQueueSubmit(gfxQueue, 1, &submitInfo, submitFence));
        vkWaitForFences(device, 1, &submitFence, VK_TRUE, UINT64_MAX);
    }

    // Covert output 6 faces images to the Vulkan's cubemap's format
    // From Front, Back, Top, ... To X+, X-, Y+,...
    {
        
    }

    // Save the vulkan format cubemap to the disk
    {
        // Fill the command buffer
        VkCommandBufferBeginInfo beginInfo{};
        {
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        }
        VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo));

        // Copy the rendered images to a buffer.
        VkBuffer stagingBuffer;
        VmaAllocation stagingBufferAlloc;

        VmaAllocationCreateInfo stagingBufAllocInfo{};
        {
            stagingBufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            stagingBufAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        }

        VkBufferCreateInfo stgBufInfo{};
        {
            stgBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            stgBufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            stgBufInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            stgBufInfo.size = 4 * sizeof(float) * app.GetOutputCubemapExtent().width * app.GetOutputCubemapExtent().height * 6; // RGBA and 6 images blocks.
        }

        VK_CHECK(vmaCreateBuffer(allocator, &stgBufInfo, &stagingBufAllocInfo, &stagingBuffer, &stagingBufferAlloc, nullptr));

        // Transfer cubemap's layout to copy source
        VkImageMemoryBarrier cubemapColorAttToSrcBarrier{};
        {
            cubemapColorAttToSrcBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            cubemapColorAttToSrcBarrier.image = app.GetOutputCubemapImg();
            cubemapColorAttToSrcBarrier.subresourceRange = cubemapSubResRange;
            cubemapColorAttToSrcBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            cubemapColorAttToSrcBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            cubemapColorAttToSrcBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            cubemapColorAttToSrcBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        }

        vkCmdPipelineBarrier(
            cmdBuffer,
            VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &cubemapColorAttToSrcBarrier);

        // Copy the data from buffer to the image
        // The output cubemap will be vStrip for convenience.
        // NOTE: Read the doc to check how do images' texel coordinates map to buffer's 1D index, which is not intuitive but mathmaically elegent.
        VkBufferImageCopy cubemapToBufferCopy{};
        {
            cubemapToBufferCopy.bufferRowLength = app.GetOutputCubemapExtent().width;
            // cubemapToBufferCopy.bufferImageHeight = app.GetOutputCubemapExtent().height;
            cubemapToBufferCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            cubemapToBufferCopy.imageSubresource.mipLevel = 0;
            cubemapToBufferCopy.imageSubresource.baseArrayLayer = 0;
            cubemapToBufferCopy.imageSubresource.layerCount = 6;
            cubemapToBufferCopy.imageExtent = app.GetOutputCubemapExtent();
        }

        vkCmdCopyImageToBuffer(cmdBuffer,
            app.GetOutputCubemapImg(),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            stagingBuffer,
            1, &cubemapToBufferCopy);

        // Submit all the works recorded before
        VK_CHECK(vkEndCommandBuffer(cmdBuffer));

        VkFence submitFence;
        VkFenceCreateInfo fenceInfo{};
        {
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        }
        VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &submitFence));
        VK_CHECK(vkResetFences(device, 1, &submitFence));

        VkSubmitInfo submitInfo{};
        {
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &cmdBuffer;
        }
        VK_CHECK(vkQueueSubmit(gfxQueue, 1, &submitInfo, submitFence));
        vkWaitForFences(device, 1, &submitFence, VK_TRUE, UINT64_MAX);

        // Copy the buffer data to RAM and save that on the disk.
        float* pImgData = new float[4 * app.GetOutputCubemapExtent().width * app.GetOutputCubemapExtent().height * 6];

        void* pBufferMapped;
        vmaMapMemory(allocator, stagingBufferAlloc, &pBufferMapped);
        memcpy(pImgData, pBufferMapped, 4 * sizeof(float) * app.GetOutputCubemapExtent().width * app.GetOutputCubemapExtent().height * 6);
        vmaUnmapMemory(allocator, stagingBufferAlloc);

        bool foundProperData = false;
        for (int i = 0; i < 4 * app.GetOutputCubemapExtent().width * app.GetOutputCubemapExtent().height * 6; i++)
        {
            if (pImgData[i] > 0.f)
            {
                foundProperData = true;
                break;
            }
        }
        if (foundProperData)
        {
            std::cout << "Found proper output data." << std::endl;
        }
        else
        {
            std::cout << "Didn't find proper output data." << std::endl;
        }

        // Convert data from 4 elements to 3 elements data
        float* pImgData3Ele = new float[3 * app.GetOutputCubemapExtent().width * app.GetOutputCubemapExtent().height * 6];
        for (uint32_t i = 0; i < app.GetOutputCubemapExtent().width * app.GetOutputCubemapExtent().height * 6; i++)
        {
            uint32_t ele4Idx0 = i * 4;
            uint32_t ele4Idx1 = i * 4 + 1;
            uint32_t ele4Idx2 = i * 4 + 2;

            uint32_t ele3Idx0 = i * 3;
            uint32_t ele3Idx1 = i * 3 + 1;
            uint32_t ele3Idx2 = i * 3 + 2;

            pImgData3Ele[ele3Idx0] = pImgData[ele4Idx0];
            pImgData3Ele[ele3Idx1] = pImgData[ele4Idx1];
            pImgData3Ele[ele3Idx2] = pImgData[ele4Idx2];
        }
        app.SaveCubemap("C:\\JiaruiYan\\Projects\\OneFileVulkans\\Tools\\SphericalToCubemap\\data\\little_paris_eiffel_tower_4k_cubemap.hdr",
            app.GetOutputCubemapExtent().width,
            app.GetOutputCubemapExtent().height * 6,
            3,
            pImgData3Ele);

        // Cleanup resources
        vkResetCommandBuffer(cmdBuffer, 0);
        vmaDestroyBuffer(allocator, stagingBuffer, stagingBufferAlloc);
        vkDestroyFence(device, submitFence, nullptr);
        delete pImgData;
        delete pImgData3Ele;
    }

    if (rdoc_api)
    {
        rdoc_api->EndFrameCapture(NULL, NULL);
    }
}