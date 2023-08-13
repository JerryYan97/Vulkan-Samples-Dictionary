#include "CmdBufUtils.h"
#include "VulkanDbgUtils.h"

namespace SharedLib
{
    // ================================================================================================================
    void CmdSendImgDataToGpu(
        VkCommandBuffer         cmdBuffer,
        VkDevice                device,
        VkQueue                 gfxQueue,
        void*                   pData,
        uint32_t                bytesCnt,
        VkImage                 dstImg,
        VkImageSubresourceRange subResRange,
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
            undefToDstBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
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

        // Transform the layout of the image to shader access resource
        VkImageMemoryBarrier hdrDstToShaderBarrier{};
        {
            hdrDstToShaderBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            hdrDstToShaderBarrier.image = dstImg;
            hdrDstToShaderBarrier.subresourceRange = subResRange;
            hdrDstToShaderBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            hdrDstToShaderBarrier.dstAccessMask = VK_ACCESS_NONE;
            hdrDstToShaderBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            hdrDstToShaderBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        vkCmdPipelineBarrier(
            cmdBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &hdrDstToShaderBarrier);

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
}