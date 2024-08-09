#include "Level.h"
#include "AppUtils.h"
#include "VulkanDbgUtils.h"
#include "vk_mem_alloc.h"

namespace SharedLib
{
    // ================================================================================================================
    bool Level::AddMshEntity(const std::string& name,
                             MeshEntity*         entity)
    {
        if(m_meshEntities.count(name) > 0)
        {
            return false;
        }
        else
        {
            m_meshEntities[name] = entity;
            return true;
        }
    }

    // ================================================================================================================
    bool Level::AddLightEntity(const std::string& name,
                               LightEntity*       entity)
    {
        if (m_lightEntities.count(name) > 0)
        {
            return false;
        }
        else
        {
            m_lightEntities[name] = entity;
            return true;
        }   
    }

    // ================================================================================================================
    void MeshEntity::InitGpuRsrc(VkDevice      device,
                                 VmaAllocator* pAllocator)
    {
        for (auto& meshPrimitive : m_meshPrimitives)
        {
            meshPrimitive.InitGpuRsrc(device, pAllocator);
        }
    }

    // ================================================================================================================
    void MeshEntity::FinializeGpuRsrc(VkDevice      device,
                                      VmaAllocator* pAllocator)
    {
        for (auto& meshPrimitive : m_meshPrimitives)
        {
            meshPrimitive.FinializeGpuRsrc(device, pAllocator);
        }
    }

    // ================================================================================================================
    void MeshPrimitive::InitGpuRsrc(VkDevice      device,
                                    VmaAllocator* pAllocator)
    {
        uint32_t vertCount = m_posData.size() / 3;

        // Create vertex buffer and send to GPU memory
        // pos<3>, normal<3>, tangent<3>, uv<2>.
        m_vertData.resize(vertCount * 11);
        for (uint32_t i = 0; i < vertCount; i++)
        {
            m_vertData[i * 11 + 0] = m_posData[i * 3 + 0];
            m_vertData[i * 11 + 1] = m_posData[i * 3 + 1];
            m_vertData[i * 11 + 2] = m_posData[i * 3 + 2];

            m_vertData[i * 11 + 3] = m_normalData[i * 3 + 0];
            m_vertData[i * 11 + 4] = m_normalData[i * 3 + 1];
            m_vertData[i * 11 + 5] = m_normalData[i * 3 + 2];

            m_vertData[i * 11 + 6] = m_tangentData[i * 3 + 0];
            m_vertData[i * 11 + 7] = m_tangentData[i * 3 + 1];
            m_vertData[i * 11 + 8] = m_tangentData[i * 3 + 2];

            m_vertData[i * 11 + 9] = m_texCoordData[i * 2 + 0];
            m_vertData[i * 11 + 10] = m_texCoordData[i * 2 + 1];
        }

        // Init the GpuBuffer for the vert buffer.
        {
            uint32_t vertBufferByteCnt = m_vertData.size() * sizeof(float);

            VkBufferCreateInfo vertBufferInfo{};
            {
                vertBufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                vertBufferInfo.size        = vertBufferByteCnt;
                vertBufferInfo.usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
                vertBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            }

            VmaAllocationCreateInfo vertBufferAllocInfo{};
            {
                vertBufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
                vertBufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
                                            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            }

            vmaCreateBuffer(*pAllocator,
                            &vertBufferInfo,
                            &vertBufferAllocInfo,
                            &m_vertBuffer.buffer,
                            &m_vertBuffer.bufferAlloc,
                            nullptr);

            SharedLib::CopyRamDataToGpuBuffer(m_vertData.data(),
                                              pAllocator,
                                              m_vertBuffer.buffer,
                                              m_vertBuffer.bufferAlloc,
                                              sizeof(float) * m_vertData.size());
        }

        // Create index buffer and send to GPU memory
        {
            VkBufferCreateInfo indexBufferInfo{};
            {
                indexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                indexBufferInfo.size = m_idxDataUint16.size() * sizeof(uint16_t);
                indexBufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
                indexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            }

            VmaAllocationCreateInfo indexBufferAllocInfo{};
            {
                indexBufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
                indexBufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
                                             VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            }

            vmaCreateBuffer(*pAllocator,
                            &indexBufferInfo,
                            &indexBufferAllocInfo,
                            &m_indexBuffer.buffer,
                            &m_indexBuffer.bufferAlloc,
                            nullptr);
        }

        VmaAllocationCreateInfo gpuImgAllocInfo{};
        {
            gpuImgAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            gpuImgAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        }

        VkSamplerCreateInfo samplerInfo{};
        {
            samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter = VK_FILTER_LINEAR;
            samplerInfo.minFilter = VK_FILTER_LINEAR;
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.minLod = -1000;
            samplerInfo.maxLod = 1000;
            samplerInfo.maxAnisotropy = 1.0f;
        }

        // Create base color's GPU image.
        {
            VkExtent3D extent{};
            {
                extent.width = m_baseColorTex.pixWidth;
                extent.height = m_baseColorTex.pixHeight;
                extent.depth = 1;
            }

            VkImageCreateInfo baseColorImgInfo{};
            {
                baseColorImgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                baseColorImgInfo.imageType = VK_IMAGE_TYPE_2D;
                baseColorImgInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
                baseColorImgInfo.extent = extent;
                baseColorImgInfo.mipLevels = 1;
                baseColorImgInfo.arrayLayers = 1;
                baseColorImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
                baseColorImgInfo.tiling = VK_IMAGE_TILING_LINEAR;
                baseColorImgInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
                baseColorImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            }

            vmaCreateImage(*pAllocator,
                           &baseColorImgInfo,
                           &gpuImgAllocInfo,
                           &m_baseColorGpuImg.image,
                           &m_baseColorGpuImg.imageAllocation,
                           nullptr);

            VkImageViewCreateInfo info{};
            {
                info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                info.image = m_baseColorGpuImg.image;
                info.viewType = VK_IMAGE_VIEW_TYPE_2D;
                info.format = VK_FORMAT_R8G8B8A8_SRGB;
                info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                info.subresourceRange.levelCount = 1;
                info.subresourceRange.layerCount = 1;
            }
            VK_CHECK(vkCreateImageView(device, &info, nullptr, &m_baseColorGpuImg.imageView));
            
            VK_CHECK(vkCreateSampler(device, &samplerInfo, nullptr, &m_baseColorGpuImg.imageSampler));

            m_baseColorGpuImg.imageDescInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            m_baseColorGpuImg.imageDescInfo.imageView = m_baseColorGpuImg.imageView;
            m_baseColorGpuImg.imageDescInfo.sampler = m_baseColorGpuImg.imageSampler;
        }
    }

