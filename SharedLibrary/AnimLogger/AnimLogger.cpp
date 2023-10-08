#include "AnimLogger.h"
#include "VulkanDbgUtils.h"
#include "DiskOpsUtils.h"
#include "AppUtils.h"

namespace SharedLib
{
    // ================================================================================================================
    AnimLogger::AnimLogger() :
        m_logFps(30),
        m_logDurationRemain(0.f),
        m_logDurationStart(0.f),
        m_isFirstTimeRecord(true),
        m_lastTime(),
        m_dumpedImgCnt(0),
        m_dumpImgInfo(),
        m_pAllocator(nullptr),
        m_isTransDstLayout(false)
    {}

    // ================================================================================================================
    AnimLogger::~AnimLogger()
    {
        if (m_dumpImgInfo.stgImg != VK_NULL_HANDLE)
        {
            vmaDestroyImage(*m_pAllocator, m_dumpImgInfo.stgImg, m_dumpImgInfo.stgImgAlloc);
        }
    }

    // ================================================================================================================
    // NOTE: The output image layout should be copy dst to be dumped so we need to add a barrier at the end.
    void AnimLogger::CmdCopyRenderTargetOut(
        VkCommandBuffer cmdBuffer,
        VkImage         srcImg,
        VkExtent2D      srcImgExtent)
    {
        VkImageSubresourceRange colorRenderTargetSubresRange{};
        {
            colorRenderTargetSubresRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            colorRenderTargetSubresRange.baseArrayLayer = 0;
            colorRenderTargetSubresRange.layerCount = 1;
            colorRenderTargetSubresRange.baseMipLevel = 0;
            colorRenderTargetSubresRange.levelCount = 1;
        }

        // Init the vkImage if no dump before
        VkImageLayout stgImgOldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        if (m_dumpImgInfo.stgImg == VK_NULL_HANDLE)
        {
            stgImgOldLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            VmaAllocationCreateInfo allocInfo{};
            {
                allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
                allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
            }

            VkExtent3D extent{};
            {
                extent.width = srcImgExtent.width;
                extent.height = srcImgExtent.height;
                extent.depth = 1;
            }
            m_dumpImgInfo.width = srcImgExtent.width;
            m_dumpImgInfo.height = srcImgExtent.height;

            VkImageCreateInfo imgInfo{};
            {
                imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                imgInfo.imageType = VK_IMAGE_TYPE_2D;
                imgInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
                imgInfo.extent = extent;
                imgInfo.mipLevels = 1;
                imgInfo.arrayLayers = 1;
                imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
                imgInfo.tiling = VK_IMAGE_TILING_LINEAR;
                imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
                imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            }

            vmaCreateImage(*m_pAllocator,
                           &imgInfo,
                           &allocInfo,
                           &m_dumpImgInfo.stgImg,
                           &m_dumpImgInfo.stgImgAlloc,
                           nullptr);
        }

        VkImageMemoryBarrier toTransDstBarrier{};
        {
            toTransDstBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            toTransDstBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            toTransDstBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            toTransDstBarrier.oldLayout = stgImgOldLayout;
            toTransDstBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toTransDstBarrier.image = m_dumpImgInfo.stgImg;
            toTransDstBarrier.subresourceRange = colorRenderTargetSubresRange;
        }

        vkCmdPipelineBarrier(cmdBuffer,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &toTransDstBarrier);

        // Destroy the current image if the input extent is new.
        // Maybe we should just assert that the screen size cannot be change during the recording...
        // if()

        // Trans the srcImg to trans src layout and assume that it's in color attachment layout
        VkImageMemoryBarrier toTransSrcBarrier{};
        {
            toTransSrcBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            toTransSrcBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            toTransSrcBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            toTransSrcBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            toTransSrcBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            toTransSrcBarrier.image = srcImg;
            toTransSrcBarrier.subresourceRange = colorRenderTargetSubresRange;
        }

        vkCmdPipelineBarrier(cmdBuffer,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &toTransSrcBarrier);

        // Copy the image
        VkImageSubresourceLayers colorRenderTargetSubres{};
        {
            colorRenderTargetSubres.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            colorRenderTargetSubres.baseArrayLayer = 0;
            colorRenderTargetSubres.layerCount = 1;
            colorRenderTargetSubres.mipLevel = 0;
        }

        VkImageCopy imgCopyInfo{};
        {
            imgCopyInfo.srcSubresource = colorRenderTargetSubres;
            imgCopyInfo.dstSubresource = colorRenderTargetSubres;
            {
                imgCopyInfo.extent.depth = 1;
                imgCopyInfo.extent.width = srcImgExtent.width;
                imgCopyInfo.extent.height = srcImgExtent.height;
            }
            imgCopyInfo.srcOffset = {0, 0, 0};
            imgCopyInfo.dstOffset = {0, 0, 0};
        }
        
        vkCmdCopyImage(cmdBuffer,
                       srcImg, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 
                       m_dumpImgInfo.stgImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &imgCopyInfo);

        // Trans the src image back to color render target attachment layout so we don't mess up the original works.
        VkImageMemoryBarrier toColorAttachmentBarrier{};
        {
            toColorAttachmentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            toColorAttachmentBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            toColorAttachmentBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            toColorAttachmentBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            toColorAttachmentBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            toColorAttachmentBarrier.image = srcImg;
            toColorAttachmentBarrier.subresourceRange = colorRenderTargetSubresRange;
        }

        // Trans the stg img to trans src so that it can be copied out to RAM.
        VkImageMemoryBarrier stgImgToTransSrcBarrier{};
        {
            stgImgToTransSrcBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            stgImgToTransSrcBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            stgImgToTransSrcBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            stgImgToTransSrcBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            stgImgToTransSrcBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            stgImgToTransSrcBarrier.image = m_dumpImgInfo.stgImg;
            stgImgToTransSrcBarrier.subresourceRange = colorRenderTargetSubresRange;
        }

        VkImageMemoryBarrier barriers[2] = {
            toColorAttachmentBarrier, stgImgToTransSrcBarrier
        };

        vkCmdPipelineBarrier(cmdBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, nullptr,
            0, nullptr,
            2, barriers);
    }

