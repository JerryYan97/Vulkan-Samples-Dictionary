#pragma once
#include <vulkan/vulkan.h>
#include "../VMA/vk_mem_alloc.h"

namespace SharedLib
{
    // Function names should start with 'Cmd' so their names should be 'CmdXxxx'.

    void CmdSendImgDataToGpu(VkCommandBuffer cmdBuffer,
                             VkDevice device,
                             VkQueue gfxQueue,
                             void* pData, uint32_t bytesCnt,
                             VkImage dstImg,
                             VkImageSubresourceRange subResRange,
                             VkBufferImageCopy bufToImgCopyInfo,
                             VmaAllocator allocator);

    void CmdCopyCubemapToBuffer();

    void SubmitCmdBufferAndWait(
        VkDevice device,
        VkQueue queue,
        VkCommandBuffer cmdBuffer);
}