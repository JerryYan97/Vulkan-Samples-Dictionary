#include "SphericalToCubemap.h"
#include "../../SharedLibrary/Utils/VulkanDbgUtils.h"
#include "../../SharedLibrary/Camera/Camera.h"
#include "../../SharedLibrary/Utils/MathUtils.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STBI_MSC_SECURE_CRT
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <cassert>

#include "vk_mem_alloc.h"

// ================================================================================================================
SphericalToCubemap::SphericalToCubemap() :
    Application(),
    m_uboBuffer(VK_NULL_HANDLE),
    m_uboAlloc(VK_NULL_HANDLE),
    m_inputHdri(VK_NULL_HANDLE),
    m_inputHdriAlloc(VK_NULL_HANDLE),
    m_inputHdriImageView(VK_NULL_HANDLE),
    m_inputHdriSampler(VK_NULL_HANDLE),
    m_outputCubemap(VK_NULL_HANDLE),
    m_outputCubemapAlloc(VK_NULL_HANDLE),
    m_outputCubemapImageView(VK_NULL_HANDLE),
    m_pipelineDescriptorSet0(VK_NULL_HANDLE),
    m_vsShaderModule(VK_NULL_HANDLE),
    m_psShaderModule(VK_NULL_HANDLE),
    m_pipelineDesSet0Layout(VK_NULL_HANDLE),
    m_pipelineLayout(VK_NULL_HANDLE),
    m_pipeline(),
    m_hdriData(nullptr),
    m_width(0),
    m_height(0),
    m_outputCubemapExtent(),
    m_vsFormatShaderModule(VK_NULL_HANDLE),
    m_psFormatShaderModule(VK_NULL_HANDLE),
    m_formatPipelineDesSet0Layout(VK_NULL_HANDLE),
    m_formatPipelineLayout(VK_NULL_HANDLE),
    m_formatPipeline(),
    m_formatWidthHeightBuffer(VK_NULL_HANDLE),
    m_formatWidthHeightAlloc(VK_NULL_HANDLE),
    m_formatPipelineDescriptorSet0(VK_NULL_HANDLE)
{
}

// ================================================================================================================
SphericalToCubemap::~SphericalToCubemap()
{
    vkDeviceWaitIdle(m_device);
    delete m_hdriData;

    DestroyHdriGpuObjects();
    DestroyFormatImgsObjects();

    vmaDestroyBuffer(*m_pAllocator, m_uboBuffer, m_uboAlloc);
    vmaDestroyBuffer(*m_pAllocator, m_formatWidthHeightBuffer, m_formatWidthHeightAlloc);

    // Destroy shader modules
    vkDestroyShaderModule(m_device, m_vsShaderModule, nullptr);
    vkDestroyShaderModule(m_device, m_psShaderModule, nullptr);

    vkDestroyShaderModule(m_device, m_vsFormatShaderModule, nullptr);
    vkDestroyShaderModule(m_device, m_psFormatShaderModule, nullptr);

    // Destroy the pipeline layout
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    vkDestroyPipelineLayout(m_device, m_formatPipelineLayout, nullptr);

    // Destroy the descriptor set layout
    vkDestroyDescriptorSetLayout(m_device, m_pipelineDesSet0Layout, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_formatPipelineDesSet0Layout, nullptr);
}

// ================================================================================================================
void SphericalToCubemap::DestroyHdriGpuObjects()
{
    vkDestroySampler(m_device, m_inputHdriSampler, nullptr);
    vkDestroyImageView(m_device, m_inputHdriImageView, nullptr);
    vkDestroyImageView(m_device, m_outputCubemapImageView, nullptr);
    vmaDestroyImage(*m_pAllocator, m_inputHdri, m_inputHdriAlloc);
    vmaDestroyImage(*m_pAllocator, m_outputCubemap, m_outputCubemapAlloc);
}

// ================================================================================================================
void SphericalToCubemap::DestroyFormatImgsObjects()
{
    for (uint32_t i = 0; i < m_formatInputImages.size(); i++)
    {
        vkDestroySampler(m_device, m_formatInputImagesSamplers[i], nullptr);
        vkDestroyImageView(m_device, m_formatInputImagesViews[i], nullptr);
        vmaDestroyImage(*m_pAllocator, m_formatInputImages[i], m_formatInputImagesAllocs[i]);
    }
}

