#include "GenIBL.h"
#include "../../SharedLibrary/Utils/VulkanDbgUtils.h"
#include "vk_mem_alloc.h"
#include "hlsl/g_envBrdf_vert.h"
#include "hlsl/g_envBrdf_frag.h"

// ================================================================================================================
void GenIBL::InitEnvBrdfPipeline()
{
    VkPipelineRenderingCreateInfoKHR envBrdfCreateInfo{};
    {
        envBrdfCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        envBrdfCreateInfo.colorAttachmentCount = 1;
        envBrdfCreateInfo.pColorAttachmentFormats = &HdriRenderTargetFormat;
    }

    m_envBrdfPipeline.SetPNext(&envBrdfCreateInfo);

    VkPipelineShaderStageCreateInfo shaderStgsInfo[2] = {};
    shaderStgsInfo[0] = CreateDefaultShaderStgCreateInfo(m_envBrdfVsShaderModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStgsInfo[1] = CreateDefaultShaderStgCreateInfo(m_envBrdfPsShaderModule, VK_SHADER_STAGE_FRAGMENT_BIT);

    m_envBrdfPipeline.SetShaderStageInfo(shaderStgsInfo, 2);
    m_envBrdfPipeline.SetPipelineLayout(m_envBrdfPipelineLayout);
    m_envBrdfPipeline.CreatePipeline(m_device);
}

// ================================================================================================================
void GenIBL::InitEnvBrdfPipelineLayout()
{
    VkPushConstantRange pushConstantRange{};
    {
        pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(float) * 2;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    {
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    }

    VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_envBrdfPipelineLayout));
}

// ================================================================================================================
void GenIBL::InitEnvBrdfShaderModules()
{
    // m_envBrdfVsShaderModule = CreateShaderModule("/hlsl/envBrdf_vert.spv");
    // m_envBrdfPsShaderModule = CreateShaderModule("/hlsl/envBrdf_frag.spv");

    m_envBrdfVsShaderModule = CreateShaderModuleFromRam((uint32_t*)SharedLib::envBrdf_vertScript,
                                                        sizeof(SharedLib::envBrdf_vertScript));

    m_envBrdfPsShaderModule = CreateShaderModuleFromRam((uint32_t*)SharedLib::envBrdf_fragScript,
                                                        sizeof(SharedLib::envBrdf_fragScript));

}

// ================================================================================================================
void GenIBL::InitEnvBrdfOutputObjects()
{
    VmaAllocationCreateInfo envBrdfMapAllocInfo{};
    {
        envBrdfMapAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        envBrdfMapAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    VkExtent3D extent{};
    {
        extent.width = EnvBrdfMapDim;
        extent.height = EnvBrdfMapDim;
        extent.depth = 1;
    }

    VkImageCreateInfo envBrdfMapImgInfo{};
    {
        envBrdfMapImgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        envBrdfMapImgInfo.imageType = VK_IMAGE_TYPE_2D;
        envBrdfMapImgInfo.format = HdriRenderTargetFormat;
        envBrdfMapImgInfo.extent = extent;
        envBrdfMapImgInfo.mipLevels = 1;
        envBrdfMapImgInfo.arrayLayers = 1;
        envBrdfMapImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        // envBrdfMapImgInfo.tiling = VK_IMAGE_TILING_LINEAR;
        envBrdfMapImgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        envBrdfMapImgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        envBrdfMapImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    VK_CHECK(vmaCreateImage(*m_pAllocator,
        &envBrdfMapImgInfo,
        &envBrdfMapAllocInfo,
        &m_envBrdfOutputImg,
        &m_envBrdfOutputImgAlloc,
        nullptr));

    VkImageViewCreateInfo info{};
    {
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image = m_envBrdfOutputImg;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = HdriRenderTargetFormat;
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.layerCount = 1;
    }
    VK_CHECK(vkCreateImageView(m_device, &info, nullptr, &m_envBrdfOutputImgView));
}

// ================================================================================================================
void GenIBL::DestroyEnvBrdfPipelineResources()
{
    vkDestroyShaderModule(m_device, m_envBrdfVsShaderModule, nullptr);
    vkDestroyShaderModule(m_device, m_envBrdfPsShaderModule, nullptr);

    vkDestroyPipelineLayout(m_device, m_envBrdfPipelineLayout, nullptr);

    vmaDestroyImage(*m_pAllocator, m_envBrdfOutputImg, m_envBrdfOutputImgAlloc);
    vkDestroyImageView(m_device, m_envBrdfOutputImgView, nullptr);
}