    // ================================================================================================================
    void MeshPrimitive::FinializeGpuRsrc(VkDevice      device,
                                         VmaAllocator* pAllocator)
    {
        vmaDestroyBuffer(*pAllocator, m_indexBuffer.buffer, m_indexBuffer.bufferAlloc);
        vmaDestroyBuffer(*pAllocator, m_vertBuffer.buffer, m_vertBuffer.bufferAlloc);

        vmaDestroyImage(*pAllocator, m_baseColorGpuImg.image, m_baseColorGpuImg.imageAllocation);
        vkDestroyImageView(device, m_baseColorGpuImg.imageView, nullptr);
        vkDestroySampler(device, m_baseColorGpuImg.imageSampler, nullptr);

        vmaDestroyImage(*pAllocator, m_normalGpuImg.image, m_normalGpuImg.imageAllocation);
        vkDestroyImageView(device, m_normalGpuImg.imageView, nullptr);
        vkDestroySampler(device, m_normalGpuImg.imageSampler, nullptr);

        vmaDestroyImage(*pAllocator, m_metallicRoughnessGpuImg.image, m_metallicRoughnessGpuImg.imageAllocation);
        vkDestroyImageView(device, m_metallicRoughnessGpuImg.imageView, nullptr);
        vkDestroySampler(device, m_metallicRoughnessGpuImg.imageSampler, nullptr);

        vmaDestroyImage(*pAllocator, m_emissiveGpuImg.image, m_emissiveGpuImg.imageAllocation);
        vkDestroyImageView(device, m_emissiveGpuImg.imageView, nullptr);
        vkDestroySampler(device, m_emissiveGpuImg.imageSampler, nullptr);

        vmaDestroyImage(*pAllocator, m_occlusionGpuImg.image, m_occlusionGpuImg.imageAllocation);
        vkDestroyImageView(device, m_occlusionGpuImg.imageView, nullptr);
        vkDestroySampler(device, m_occlusionGpuImg.imageSampler, nullptr);
    }
}