#include "GenIBL.h"
#include "../../SharedLibrary/Utils/VulkanDbgUtils.h"
#include "../../SharedLibrary/Camera/Camera.h"
#include "../../SharedLibrary/Event/Event.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "vk_mem_alloc.h"

// ================================================================================================================
GenIBL::GenIBL() :
    Application(),
    m_hdrCubeMapImage(VK_NULL_HANDLE),
    m_hdrCubeMapView(VK_NULL_HANDLE),
    m_hdrCubeMapSampler(VK_NULL_HANDLE),
    m_hdrCubeMapAlloc(VK_NULL_HANDLE)
{
}

// ================================================================================================================
GenIBL::~GenIBL()
{
    vkDeviceWaitIdle(m_device);

    DestroyHdrRenderObjs();
    DestroyPrefilterEnvMapPipelineResourses();
}

// ================================================================================================================
void GenIBL::DestroyHdrRenderObjs()
{
    vmaDestroyImage(*m_pAllocator, m_hdrCubeMapImage, m_hdrCubeMapAlloc);
    vkDestroyImageView(m_device, m_hdrCubeMapView, nullptr);
    vkDestroySampler(m_device, m_hdrCubeMapSampler, nullptr);
}

// ================================================================================================================
VkDeviceSize GenIBL::GetHdrByteNum()
{
    return 3 * sizeof(float) * m_hdrImgWidth * m_hdrImgHeight;
}

// ================================================================================================================
void GenIBL::InitHdrRenderObjects()
{
    // Load the HDRI image into RAM
    std::string hdriFilePath = SOURCE_PATH;
    // hdriFilePath += "/../data/output_skybox.hdr";
    hdriFilePath += "/../data/little_paris_eiffel_tower_4k_cubemap.hdr";

    int width, height, nrComponents;
    m_hdrImgData = stbi_loadf(hdriFilePath.c_str(), &width, &height, &nrComponents, 0);

    m_hdrImgWidth = (uint32_t)width;
    m_hdrImgHeight = (uint32_t)height;

    VmaAllocationCreateInfo hdrAllocInfo{};
    {
        hdrAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        hdrAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    VkExtent3D extent{};
    {
        // extent.width = m_hdrImgWidth / 6;
        // extent.height = m_hdrImgHeight;
        extent.width = m_hdrImgWidth;
        extent.height = m_hdrImgWidth;
        extent.depth = 1;
    }

    VkImageCreateInfo cubeMapImgInfo{};
    {
        cubeMapImgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        cubeMapImgInfo.imageType = VK_IMAGE_TYPE_2D;
        cubeMapImgInfo.format = VK_FORMAT_R32G32B32_SFLOAT;
        cubeMapImgInfo.extent = extent;
        cubeMapImgInfo.mipLevels = 1;
        cubeMapImgInfo.arrayLayers = 6;
        cubeMapImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        cubeMapImgInfo.tiling = VK_IMAGE_TILING_LINEAR;
        cubeMapImgInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        cubeMapImgInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        cubeMapImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    VK_CHECK(vmaCreateImage(*m_pAllocator,
                            &cubeMapImgInfo,
                            &hdrAllocInfo,
                            &m_hdrCubeMapImage,
                            &m_hdrCubeMapAlloc,
                            nullptr));

    VkImageViewCreateInfo info{};
    {
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image = m_hdrCubeMapImage;
        info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        info.format = VK_FORMAT_R32G32B32_SFLOAT;
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.layerCount = 6;
    }
    VK_CHECK(vkCreateImageView(m_device, &info, nullptr, &m_hdrCubeMapView));

    VkSamplerCreateInfo sampler_info{};
    {
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT; // outside image bounds just use border color
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.minLod = -1000;
        sampler_info.maxLod = 1000;
        sampler_info.maxAnisotropy = 1.0f;
    }
    VK_CHECK(vkCreateSampler(m_device, &sampler_info, nullptr, &m_hdrCubeMapSampler));
}

// ================================================================================================================
void GenIBL::DestroyPrefilterEnvMapPipelineResourses()
{

}

// ================================================================================================================
void GenIBL::InitPrefilterEnvMapPipelineDescriptorSets()
{
    
}

// ================================================================================================================
void GenIBL::InitPrefilterEnvMapPipelineLayout()
{

}

// ================================================================================================================
void GenIBL::InitPrefilterEnvMapShaderModules()
{
}

// ================================================================================================================
void GenIBL::InitPrefilterEnvMapPipelineDescriptorSetLayout()
{
}

// ================================================================================================================
void GenIBL::InitPrefilterEnvMapPipeline()
{
}

// ================================================================================================================
void GenIBL::AppInit()
{
}