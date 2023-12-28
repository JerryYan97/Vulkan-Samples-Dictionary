#include "GenIBL.h"
#include "vk_mem_alloc.h"
#include "../../SharedLibrary/Utils/VulkanDbgUtils.h"

// ================================================================================================================
void GenIBL::InitDiffuseIrradiancePipeline()
{
    VkPipelineRenderingCreateInfoKHR diffuseIrradianceCreateInfo{};
    {
        diffuseIrradianceCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        diffuseIrradianceCreateInfo.viewMask = 0x3F;
        diffuseIrradianceCreateInfo.colorAttachmentCount = 1;
        diffuseIrradianceCreateInfo.pColorAttachmentFormats = &HdriRenderTargetFormat;
    }

    m_diffuseIrradiancePipeline.SetPNext(&diffuseIrradianceCreateInfo);

    VkPipelineShaderStageCreateInfo shaderStgsInfo[2] = {};
    shaderStgsInfo[0] = CreateDefaultShaderStgCreateInfo(m_diffuseIrradianceVsShaderModule,
                                                         VK_SHADER_STAGE_VERTEX_BIT);
    shaderStgsInfo[1] = CreateDefaultShaderStgCreateInfo(m_diffuseIrradiancePsShaderModule,
                                                         VK_SHADER_STAGE_FRAGMENT_BIT);

    m_diffuseIrradiancePipeline.SetShaderStageInfo(shaderStgsInfo, 2);
    m_diffuseIrradiancePipeline.SetPipelineLayout(m_diffuseIrradiancePipelineLayout);
    m_diffuseIrradiancePipeline.CreatePipeline(m_device);
}

// ================================================================================================================
void GenIBL::InitDiffuseIrradiancePipelineLayout()
{
    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    {
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_diffIrrPreFilterEnvMapDesSet0Layout;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
    }

    VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_diffuseIrradiancePipelineLayout));
}

// ================================================================================================================
void GenIBL::InitDiffuseIrradianceShaderModules()
{
    m_diffuseIrradianceVsShaderModule = CreateShaderModule("/hlsl/diffuseIrradiance_vert.spv");
    m_diffuseIrradiancePsShaderModule = CreateShaderModule("/hlsl/diffuseIrradiance_frag.spv");
}

// ================================================================================================================
void GenIBL::DestroyDiffuseIrradiancePipelineResourses()
{
    vkDestroyShaderModule(m_device, m_diffuseIrradianceVsShaderModule, nullptr);
    vkDestroyShaderModule(m_device, m_diffuseIrradiancePsShaderModule, nullptr);

    // Destroy the pipeline layout
    vkDestroyPipelineLayout(m_device, m_diffuseIrradiancePipelineLayout, nullptr);

    vmaDestroyImage(*m_pAllocator, m_diffuseIrradianceCubemap, m_diffuseIrradianceCubemapAlloc);
    vkDestroyImageView(m_device, m_diffuseIrradianceCubemapImageView, nullptr);
}

// ================================================================================================================
void GenIBL::InitDiffuseIrradianceOutputObjects()
{
    VmaAllocationCreateInfo diffIrrMapAllocInfo{};
    {
        diffIrrMapAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        diffIrrMapAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
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
        cubeMapImgInfo.mipLevels = 1;
        cubeMapImgInfo.arrayLayers = 6;
        cubeMapImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        // cubeMapImgInfo.tiling = VK_IMAGE_TILING_LINEAR;
        cubeMapImgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        cubeMapImgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        // cubeMapImgInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT; // It's just an output. We don't need a cubemap sampler.
        cubeMapImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    VK_CHECK(vmaCreateImage(*m_pAllocator,
        &cubeMapImgInfo,
        &diffIrrMapAllocInfo,
        &m_diffuseIrradianceCubemap,
        &m_diffuseIrradianceCubemapAlloc,
        nullptr));

    VkImageViewCreateInfo info{};
    {
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image = m_diffuseIrradianceCubemap;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        info.format = HdriRenderTargetFormat;
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.layerCount = 6;
    }
    VK_CHECK(vkCreateImageView(m_device, &info, nullptr, &m_diffuseIrradianceCubemapImageView));
}