    // ================================================================================================================
    void AnimLogger::Init(
        AnimLoggerInitInfo initInfo)
    {
        m_logFps = initInfo.logFps;
        m_logDurationRemain = initInfo.logDuration;
        m_logDurationStart = initInfo.logDuration;
        m_pAllocator = initInfo.pAllocator;
        m_dumpDir = initInfo.dumpDir;
    }

    // ================================================================================================================
    void AnimLogger::DumpRenderTargetData(
        VkDevice device,
        VkQueue  submitQueue,
        VkFence  queueExeFence,
        VkCommandBuffer cmdBuffer)
    {
        if (m_logDurationRemain > 0.f)
        {
            // We only need to dump the buffer when the idle duration exceeds fps duration.
            float fpsDuration = 1.f / float(m_logFps);

            // Wait for the queue execution fence.
            // Timeout is in the unit of nanosec.
            CheckVkResult(vkWaitForFences(device, 1, &queueExeFence, VK_TRUE, 1000000));

            if (m_isFirstTimeRecord)
            {
                m_isFirstTimeRecord = false;
                m_lastTime = std::chrono::high_resolution_clock::now();
            }

            auto thisTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(thisTime - m_lastTime);

            float delta = (float)duration.count() / 1000.f; // Delta is in second.
            if (delta > fpsDuration)
            {
                // Dump the image.
                std::string dumpImgName = "/" + std::to_string(m_dumpedImgCnt + 1) + "_fps" + std::to_string(m_logFps) + ".png";

                VkImageSubresourceLayers colorRenderTargetSubres{};
                {
                    colorRenderTargetSubres.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    colorRenderTargetSubres.baseArrayLayer = 0;
                    colorRenderTargetSubres.layerCount = 1;
                    colorRenderTargetSubres.mipLevel = 0;
                }

                VkExtent3D extent{};
                {
                    extent.width = m_dumpImgInfo.width;
                    extent.height = m_dumpImgInfo.height;
                    extent.depth = 1;
                }

                std::vector<char> imgData;
                imgData.resize(4 * extent.width * extent.height);

                CopyImgToRam(cmdBuffer, device, submitQueue, *m_pAllocator,
                             m_dumpImgInfo.stgImg, colorRenderTargetSubres, extent, 4, 1, imgData.data());

                // "stride_in_bytes" is the distance in bytes from the first byte of a row of pixels to the first byte of the next row of pixels.
                SaveImgPng(m_dumpDir + dumpImgName, m_dumpImgInfo.width, m_dumpImgInfo.height, 4, imgData.data(), 0);

                m_logDurationRemain -= delta;
                m_dumpedImgCnt++;
                m_lastTime = thisTime;
            }
        }        
    }
}