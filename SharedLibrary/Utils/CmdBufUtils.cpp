#include "CmdBufUtils.h"
#include "VulkanDbgUtils.h"
#include "Application.h"

namespace SharedLib
{
    // ================================================================================================================
    void TransitionImgLayout(
        VkCommandBuffer cmdBuffer,
        VkDevice device,
        VkQueue gfxQueue,
        VkImage img,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        VkImageSubresourceRange subResRange)
    {
        VkFence stagingFence;

        VkFenceCreateInfo fenceInfo{};
        {
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        }
        VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &stagingFence));

        VkCommandBufferBeginInfo beginInfo{};
        {
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        }
        VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo));

        // Transform the layout of the image
        VkImageMemoryBarrier formatTransBarrier{};
        {
            formatTransBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            formatTransBarrier.image = img;
            formatTransBarrier.subresourceRange = subResRange;
            formatTransBarrier.srcAccessMask = 0;
            formatTransBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            // formatTransBarrier.dstAccessMask = VK_ACCESS_NONE;
            formatTransBarrier.oldLayout = oldLayout;
            formatTransBarrier.newLayout = newLayout;
        }

        vkCmdPipelineBarrier(
            cmdBuffer,
            VK_PIPELINE_STAGE_HOST_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            // VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &formatTransBarrier);

        // End the command buffer and submit the packets
        vkEndCommandBuffer(cmdBuffer);

        // Submit the filled command buffer to the graphics queue to draw the image
        VkSubmitInfo submitInfo{};
        {
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &cmdBuffer;
        }
        vkResetFences(device, 1, &stagingFence);
        VK_CHECK(vkQueueSubmit(gfxQueue, 1, &submitInfo, stagingFence));

        // Wait for the end of all transformation and reset the command buffer. The fence would be waited in the first loop.
        vkWaitForFences(device, 1, &stagingFence, VK_TRUE, UINT64_MAX);
        vkResetCommandBuffer(cmdBuffer, 0);

        // Destroy temp resources
        vkDestroyFence(device, stagingFence, nullptr);
    }

    // ================================================================================================================
    void Send2dImgDataToGpu(
        VkCommandBuffer cmdBuffer,
        VkDevice        device,
        VkQueue         gfxQueue,
        VmaAllocator    allocator,
        ImgInfo*        pImgInfo,
        VkImage         image)
    {
        // The universal 2D texture SubresourceRange
        VkImageSubresourceRange tex2dSubResRange{};
        {
            tex2dSubResRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            tex2dSubResRange.baseMipLevel = 0;
            tex2dSubResRange.levelCount = 1;
            tex2dSubResRange.baseArrayLayer = 0;
            tex2dSubResRange.layerCount = 1;
        }

        VkBufferImageCopy tex2dBufToImgCopy{};
        {
            VkExtent3D extent{};
            {
                extent.width = pImgInfo->pixWidth;
                extent.height = pImgInfo->pixWidth;
                extent.depth = 1;
            }

            tex2dBufToImgCopy.bufferRowLength = extent.width;
            tex2dBufToImgCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            tex2dBufToImgCopy.imageSubresource.mipLevel = 0;
            tex2dBufToImgCopy.imageSubresource.baseArrayLayer = 0;
            tex2dBufToImgCopy.imageSubresource.layerCount = 1;
            tex2dBufToImgCopy.imageExtent = extent;
        }

        SharedLib::SendImgDataToGpu(cmdBuffer,
                                    device,
                                    gfxQueue,
                                    pImgInfo->dataVec.data(),
                                    pImgInfo->dataVec.size(),
                                    image,
                                    tex2dSubResRange,
                                    VK_IMAGE_LAYOUT_UNDEFINED,
                                    tex2dBufToImgCopy,
                                    allocator);
    }

    // ================================================================================================================
    // The staging buffer has to be freed after the copy finishes, so this func has to control a fence.
    // Transfer the dstImg to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL -- Change it to the shader read optimal.
    // TODO: I need to cleanup code that transfer it from dst optimal to shader read optimal barriers.
    void SendImgDataToGpu(
        VkCommandBuffer         cmdBuffer,
        VkDevice                device,
        VkQueue                 gfxQueue,
        void*                   pData,
        uint32_t                bytesCnt,
        VkImage                 dstImg,
        VkImageSubresourceRange subResRange,
        VkImageLayout           dstImgCurrentLayout,
        VkBufferImageCopy       bufToImgCopyInfo,
        VmaAllocator            allocator)
    {
        // Create the staging buffer resources
        VkBuffer stagingBuffer;
        VmaAllocation stagingBufAlloc;
        VkFence stagingFence;

        VkFenceCreateInfo fenceInfo{};
        {
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        }
        VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &stagingFence));

        VmaAllocationCreateInfo stagingBufAllocInfo{};
        {
            stagingBufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            stagingBufAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        }

        VkBufferCreateInfo stgBufInfo{};
        {
            stgBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            stgBufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            stgBufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            stgBufInfo.size = bytesCnt;
        }

        VK_CHECK(vmaCreateBuffer(allocator, &stgBufInfo, &stagingBufAllocInfo, &stagingBuffer, &stagingBufAlloc, nullptr));

        // Send data to staging Buffer
        void* pStgData;
        vmaMapMemory(allocator, stagingBufAlloc, &pStgData);
        memcpy(pStgData, pData, bytesCnt);
        vmaUnmapMemory(allocator, stagingBufAlloc);

        /* Send staging buffer data to the GPU image. */
        VkCommandBufferBeginInfo beginInfo{};
        {
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        }
        VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo));

        // Transform the layout of the image to copy source
        VkImageMemoryBarrier undefToDstBarrier{};
        {
            undefToDstBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            undefToDstBarrier.image = dstImg;
            undefToDstBarrier.subresourceRange = subResRange;
            undefToDstBarrier.srcAccessMask = 0;
            undefToDstBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            undefToDstBarrier.oldLayout = dstImgCurrentLayout;
            undefToDstBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        }

        vkCmdPipelineBarrier(
            cmdBuffer,
            VK_PIPELINE_STAGE_HOST_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &undefToDstBarrier);

        // Copy the data from buffer to the image
        vkCmdCopyBufferToImage(
            cmdBuffer,
            stagingBuffer,
            dstImg,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &bufToImgCopyInfo);

        // Transform the layout of the image to shader read optimal
        VkImageMemoryBarrier cpDstToRdOpt = undefToDstBarrier;
        {
            cpDstToRdOpt.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            // cpDstToRdOpt.newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL; -- This needs the sync 2 extensions.
            cpDstToRdOpt.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        vkCmdPipelineBarrier(
            cmdBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &cpDstToRdOpt);

        // End the command buffer and submit the packets
        vkEndCommandBuffer(cmdBuffer);

        // Submit the filled command buffer to the graphics queue to draw the image
        VkSubmitInfo submitInfo{};
        {
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &cmdBuffer;
        }
        vkResetFences(device, 1, &stagingFence);
        VK_CHECK(vkQueueSubmit(gfxQueue, 1, &submitInfo, stagingFence));

        // Wait for the end of all transformation and reset the command buffer. The fence would be waited in the first loop.
        vkWaitForFences(device, 1, &stagingFence, VK_TRUE, UINT64_MAX);
        vkResetCommandBuffer(cmdBuffer, 0);

        // Destroy temp resources
        vmaDestroyBuffer(allocator, stagingBuffer, stagingBufAlloc);
        vkDestroyFence(device, stagingFence, nullptr);
    }

    // ================================================================================================================
    void SubmitCmdBufferAndWait(
        VkDevice device,
        VkQueue queue,
        VkCommandBuffer cmdBuffer)
    {
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
        VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, submitFence));
        VK_CHECK(vkWaitForFences(device, 1, &submitFence, VK_TRUE, UINT64_MAX));

        // VK_CHECK(vkDeviceWaitIdle(device));

        vkDestroyFence(device, submitFence, nullptr);
    }

    // ================================================================================================================
    // TODO: The submit cannot happen in it.
    void CmdCopyCubemapToBuffer(VkCommandBuffer cmdBuffer,
                                VkDevice        device,
                                VkQueue         gfxQueue,
                                VkImage         srcImg,
                                uint32_t        widthHeight, // Assume the width and height of a face is always same.
                                VkBuffer        dstBuffer)
    {
        // Fill the command buffer
        VkCommandBufferBeginInfo beginInfo{};
        {
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        }
        VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo));

        VkExtent3D cubemapExtent{};
        {
            cubemapExtent.width  = widthHeight;
            cubemapExtent.height = widthHeight;
            cubemapExtent.depth  = 1;
        }

        // Copy the data from buffer to the image
        // The output cubemap will be vStrip for convenience.
        // NOTE: Read the doc to check how do images' texel coordinates map to buffer's 1D index, which is not intuitive but mathmaically elegent.
        VkBufferImageCopy cubemapToBufferCopy{};
        {
            cubemapToBufferCopy.bufferRowLength = widthHeight;
            // cubemapToBufferCopy.bufferImageHeight = app.GetOutputCubemapExtent().height;
            cubemapToBufferCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            cubemapToBufferCopy.imageSubresource.mipLevel = 0;
            cubemapToBufferCopy.imageSubresource.baseArrayLayer = 0;
            cubemapToBufferCopy.imageSubresource.layerCount = 6;
            cubemapToBufferCopy.imageExtent = cubemapExtent;
        }

        vkCmdCopyImageToBuffer(cmdBuffer,
                               srcImg,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               dstBuffer,
                               1, &cubemapToBufferCopy);

        // Submit all the works recorded before
        VK_CHECK(vkEndCommandBuffer(cmdBuffer));

        SharedLib::SubmitCmdBufferAndWait(device, gfxQueue, cmdBuffer);
    }
}