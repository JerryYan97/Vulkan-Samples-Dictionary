#pragma once
#include <vulkan/vulkan.h>
#include "../VMA/vk_mem_alloc.h"

namespace SharedLib
{
    // Function names should start with 'Cmd' so their names should be 'CmdXxxx'.
    // Maybe we should only change the layouts at the beginning of CmdXxxx functions.
    void SendImgDataToGpu(VkCommandBuffer cmdBuffer,
                             VkDevice device,
                             VkQueue gfxQueue,
                             void* pData, uint32_t bytesCnt,
                             VkImage dstImg,
                             VkImageSubresourceRange subResRange,
                             VkImageLayout           dstImgCurrentLayout,
                             VkBufferImageCopy bufToImgCopyInfo,
                             VmaAllocator allocator);

    // The output color is always a 3 channels -- RGB.
    // The input image is always 4 channels -- RGBA.
    // Always 32 bits for each channels.
    void CmdCopyCubemapToBuffer(VkCommandBuffer cmdBuffer,
                                VkDevice        device,
                                VkQueue         gfxQueue,
                                VkImage         srcImg,
                                uint32_t        widthHeight, // Assume the width and height of a face is always same.
                                VkBuffer        dstBuffer);

    void SubmitCmdBufferAndWait(
        VkDevice device,
        VkQueue queue,
        VkCommandBuffer cmdBuffer);
}