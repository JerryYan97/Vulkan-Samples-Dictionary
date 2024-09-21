#pragma once
#include <vulkan/vulkan.h>
#include "../VMA/vk_mem_alloc.h"

namespace SharedLib
{
    struct GpuImg;

    // A helper function to transition the image layout. Block the thread until the transition is done.
    void TransitionImgLayout(VkCommandBuffer cmdBuffer,
                             VkDevice device,
                             VkQueue gfxQueue,
                             VkImage img,
                             VkImageLayout oldLayout,
                             VkImageLayout newLayout,
                             VkImageSubresourceRange subResRange);

    // A helper function to send 2D image data to the GPU. Block the thread until the data is sent.
    void Send2dImgDataToGpu(
        VkCommandBuffer cmdBuffer,
        VkDevice        device,
        VkQueue         gfxQueue,
        VmaAllocator    allocator,
        ImgInfo*        pImgInfo,
        VkImage         image);

    // A helper function to send the image data to the GPU. Block the thread until the data is sent.
    void SendImgDataToGpu(VkCommandBuffer cmdBuffer,
                          VkDevice device,
                          VkQueue gfxQueue,
                          void* pData,
                          uint32_t bytesCnt,
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