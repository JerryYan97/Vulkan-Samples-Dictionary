#include "GenIBL.h"
#include "vk_mem_alloc.h"
#include "../../SharedLibrary/Utils/VulkanDbgUtils.h"


// ================================================================================================================
void GenIBL::DestroyPrefilterEnvMapPipelineResourses()
{
    vkDestroyShaderModule(m_device, m_preFilterEnvMapVsShaderModule, nullptr);
    vkDestroyShaderModule(m_device, m_preFilterEnvMapPsShaderModule, nullptr);

    // Destroy the pipeline layout
    vkDestroyPipelineLayout(m_device, m_preFilterEnvMapPipelineLayout, nullptr);

    vmaDestroyImage(*m_pAllocator, m_preFilterEnvMapCubemap, m_preFilterEnvMapCubemapAlloc);
    vkDestroyImageView(m_device, m_preFilterEnvMapCubemapImageView, nullptr);
}

// ================================================================================================================
void GenIBL::InitPrefilterEnvMapPipelineLayout()
{
    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    {
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_diffIrrPreFilterEnvMapDesSet0Layout;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
    }

    VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_preFilterEnvMapPipelineLayout));
}

// ================================================================================================================
void GenIBL::InitPrefilterEnvMapShaderModules()
{
    m_preFilterEnvMapVsShaderModule = CreateShaderModule("/hlsl/prefilterEnvMap_vert.spv");
    m_preFilterEnvMapPsShaderModule = CreateShaderModule("/hlsl/prefilterEnvMap_frag.spv");
}

// ================================================================================================================
void GenIBL::InitPrefilterEnvMapPipeline()
{
    VkPipelineRenderingCreateInfoKHR prefilterEnvMapCreateInfo{};
    {
        prefilterEnvMapCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        prefilterEnvMapCreateInfo.viewMask = 0x3F;
        prefilterEnvMapCreateInfo.colorAttachmentCount = 1;
        prefilterEnvMapCreateInfo.pColorAttachmentFormats = &HdriRenderTargetFormat;
    }

    m_preFilterEnvMapPipeline.SetPNext(&prefilterEnvMapCreateInfo);

    VkPipelineShaderStageCreateInfo shaderStgsInfo[2] = {};
    shaderStgsInfo[0] = CreateDefaultShaderStgCreateInfo(m_preFilterEnvMapVsShaderModule,
        VK_SHADER_STAGE_VERTEX_BIT);
    shaderStgsInfo[1] = CreateDefaultShaderStgCreateInfo(m_preFilterEnvMapPsShaderModule,
        VK_SHADER_STAGE_FRAGMENT_BIT);

    m_preFilterEnvMapPipeline.SetShaderStageInfo(shaderStgsInfo, 2);
    m_preFilterEnvMapPipeline.SetPipelineLayout(m_preFilterEnvMapPipelineLayout);
    m_preFilterEnvMapPipeline.CreatePipeline(m_device);
}

// ================================================================================================================
void GenIBL::InitPrefilterEnvMapOutputObjects()
{
    VmaAllocationCreateInfo preFilterEnvMapAllocInfo{};
    {
        preFilterEnvMapAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        preFilterEnvMapAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    VkExtent3D extent{};
    {
        extent.width = m_hdrCubeMapInfo.width;
        extent.height = m_hdrCubeMapInfo.width;
        extent.depth = 1;
    }

    VkImageCreateInfo cubeMapImgInfo{};
    {
        cubeMapImgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        cubeMapImgInfo.imageType = VK_IMAGE_TYPE_2D;
        cubeMapImgInfo.format = HdriRenderTargetFormat;
        cubeMapImgInfo.extent = extent;
        cubeMapImgInfo.mipLevels = 8;
        cubeMapImgInfo.arrayLayers = 6;
        cubeMapImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        cubeMapImgInfo.tiling = VK_IMAGE_TILING_LINEAR;
        cubeMapImgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        // cubeMapImgInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT; // It's just an output. We don't need a cubemap sampler.
        cubeMapImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    VK_CHECK(vmaCreateImage(*m_pAllocator,
        &cubeMapImgInfo,
        &preFilterEnvMapAllocInfo,
        &m_preFilterEnvMapCubemap,
        &m_preFilterEnvMapCubemapAlloc,
        nullptr));

    VkImageViewCreateInfo info{};
    {
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image = m_preFilterEnvMapCubemap;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        info.format = HdriRenderTargetFormat;
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.levelCount = 8;
        info.subresourceRange.layerCount = 6;
    }
    VK_CHECK(vkCreateImageView(m_device, &info, nullptr, &m_preFilterEnvMapCubemapImageView));
}