// ================================================================================================================
void SphericalToCubemap::ReadInHdri(const std::string& namePath)
{
    int nrComponents, width, height;
    m_hdriData = stbi_loadf(namePath.c_str(), &width, &height, &nrComponents, 0);

    m_width = (uint32_t)width;
    m_height = (uint32_t)height;
}

// ================================================================================================================
void SphericalToCubemap::SaveCubemap(
    const std::string& namePath, 
    uint32_t width, 
    uint32_t height, 
    uint32_t components, 
    float* pData)
{
    int res = stbi_write_hdr(namePath.c_str(), width, height, components, pData);
    if (res > 0)
    {
        std::cout << "Cubemap saves successfully." << std::endl;
    }
    else
    {
        std::cout << "Cubemap fails to save." << std::endl;
    }
}

// ================================================================================================================
void SphericalToCubemap::InitHdriGpuObjects()
{
    assert(m_hdriData != nullptr);
    
    // Create GPU objects
    VmaAllocationCreateInfo hdrAllocInfo{};
    {
        hdrAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        hdrAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    VkExtent3D inputHdriExtent{};
    {
        inputHdriExtent.width = m_width;
        inputHdriExtent.height = m_height;
        inputHdriExtent.depth = 1;
    }

    VkImageCreateInfo hdriImgInfo{};
    {
        hdriImgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        hdriImgInfo.imageType = VK_IMAGE_TYPE_2D;
        hdriImgInfo.format = VK_FORMAT_R32G32B32_SFLOAT;
        hdriImgInfo.extent = inputHdriExtent;
        hdriImgInfo.mipLevels = 1;
        hdriImgInfo.arrayLayers = 1;
        hdriImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        hdriImgInfo.tiling = VK_IMAGE_TILING_LINEAR;
        hdriImgInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        hdriImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    VK_CHECK(vmaCreateImage(*m_pAllocator,
                            &hdriImgInfo,
                            &hdrAllocInfo,
                            &m_inputHdri,
                            &m_inputHdriAlloc,
                            nullptr));

    VkImageViewCreateInfo hdriImgViewInfo{};
    {
        hdriImgViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        hdriImgViewInfo.image = m_inputHdri;
        hdriImgViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        hdriImgViewInfo.format = VK_FORMAT_R32G32B32_SFLOAT;
        hdriImgViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        hdriImgViewInfo.subresourceRange.levelCount = 1;
        hdriImgViewInfo.subresourceRange.layerCount = 1;
    }
    VK_CHECK(vkCreateImageView(m_device, &hdriImgViewInfo, nullptr, &m_inputHdriImageView));
    
    {
        m_outputCubemapExtent.width  = m_height / 2;
        m_outputCubemapExtent.height = m_height / 2;
        m_outputCubemapExtent.depth  = 1;
    }

    VkImageCreateInfo cubeMapImgInfo{};
    {
        cubeMapImgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        cubeMapImgInfo.imageType = VK_IMAGE_TYPE_2D;
        // cubeMapImgInfo.format = VK_FORMAT_R32G32B32_SFLOAT; // The color attachment format must has an 'A' element
        cubeMapImgInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        cubeMapImgInfo.extent = m_outputCubemapExtent;
        cubeMapImgInfo.mipLevels = 1;
        cubeMapImgInfo.arrayLayers = 6;
        cubeMapImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        cubeMapImgInfo.tiling = VK_IMAGE_TILING_LINEAR;
        cubeMapImgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        // cubeMapImgInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT; // It's just an output. We don't need a cubemap sampler.
        cubeMapImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    // CheckVkImageSupport(m_physicalDevice, cubeMapImgInfo);
    // PrintFormatForColorRenderTarget(m_physicalDevice);

    VK_CHECK(vmaCreateImage(*m_pAllocator,
                            &cubeMapImgInfo,
                            &hdrAllocInfo,
                            &m_outputCubemap,
                            &m_outputCubemapAlloc,
                            nullptr));

    VkImageViewCreateInfo outputCubemapInfo{};
    {
        outputCubemapInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        outputCubemapInfo.image = m_outputCubemap;
        outputCubemapInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        outputCubemapInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        outputCubemapInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        outputCubemapInfo.subresourceRange.levelCount = 1;
        outputCubemapInfo.subresourceRange.layerCount = 6;
        outputCubemapInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        outputCubemapInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        outputCubemapInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        outputCubemapInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    }
    VK_CHECK(vkCreateImageView(m_device, &outputCubemapInfo, nullptr, &m_outputCubemapImageView));

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
    VK_CHECK(vkCreateSampler(m_device, &sampler_info, nullptr, &m_inputHdriSampler));
}

// ================================================================================================================
void SphericalToCubemap::InitPipelineDescriptorSetLayout()
{
    // Create pipeline binding and descriptor objects for the camera parameters
    VkDescriptorSetLayoutBinding sceneInfoUboBinding{};
    {
        sceneInfoUboBinding.binding = 1;
        sceneInfoUboBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        sceneInfoUboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        sceneInfoUboBinding.descriptorCount = 1;
    }
    
    // Create pipeline binding objects for the HDRI image
    VkDescriptorSetLayoutBinding hdriSamplerBinding{};
    {
        hdriSamplerBinding.binding = 0;
        hdriSamplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        hdriSamplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        hdriSamplerBinding.descriptorCount = 1;
    }
    
    // Create pipeline's descriptors layout
    VkDescriptorSetLayoutBinding pipelineDesSet0LayoutBindings[2] = { hdriSamplerBinding, sceneInfoUboBinding };

    VkDescriptorSetLayoutCreateInfo pipelineDesSet0LayoutInfo{};
    {
        pipelineDesSet0LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        pipelineDesSet0LayoutInfo.bindingCount = 2;
        pipelineDesSet0LayoutInfo.pBindings = pipelineDesSet0LayoutBindings;
    }

    VK_CHECK(vkCreateDescriptorSetLayout(m_device,
                                         &pipelineDesSet0LayoutInfo,
                                         nullptr,
                                         &m_pipelineDesSet0Layout));
}

// ================================================================================================================
void SphericalToCubemap::InitPipelineLayout()
{
    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    {
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_pipelineDesSet0Layout;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
    }

    VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout));
}

// ================================================================================================================
void SphericalToCubemap::InitShaderModules()
{
    m_vsShaderModule = CreateShaderModule("/ToCubeMap_vert.spv");
    m_psShaderModule = CreateShaderModule("/ToCubeMap_frag.spv");
}

// ================================================================================================================
void SphericalToCubemap::InitSceneBufferInfo()
{
    VmaAllocationCreateInfo uboBufAllocInfo{};
    {
        uboBufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        uboBufAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    VkDeviceSize bufferBytesCnt = sizeof(float) * ((4 * 4) * 6 + 4);
    VkBufferCreateInfo uboBufInfo{};
    {
        uboBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        uboBufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        uboBufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        uboBufInfo.size = bufferBytesCnt; // 6 4x4 matrices and a vec4. 
                                          // Note that the alignment is 4 floats.
    }

    VK_CHECK(vmaCreateBuffer(*m_pAllocator, &uboBufInfo, &uboBufAllocInfo, &m_uboBuffer, &m_uboAlloc, nullptr));

    // Create the buffer in the RAM
    float uboBufferData[((4 * 4) * 6 + 4)];
    memset(uboBufferData, 0, sizeof(uboBufferData));

    // Front
    float matFront[9];
    memset(matFront, 0, sizeof(float) * 9);
    matFront[0] = 1.f;
    matFront[4] = 1.f;
    matFront[8] = 1.f;
    
    float matFront_4x4[16]{};
    SharedLib::Mat3x3ToMat4x4(matFront, matFront_4x4);

    // Left -- Pan by 90 degrees
    float matLeft[9];
    memset(matLeft, 0, sizeof(float) * 9);
    SharedLib::GenRotationMatZ(M_PI / 2.f, matLeft);
    
    float matLeft_4x4[16]{};
    SharedLib::Mat3x3ToMat4x4(matLeft, matLeft_4x4);
    SharedLib::MatTranspose(matLeft_4x4, 4);


    // Right -- Pan by -90 degrees
    float matRight[9];
    memset(matRight, 0, sizeof(float) * 9);
    SharedLib::GenRotationMatZ(-M_PI / 2.f, matRight);

    float vecY[3] = { 0.f, 1.f, 0.f };
    float vecRes[3] = { 0.f, 0.f, 0.f };
    SharedLib::MatMulVec(matRight, vecY, 3, vecRes);
    
    float matRight_4x4[16]{};
    SharedLib::Mat3x3ToMat4x4(matRight, matRight_4x4);
    SharedLib::MatTranspose(matRight_4x4, 4);


    // Back -- Pan by 180 degrees
    float matBack[9];
    memset(matBack, 0, sizeof(float) * 9);
    SharedLib::GenRotationMatZ(M_PI, matBack);

    float matBack_4x4[16]{};
    SharedLib::Mat3x3ToMat4x4(matBack, matBack_4x4);
    SharedLib::MatTranspose(matBack_4x4, 4);


    // Top -- Tilt by -90 degrees
    float matTop[9];
    memset(matTop, 0, sizeof(float) * 9);
    SharedLib::GenRotationMatX(M_PI / 2.f, matTop);

    float matTop_4x4[16]{};
    SharedLib::Mat3x3ToMat4x4(matTop, matTop_4x4);
    SharedLib::MatTranspose(matTop_4x4, 4);


    // Bottom -- Tilt by 90 degrees
    float matBottom[9];
    memset(matBottom, 0, sizeof(float) * 9);
    SharedLib::GenRotationMatX(-M_PI / 2.f, matBottom);

    float matBottom_4x4[16]{};
    SharedLib::Mat3x3ToMat4x4(matBottom, matBottom_4x4);
    SharedLib::MatTranspose(matBottom_4x4, 4);

    // Put matrices to the larger buffer
    // Vulkan cubemap sequence: pos-x, neg-x, pos-y, neg-y, pos-z, neg-z.
    memcpy(uboBufferData,      matFront_4x4, sizeof(float) * 16);
    memcpy(&uboBufferData[16], matBack_4x4, sizeof(float) * 16);
    memcpy(&uboBufferData[32], matTop_4x4, sizeof(float) * 16);
    memcpy(&uboBufferData[48], matBottom_4x4, sizeof(float) * 16);
    memcpy(&uboBufferData[64], matRight_4x4, sizeof(float) * 16);
    memcpy(&uboBufferData[80], matLeft_4x4, sizeof(float) * 16);

    // Put the viewport sizes into the large buffer
    uboBufferData[96] = m_outputCubemapExtent.width;
    uboBufferData[97] = m_outputCubemapExtent.height;

    // Send data to gpu
    CopyRamDataToGpuBuffer(uboBufferData, m_uboBuffer, m_uboAlloc, bufferBytesCnt);
}

// ================================================================================================================
void SphericalToCubemap::InitPipelineDescriptorSet()
{
    // Create pipeline descirptor
    VkDescriptorSetAllocateInfo pipelineDesSet0AllocInfo{};
    {
        pipelineDesSet0AllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        pipelineDesSet0AllocInfo.descriptorPool = m_descriptorPool;
        pipelineDesSet0AllocInfo.pSetLayouts = &m_pipelineDesSet0Layout;
        pipelineDesSet0AllocInfo.descriptorSetCount = 1;
    }

    VK_CHECK(vkAllocateDescriptorSets(m_device,
                                      &pipelineDesSet0AllocInfo,
                                      &m_pipelineDescriptorSet0));

    // Link descriptors to the buffer and image
    VkDescriptorImageInfo hdriDesImgInfo{};
    {
        hdriDesImgInfo.imageView = m_inputHdriImageView;
        hdriDesImgInfo.sampler = m_inputHdriSampler;
        hdriDesImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    VkDescriptorBufferInfo sceneBufferInfo{};
    {
        sceneBufferInfo.buffer = m_uboBuffer;
        sceneBufferInfo.offset = 0;
        sceneBufferInfo.range = sizeof(float) * ((4 * 4) * 6 + 4);
    }

    VkWriteDescriptorSet writeUboBufDesSet{};
    {
        writeUboBufDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeUboBufDesSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeUboBufDesSet.dstSet = m_pipelineDescriptorSet0;
        writeUboBufDesSet.dstBinding = 1;
        writeUboBufDesSet.descriptorCount = 1;
        writeUboBufDesSet.pBufferInfo = &sceneBufferInfo;
    }

    VkWriteDescriptorSet writeHdrDesSet{};
    {
        writeHdrDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeHdrDesSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeHdrDesSet.dstSet = m_pipelineDescriptorSet0;
        writeHdrDesSet.dstBinding = 0;
        writeHdrDesSet.pImageInfo = &hdriDesImgInfo;
        writeHdrDesSet.descriptorCount = 1;
    }

    // Linking pipeline descriptors: cubemap and scene buffer descriptors to their GPU memory and info.
    VkWriteDescriptorSet writeSkyboxPipelineDescriptors[2] = { writeHdrDesSet, writeUboBufDesSet };
    vkUpdateDescriptorSets(m_device, 2, writeSkyboxPipelineDescriptors, 0, NULL);
}

// ================================================================================================================
void SphericalToCubemap::InitPipeline()
{
    VkFormat colorAttachmentFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
    VkPipelineRenderingCreateInfoKHR pipelineRenderCreateInfo{};
    {
        pipelineRenderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        pipelineRenderCreateInfo.viewMask = 0x3F;
        pipelineRenderCreateInfo.colorAttachmentCount = 1;
        pipelineRenderCreateInfo.pColorAttachmentFormats = &colorAttachmentFormat;
    }

    m_pipeline.SetPNext(&pipelineRenderCreateInfo);

    VkPipelineShaderStageCreateInfo shaderStgsInfo[2] = {};
    shaderStgsInfo[0] = CreateDefaultShaderStgCreateInfo(m_vsShaderModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStgsInfo[1] = CreateDefaultShaderStgCreateInfo(m_psShaderModule, VK_SHADER_STAGE_FRAGMENT_BIT);

    m_pipeline.SetShaderStageInfo(shaderStgsInfo, 2);
    m_pipeline.SetPipelineLayout(m_pipelineLayout);
    m_pipeline.CreatePipeline(m_device);
}

// ================================================================================================================
void SphericalToCubemap::InitFormatImgsObjects()
{
    // Create GPU objects
    VmaAllocationCreateInfo formatImgAllocInfo{};
    {
        formatImgAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        formatImgAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    VkImageCreateInfo formatImgInfo{};
    {
        formatImgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        formatImgInfo.imageType = VK_IMAGE_TYPE_2D;
        formatImgInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        formatImgInfo.extent = m_outputCubemapExtent;
        formatImgInfo.mipLevels = 1;
        formatImgInfo.arrayLayers = 1;
        formatImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        formatImgInfo.tiling = VK_IMAGE_TILING_LINEAR;
        formatImgInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        formatImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    VkSamplerCreateInfo samplerInfo{};
    {
        samplerInfo.sType         = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter     = VK_FILTER_LINEAR;
        samplerInfo.minFilter     = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode    = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU  = VK_SAMPLER_ADDRESS_MODE_REPEAT; // outside image bounds just use border color
        samplerInfo.addressModeV  = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW  = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.minLod        = -1000;
        samplerInfo.maxLod        = 1000;
        samplerInfo.maxAnisotropy = 1.0f;
    }

    m_formatInputImages.resize(6);
    m_formatInputImagesViews.resize(6);
    m_formatInputImagesAllocs.resize(6);
    m_formatInputImagesSamplers.resize(6);
    for (uint32_t i = 0; i < 6; i++)
    {
        VK_CHECK(vmaCreateImage(*m_pAllocator,
                                &formatImgInfo,
                                &formatImgAllocInfo,
                                &m_formatInputImages[i],
                                &m_formatInputImagesAllocs[i],
                                nullptr));

        VkImageViewCreateInfo formatImgViewInfo{};
        {
            formatImgViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            formatImgViewInfo.image = m_formatInputImages[i];
            formatImgViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            formatImgViewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
            formatImgViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            formatImgViewInfo.subresourceRange.levelCount = 1;
            formatImgViewInfo.subresourceRange.layerCount = 1;
        }

        VK_CHECK(vkCreateImageView(m_device, &formatImgViewInfo, nullptr, &m_formatInputImagesViews[i]));
        VK_CHECK(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_formatInputImagesSamplers[i]));
    }
}

// ================================================================================================================
void SphericalToCubemap::InitFormatPipeline()
{
    VkFormat colorAttachmentFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
    VkPipelineRenderingCreateInfoKHR pipelineRenderCreateInfo{};
    {
        pipelineRenderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        pipelineRenderCreateInfo.viewMask = 0x3F;
        pipelineRenderCreateInfo.colorAttachmentCount = 1;
        pipelineRenderCreateInfo.pColorAttachmentFormats = &colorAttachmentFormat;
    }

    m_formatPipeline.SetPNext(&pipelineRenderCreateInfo);

    VkPipelineShaderStageCreateInfo shaderStgsInfo[2] = {};
    shaderStgsInfo[0] = CreateDefaultShaderStgCreateInfo(m_vsFormatShaderModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStgsInfo[1] = CreateDefaultShaderStgCreateInfo(m_psFormatShaderModule, VK_SHADER_STAGE_FRAGMENT_BIT);

    m_formatPipeline.SetShaderStageInfo(shaderStgsInfo, 2);
    m_formatPipeline.SetPipelineLayout(m_formatPipelineLayout);
    m_formatPipeline.CreatePipeline(m_device);
}

// ================================================================================================================
void SphericalToCubemap::InitFormatPipelineDescriptorSetLayout()
{
    // Create pipeline binding and descriptor objects for the screen parameters
    VkDescriptorSetLayoutBinding widthHeightUboBinding{};
    {
        widthHeightUboBinding.binding = 1;
        widthHeightUboBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        widthHeightUboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        widthHeightUboBinding.descriptorCount = 1;
    }

    // Create pipeline binding objects for the 6 images
    VkDescriptorSetLayoutBinding imgsSamplerBinding{};
    {
        imgsSamplerBinding.binding = 0;
        imgsSamplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        imgsSamplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        imgsSamplerBinding.descriptorCount = 6;
    }

    // Create pipeline's descriptors layout
    VkDescriptorSetLayoutBinding pipelineDesSet0LayoutBindings[2] = { widthHeightUboBinding, imgsSamplerBinding };

    VkDescriptorSetLayoutCreateInfo pipelineDesSet0LayoutInfo{};
    {
        pipelineDesSet0LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        pipelineDesSet0LayoutInfo.bindingCount = 2;
        pipelineDesSet0LayoutInfo.pBindings = pipelineDesSet0LayoutBindings;
    }

    VK_CHECK(vkCreateDescriptorSetLayout(m_device,
                                         &pipelineDesSet0LayoutInfo,
                                         nullptr,
                                         &m_formatPipelineDesSet0Layout));
}

// ================================================================================================================
void SphericalToCubemap::InitFormatPipelineLayout()
{
    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    {
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_formatPipelineDesSet0Layout;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
    }

    VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_formatPipelineLayout));
}

// ================================================================================================================
void SphericalToCubemap::InitFormatShaderModules()
{
    m_vsFormatShaderModule = CreateShaderModule("/CubeMapFormat_vert.spv");
    m_psFormatShaderModule = CreateShaderModule("/CubeMapFormat_frag.spv");
}

// ================================================================================================================
void SphericalToCubemap::InitFormatPipelineDescriptorSet()
{
    // Create pipeline descirptor
    VkDescriptorSetAllocateInfo pipelineDesSet0AllocInfo{};
    {
        pipelineDesSet0AllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        pipelineDesSet0AllocInfo.descriptorPool = m_descriptorPool;
        pipelineDesSet0AllocInfo.pSetLayouts = &m_formatPipelineDesSet0Layout;
        pipelineDesSet0AllocInfo.descriptorSetCount = 1;
    }

    VK_CHECK(vkAllocateDescriptorSets(m_device,
                                      &pipelineDesSet0AllocInfo,
                                      &m_formatPipelineDescriptorSet0));

    // Link descriptors to the buffer and image
    VkDescriptorImageInfo formatImgInfo[6] = {};
    for(uint32_t i = 0; i < 6; i++)
    {
        formatImgInfo[i].imageView = m_formatInputImagesViews[i];
        formatImgInfo[i].sampler = m_formatInputImagesSamplers[i];
        formatImgInfo[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    VkDescriptorBufferInfo screenBufferInfo{};
    {
        screenBufferInfo.buffer = m_formatWidthHeightBuffer;
        screenBufferInfo.offset = 0;
        screenBufferInfo.range = sizeof(float) * 2;
    }

    VkWriteDescriptorSet writeUboBufDesSet{};
    {
        writeUboBufDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeUboBufDesSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeUboBufDesSet.dstSet = m_formatPipelineDescriptorSet0;
        writeUboBufDesSet.dstBinding = 1;
        writeUboBufDesSet.descriptorCount = 1;
        writeUboBufDesSet.pBufferInfo = &screenBufferInfo;
    }

    VkWriteDescriptorSet writeformatImgsDesSet{};
    {
        writeformatImgsDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeformatImgsDesSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeformatImgsDesSet.dstSet = m_formatPipelineDescriptorSet0;
        writeformatImgsDesSet.dstBinding = 0;
        writeformatImgsDesSet.pImageInfo = formatImgInfo;
        writeformatImgsDesSet.descriptorCount = 6;
    }

    // Linking pipeline descriptors: cubemap and scene buffer descriptors to their GPU memory and info.
    VkWriteDescriptorSet writeFormatPipelineDescriptors[2] = { writeformatImgsDesSet, writeUboBufDesSet };
    vkUpdateDescriptorSets(m_device, 2, writeFormatPipelineDescriptors, 0, NULL);
}

// ================================================================================================================
void SphericalToCubemap::InitWidthHeightBufferInfo()
{
    VmaAllocationCreateInfo screenBufAllocInfo{};
    {
        screenBufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        screenBufAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    VkDeviceSize bufferBytesCnt = sizeof(float) * 2;
    VkBufferCreateInfo screenBufInfo{};
    {
        screenBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        screenBufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        screenBufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        screenBufInfo.size = bufferBytesCnt;
    }

    VK_CHECK(vmaCreateBuffer(*m_pAllocator,
                             &screenBufInfo,
                             &screenBufAllocInfo,
                             &m_formatWidthHeightBuffer,
                             &m_formatWidthHeightAlloc,
                             nullptr));

    float widthHeight[2] = {m_outputCubemapExtent.width, m_outputCubemapExtent.height};
    CopyRamDataToGpuBuffer(widthHeight, m_formatWidthHeightBuffer, m_formatWidthHeightAlloc, sizeof(float) * 2);
}

// ================================================================================================================
void SphericalToCubemap::AppInit()
{
    std::vector<const char*> instExtensions;
    InitInstance(instExtensions, 0);

    InitPhysicalDevice();
    InitGfxQueueFamilyIdx();

    // Queue family index should be unique in vk1.2:
    // https://vulkan.lunarg.com/doc/view/1.2.198.0/windows/1.2-extensions/vkspec.html#VUID-VkDeviceCreateInfo-queueFamilyIndex-02802
    std::vector<VkDeviceQueueCreateInfo> deviceQueueInfos = CreateDeviceQueueInfos({ m_graphicsQueueFamilyIdx });
    // We need the swap chain device extension and the dynamic rendering extension.
    const std::vector<const char*> deviceExtensions = { VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, VK_KHR_MULTIVIEW_EXTENSION_NAME };

    VkPhysicalDeviceVulkan11Features vulkan11Features{};
    {
        vulkan11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        vulkan11Features.multiview = VK_TRUE;
    }

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeature{};
    {
        dynamicRenderingFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
        dynamicRenderingFeature.pNext = &vulkan11Features;
        dynamicRenderingFeature.dynamicRendering = VK_TRUE;
    }

    InitDevice(deviceExtensions, deviceExtensions.size(), deviceQueueInfos, &dynamicRenderingFeature);
    InitVmaAllocator();
    InitGraphicsQueue();
    InitDescriptorPool();

    InitGfxCommandPool();
    InitGfxCommandBuffers(1);

    InitShaderModules();
    InitPipelineDescriptorSetLayout();
    InitPipelineLayout();
    InitPipeline();

    InitHdriGpuObjects();
    InitSceneBufferInfo();
    InitPipelineDescriptorSet();

    InitFormatShaderModules();
    InitFormatPipelineDescriptorSetLayout();
    InitFormatPipelineLayout();
    InitFormatPipeline();

    InitFormatImgsObjects();
    InitWidthHeightBufferInfo();
    InitFormatPipelineDescriptorSet();
}