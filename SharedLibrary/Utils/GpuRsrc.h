#pragma once

#include <vulkan/vulkan.h>

VK_DEFINE_HANDLE(VmaAllocation);

namespace SharedLib
{
    struct ImgInfo
    {
        uint32_t pixWidth;
        uint32_t pixHeight;
        uint32_t componentCnt;
        std::vector<uint8_t> dataVec;
        float* pData;
    };

    struct BinBufferInfo
    {
        uint32_t byteCnt;
        float* pData;
    };

    class GpuBuffer
    {
        VkBuffer      buffer;
        VmaAllocation bufferAlloc;
    };

    class GpuImage
    {
        VkImage               img;
        VmaAllocation         imgAlloc;
        VkImageView           imgView;
        VkSampler             imgSampler;
        VkDescriptorImageInfo imgDescriptorInfo;
    };
}