#include "AppUtils.h"
#include "../../SharedLibrary/Utils/VulkanDbgUtils.h"
#include "../../SharedLibrary/Utils/CmdBufUtils.h"
#include "../../SharedLibrary/Utils/DiskOpsUtils.h"
#include "../../SharedLibrary/HLSL/g_cubemapFormat_vert.h"
#include "../../SharedLibrary/HLSL/g_cubemapFormat_frag.h"
#include <fstream>
#include <cassert>

namespace SharedLib
{
    // ================================================================================================================
    VkPipelineShaderStageCreateInfo CreateDefaultShaderStgCreateInfo(
        const VkShaderModule& shaderModule,
        const VkShaderStageFlagBits stg)
    {
        VkPipelineShaderStageCreateInfo info{};
        {
            info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            info.pNext = nullptr;
            info.flags = 0;
            info.stage = stg;
            info.module = shaderModule;
            info.pName = "main";
            info.pSpecializationInfo = nullptr;
        }
        return info;
    }

    // ================================================================================================================
    void CopyRamDataToGpuBuffer(
        void*         pSrc,
        VmaAllocator* pAllocator,
        VkBuffer      dstBuffer,
        VmaAllocation dstAllocation,
        uint32_t      byteNum)
    {
        void* pBufferDataMap;
        VK_CHECK(vmaMapMemory(*pAllocator, dstAllocation, &pBufferDataMap));
        memcpy(pBufferDataMap, pSrc, byteNum);
        vmaUnmapMemory(*pAllocator, dstAllocation);
    }

    // ================================================================================================================
    AppUtil::AppUtil() :
        m_pPipeline(nullptr),
        m_vkInfos()
    {

    }

    // ================================================================================================================
    VkShaderModule AppUtil::CreateShaderModule(
        const std::string& spvName)
    {
        // Create  Shader Module -- SOURCE_PATH is a MACRO definition passed in during compilation, which is specified
        //                          in the CMakeLists.txt file in the same level of repository.
        std::string shaderPath = std::string(SHARED_LIB_PATH) + spvName;
        std::ifstream inputShader(shaderPath.c_str(), std::ios::binary | std::ios::in);
        std::vector<unsigned char> inputShaderStr(std::istreambuf_iterator<char>(inputShader), {});
        inputShader.close();
        VkShaderModuleCreateInfo shaderModuleCreateInfo{};
        {
            shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            shaderModuleCreateInfo.codeSize = inputShaderStr.size();
            shaderModuleCreateInfo.pCode = (uint32_t*)inputShaderStr.data();
        }
        VkShaderModule shaderModule;
        CheckVkResult(vkCreateShaderModule(m_vkInfos.device, &shaderModuleCreateInfo, nullptr, &shaderModule));

        return shaderModule;
    }

    // ================================================================================================================
    VkShaderModule AppUtil::CreateShaderModuleFromRam(
        uint32_t* pCode,
        uint32_t codeSizeInBytes)
    {
        VkShaderModuleCreateInfo shaderModuleCreateInfo{};
        {
            shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            shaderModuleCreateInfo.codeSize = codeSizeInBytes;
            shaderModuleCreateInfo.pCode = pCode;
        }
        VkShaderModule shaderModule;
        CheckVkResult(vkCreateShaderModule(m_vkInfos.device, &shaderModuleCreateInfo, nullptr, &shaderModule));

        return shaderModule;
    }

    // ================================================================================================================
    CubemapFormatTransApp::CubemapFormatTransApp() :
        m_vsFormatShaderModule(VK_NULL_HANDLE),
        m_psFormatShaderModule(VK_NULL_HANDLE),
        m_formatPipelineDesSet0Layout(VK_NULL_HANDLE),
        m_formatPipelineLayout(VK_NULL_HANDLE),
        m_formatWidthHeightBuffer(VK_NULL_HANDLE),
        m_formatWidthHeightAlloc(VK_NULL_HANDLE),
        m_inputCubemap(VK_NULL_HANDLE),
        m_inputCubemapExtent(),
        m_outputCubemap(VK_NULL_HANDLE),
        m_outputCubemapImgView(VK_NULL_HANDLE),
        m_outputCubemapAlloc(VK_NULL_HANDLE),
        m_formatInputImages(VK_NULL_HANDLE),
        m_formatInputImagesViews(VK_NULL_HANDLE),
        m_formatInputImagesAllocs(VK_NULL_HANDLE),
        m_formatInputImagesSamplers(VK_NULL_HANDLE),
        m_formatInputImageInfo(),
        m_formatWidthHeightDesBufferInfo(),
        m_inputSubres(),
        m_pfnVkCmdPushDescriptorSetKHR(nullptr)
    {

    }

    // ================================================================================================================
    void CubemapFormatTransApp::Init()
    {
        m_pPipeline = new Pipeline();

        InitFormatShaderModules();
        InitFormatPipelineDescriptorSetLayout();
        InitFormatPipelineLayout();
        InitFormatPipeline();
        InitFormatImgsObjects();
        InitWidthHeightBufferInfo();

        // Get necessary function pointers
        m_pfnVkCmdPushDescriptorSetKHR = (PFN_vkCmdPushDescriptorSetKHR)vkGetDeviceProcAddr(m_vkInfos.device,
                                                                                            "vkCmdPushDescriptorSetKHR");
        if (!m_pfnVkCmdPushDescriptorSetKHR) {
            exit(1);
        }
    }

    // ================================================================================================================
    void CubemapFormatTransApp::Destroy()
    {
        DestroyFormatImgsObjects();
        vmaDestroyBuffer(*m_vkInfos.pAllocator, m_formatWidthHeightBuffer, m_formatWidthHeightAlloc);
        vkDestroyShaderModule(m_vkInfos.device, m_vsFormatShaderModule, nullptr);
        vkDestroyShaderModule(m_vkInfos.device, m_psFormatShaderModule, nullptr);
        vkDestroyPipelineLayout(m_vkInfos.device, m_formatPipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_vkInfos.device, m_formatPipelineDesSet0Layout, nullptr);

        delete m_pPipeline;
    }

    // ================================================================================================================
    void CubemapFormatTransApp::CmdConvertCubemapFormat(
        VkCommandBuffer cmdBuffer)
    {
        VkImageSubresourceRange outputCubemapSubResRange{};
        {
            outputCubemapSubResRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            outputCubemapSubResRange.baseMipLevel = 0;
            outputCubemapSubResRange.levelCount = 1;
            outputCubemapSubResRange.baseArrayLayer = 0;
            outputCubemapSubResRange.layerCount = 6;
        }
        
        // Transform the layout of the cubemap from render target to copy src.
        // Transform the layout of the 6 images from undef to copy dst.
        VkImageMemoryBarrier stg1ImgsTrans[2] = {};
        {
            stg1ImgsTrans[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            stg1ImgsTrans[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            stg1ImgsTrans[0].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            stg1ImgsTrans[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            stg1ImgsTrans[0].image = m_inputCubemap;
            stg1ImgsTrans[0].subresourceRange = m_inputSubres;

            stg1ImgsTrans[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            stg1ImgsTrans[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            stg1ImgsTrans[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            stg1ImgsTrans[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            stg1ImgsTrans[1].image = m_formatInputImages;
            stg1ImgsTrans[1].subresourceRange = outputCubemapSubResRange;
        }

        vkCmdPipelineBarrier(cmdBuffer,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            2, stg1ImgsTrans);

        // Copy the cubemap to 6 separate images
        VkImageCopy imgCpyInfo{};
        {
            imgCpyInfo.srcOffset = { 0, 0, 0 };
            imgCpyInfo.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imgCpyInfo.srcSubresource.baseArrayLayer = 0;
            imgCpyInfo.srcSubresource.layerCount = 6;
            imgCpyInfo.srcSubresource.mipLevel = m_inputSubres.baseMipLevel;

            imgCpyInfo.dstOffset = { 0, 0, 0 };
            imgCpyInfo.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imgCpyInfo.dstSubresource.baseArrayLayer = 0;
            imgCpyInfo.dstSubresource.layerCount = 6;
            imgCpyInfo.dstSubresource.mipLevel = 0;

            imgCpyInfo.extent = m_inputCubemapExtent;
        }

        vkCmdCopyImage(cmdBuffer,
            m_inputCubemap,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            m_formatInputImages,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &imgCpyInfo);

        // Transform the layout of cubemap to render target
        // Transform the 6 images layout to ps shader input
        VkImageMemoryBarrier stg2ImgsTrans[2] = {};
        {
            stg2ImgsTrans[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            stg2ImgsTrans[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            stg2ImgsTrans[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            stg2ImgsTrans[0].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            stg2ImgsTrans[0].image = m_outputCubemap;
            stg2ImgsTrans[0].subresourceRange = outputCubemapSubResRange;

            stg2ImgsTrans[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            stg2ImgsTrans[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            stg2ImgsTrans[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            stg2ImgsTrans[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            stg2ImgsTrans[1].image = m_formatInputImages;
            stg2ImgsTrans[1].subresourceRange = outputCubemapSubResRange;
        }

        vkCmdPipelineBarrier(cmdBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            2, stg2ImgsTrans);

        VkClearValue clearColor = { {{1.0f, 0.0f, 0.0f, 1.0f}} };

        VkRenderingAttachmentInfoKHR renderAttachmentInfo{};
        {
            renderAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
            renderAttachmentInfo.imageView = m_outputCubemapImgView;
            renderAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            renderAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            renderAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            renderAttachmentInfo.clearValue = clearColor;
        }

        VkExtent2D colorRenderTargetExtent{};
        {
            colorRenderTargetExtent.width = m_inputCubemapExtent.width;
            colorRenderTargetExtent.height = m_inputCubemapExtent.height;
        }

        VkRenderingInfoKHR renderInfo{};
        {
            renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
            renderInfo.renderArea.offset = { 0, 0 };
            renderInfo.renderArea.extent = colorRenderTargetExtent;
            renderInfo.layerCount = 6;
            renderInfo.colorAttachmentCount = 1;
            renderInfo.viewMask = 0x3F;
            renderInfo.pColorAttachments = &renderAttachmentInfo;
        }

        vkCmdBeginRendering(cmdBuffer, &renderInfo);

        // Bind the graphics pipeline
        m_pfnVkCmdPushDescriptorSetKHR(cmdBuffer, 
                                       VK_PIPELINE_BIND_POINT_GRAPHICS,
                                       m_formatPipelineLayout,
                                       0, m_descriptorSet0Writes.size(), m_descriptorSet0Writes.data());

        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pPipeline->GetVkPipeline());

        // Set the viewport
        VkViewport viewport{};
        {
            viewport.x = 0.f;
            viewport.y = 0.f;
            viewport.width = (float)colorRenderTargetExtent.width;
            viewport.height = (float)colorRenderTargetExtent.height;
            viewport.minDepth = 0.f;
            viewport.maxDepth = 1.f;
        }
        vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

        // Set the scissor
        VkRect2D scissor{};
        {
            scissor.offset = { 0, 0 };
            scissor.extent = colorRenderTargetExtent;
            vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
        }

        vkCmdDraw(cmdBuffer, 6, 1, 0, 0);

        vkCmdEndRendering(cmdBuffer);

        // Transfer cubemap's layout to copy source
        VkImageMemoryBarrier cubemapColorAttToSrcBarrier{};
        {
            cubemapColorAttToSrcBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            cubemapColorAttToSrcBarrier.image = m_outputCubemap;
            cubemapColorAttToSrcBarrier.subresourceRange = outputCubemapSubResRange;
            cubemapColorAttToSrcBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            cubemapColorAttToSrcBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            cubemapColorAttToSrcBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            cubemapColorAttToSrcBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        }

        vkCmdPipelineBarrier(
            cmdBuffer,
            VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &cubemapColorAttToSrcBarrier);
    }

    // ================================================================================================================
    void CubemapFormatTransApp::InitFormatPipeline()
    {
        VkFormat colorAttachmentFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
        VkPipelineRenderingCreateInfoKHR pipelineRenderCreateInfo{};
        {
            pipelineRenderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
            pipelineRenderCreateInfo.viewMask = 0x3F;
            pipelineRenderCreateInfo.colorAttachmentCount = 1;
            pipelineRenderCreateInfo.pColorAttachmentFormats = &colorAttachmentFormat;
        }

        m_pPipeline->SetPNext(&pipelineRenderCreateInfo);

        VkPipelineShaderStageCreateInfo shaderStgsInfo[2] = {};
        shaderStgsInfo[0] = CreateDefaultShaderStgCreateInfo(m_vsFormatShaderModule, VK_SHADER_STAGE_VERTEX_BIT);
        shaderStgsInfo[1] = CreateDefaultShaderStgCreateInfo(m_psFormatShaderModule, VK_SHADER_STAGE_FRAGMENT_BIT);

        m_pPipeline->SetShaderStageInfo(shaderStgsInfo, 2);
        m_pPipeline->SetPipelineLayout(m_formatPipelineLayout);
        m_pPipeline->CreatePipeline(m_vkInfos.device);
    }

    // ================================================================================================================
    void CubemapFormatTransApp::InitFormatPipelineDescriptorSetLayout()
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
            imgsSamplerBinding.descriptorCount = 1;
        }

        // Create pipeline's descriptors layout
        VkDescriptorSetLayoutBinding pipelineDesSet0LayoutBindings[2] = { widthHeightUboBinding, imgsSamplerBinding };

        VkDescriptorSetLayoutCreateInfo pipelineDesSet0LayoutInfo{};
        {
            pipelineDesSet0LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            pipelineDesSet0LayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
            pipelineDesSet0LayoutInfo.bindingCount = 2;
            pipelineDesSet0LayoutInfo.pBindings = pipelineDesSet0LayoutBindings;
        }

        VK_CHECK(vkCreateDescriptorSetLayout(m_vkInfos.device,
            &pipelineDesSet0LayoutInfo,
            nullptr,
            &m_formatPipelineDesSet0Layout));
    }

    // ================================================================================================================
    void CubemapFormatTransApp::InitFormatPipelineLayout()
    {
        // Create pipeline layout
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        {
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = 1;
            pipelineLayoutInfo.pSetLayouts = &m_formatPipelineDesSet0Layout;
            pipelineLayoutInfo.pushConstantRangeCount = 0;
        }

        VK_CHECK(vkCreatePipelineLayout(m_vkInfos.device, &pipelineLayoutInfo, nullptr, &m_formatPipelineLayout));
    }

    // ================================================================================================================
    void CubemapFormatTransApp::InitFormatShaderModules()
    {
        // m_vsFormatShaderModule = CreateShaderModule("/hlsl/CubeMapFormat_vert.spv");
        // m_psFormatShaderModule = CreateShaderModule("/hlsl/CubeMapFormat_frag.spv");
        m_vsFormatShaderModule = CreateShaderModuleFromRam((uint32_t*)SharedLib::cubemapFormat_vertScript,
                                                           sizeof(SharedLib::cubemapFormat_vertScript));

        m_psFormatShaderModule = CreateShaderModuleFromRam((uint32_t*)SharedLib::cubemapFormat_fragScript,
                                                           sizeof(SharedLib::cubemapFormat_fragScript));

    }

    // ================================================================================================================
    void CubemapFormatTransApp::SetInputCubemapImg(
        VkImage    iCubemapImg, 
        VkExtent3D extent,
        VkImageSubresourceRange iSubres)
    {
        m_inputCubemap       = iCubemapImg;
        m_inputCubemapExtent = extent;
        m_inputSubres        = iSubres;
        assert(iSubres.levelCount == 1, "The mipmap level count must be 1.");
    }

    // ================================================================================================================
    void CubemapFormatTransApp::InitFormatImgsObjects()
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
            formatImgInfo.extent = m_inputCubemapExtent;
            formatImgInfo.mipLevels = 1;
            formatImgInfo.arrayLayers = 6;
            formatImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            // formatImgInfo.tiling = VK_IMAGE_TILING_LINEAR;
            formatImgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            formatImgInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            formatImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        }

        VkSamplerCreateInfo samplerInfo{};
        {
            samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter = VK_FILTER_LINEAR;
            samplerInfo.minFilter = VK_FILTER_LINEAR;
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT; // outside image bounds just use border color
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.minLod = -1000;
            samplerInfo.maxLod = 1000;
            samplerInfo.maxAnisotropy = 1.0f;
        }

        VK_CHECK(vmaCreateImage(*m_vkInfos.pAllocator,
            &formatImgInfo,
            &formatImgAllocInfo,
            &m_formatInputImages,
            &m_formatInputImagesAllocs,
            nullptr));

        VkImageViewCreateInfo formatImgViewInfo{};
        {
            formatImgViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            formatImgViewInfo.image = m_formatInputImages;
            formatImgViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            formatImgViewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
            formatImgViewInfo.subresourceRange = {};
            formatImgViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            formatImgViewInfo.subresourceRange.levelCount = 1;
            formatImgViewInfo.subresourceRange.layerCount = 6;
        }

        VK_CHECK(vkCreateImageView(m_vkInfos.device, &formatImgViewInfo, nullptr, &m_formatInputImagesViews));
        VK_CHECK(vkCreateSampler(m_vkInfos.device, &samplerInfo, nullptr, &m_formatInputImagesSamplers));

        // Create format input images descriptor write info
        {
            m_formatInputImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            m_formatInputImageInfo.imageView = m_formatInputImagesViews;
            m_formatInputImageInfo.sampler = m_formatInputImagesSamplers;
        }

        VkWriteDescriptorSet writeformatImgsDesSet{};
        {
            writeformatImgsDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeformatImgsDesSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writeformatImgsDesSet.dstBinding = 0;
            writeformatImgsDesSet.pImageInfo = &m_formatInputImageInfo;
            writeformatImgsDesSet.descriptorCount = 1;
        }
        m_descriptorSet0Writes.push_back(writeformatImgsDesSet);

        // Allocate output cubemap resources
        VmaAllocationCreateInfo outputImgAllocInfo{};
        {
            outputImgAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            outputImgAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        }

        VkImageCreateInfo outputImgInfo{};
        {
            outputImgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            outputImgInfo.imageType = VK_IMAGE_TYPE_2D;
            outputImgInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
            outputImgInfo.extent = m_inputCubemapExtent;
            outputImgInfo.mipLevels = 1;
            outputImgInfo.arrayLayers = 6;
            outputImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            // outputImgInfo.tiling = VK_IMAGE_TILING_LINEAR;
            outputImgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            outputImgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            outputImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        }

        VK_CHECK(vmaCreateImage(*m_vkInfos.pAllocator,
            &outputImgInfo,
            &outputImgAllocInfo,
            &m_outputCubemap,
            &m_outputCubemapAlloc,
            nullptr));

        VkImageViewCreateInfo outputImgViewInfo{};
        {
            outputImgViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            outputImgViewInfo.image = m_outputCubemap;
            outputImgViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            outputImgViewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
            outputImgViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            outputImgViewInfo.subresourceRange.levelCount = 1;
            outputImgViewInfo.subresourceRange.layerCount = 6;
        }

        VK_CHECK(vkCreateImageView(m_vkInfos.device, &outputImgViewInfo, nullptr, &m_outputCubemapImgView));
    }

    // ================================================================================================================
    void CubemapFormatTransApp::DestroyFormatImgsObjects()
    {
        vkDestroySampler(m_vkInfos.device, m_formatInputImagesSamplers, nullptr);
        vkDestroyImageView(m_vkInfos.device, m_formatInputImagesViews, nullptr);
        vmaDestroyImage(*m_vkInfos.pAllocator, m_formatInputImages, m_formatInputImagesAllocs);

        vkDestroyImageView(m_vkInfos.device, m_outputCubemapImgView, nullptr);
        vmaDestroyImage(*m_vkInfos.pAllocator, m_outputCubemap, m_outputCubemapAlloc);
    }

    // ================================================================================================================
    void CubemapFormatTransApp::InitWidthHeightBufferInfo()
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

        VK_CHECK(vmaCreateBuffer(*m_vkInfos.pAllocator,
            &screenBufInfo,
            &screenBufAllocInfo,
            &m_formatWidthHeightBuffer,
            &m_formatWidthHeightAlloc,
            nullptr));

        float widthHeight[2] = { m_inputCubemapExtent.width, m_inputCubemapExtent.height };
        CopyRamDataToGpuBuffer(widthHeight,
                               m_vkInfos.pAllocator,
                               m_formatWidthHeightBuffer,
                               m_formatWidthHeightAlloc,
                               sizeof(float) * 2);

        // Create format width height buffer descriptor set write info
        {
            m_formatWidthHeightDesBufferInfo.buffer = m_formatWidthHeightBuffer;
            m_formatWidthHeightDesBufferInfo.offset = 0;
            m_formatWidthHeightDesBufferInfo.range = sizeof(float) * 2;
        }

        VkWriteDescriptorSet writeUboBufDesSet{};
        {
            writeUboBufDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeUboBufDesSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writeUboBufDesSet.dstBinding = 1;
            writeUboBufDesSet.descriptorCount = 1;
            writeUboBufDesSet.pBufferInfo = &m_formatWidthHeightDesBufferInfo;
        }
        m_descriptorSet0Writes.push_back(writeUboBufDesSet);
    }

    // ================================================================================================================
    void CubemapFormatTransApp::DumpOutputCubemapToDisk(
        const std::string& outputCubemapPathName)
    {
        VkCommandBuffer tmpGfxCmdBuffer;
        VkCommandBufferAllocateInfo commandBufferAllocInfo{};
        {
            commandBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            commandBufferAllocInfo.commandPool = m_vkInfos.gfxCmdPool;
            commandBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            commandBufferAllocInfo.commandBufferCount = 1;
        }
        VK_CHECK(vkAllocateCommandBuffers(m_vkInfos.device, &commandBufferAllocInfo, &tmpGfxCmdBuffer));

        // Copy the buffer data to RAM and save that on the disk.
        float* pImgData = new float[4 * m_inputCubemapExtent.width * m_inputCubemapExtent.height * 6];

        VkImageSubresourceLayers outputCubemapLayersSubres{};
        {
            outputCubemapLayersSubres.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            outputCubemapLayersSubres.mipLevel = 0;
            outputCubemapLayersSubres.baseArrayLayer = 0;
            outputCubemapLayersSubres.layerCount = 6;
        }

        VkExtent3D outputCubemapExtent{};
        {
            outputCubemapExtent.width = m_inputCubemapExtent.width;
            outputCubemapExtent.height = m_inputCubemapExtent.width;
            outputCubemapExtent.depth = 1;
        }
        
        CopyImgToRam(tmpGfxCmdBuffer, 
                     m_vkInfos.device,
                     m_vkInfos.gfxQueue,
                     *m_vkInfos.pAllocator,
                     m_outputCubemap,
                     outputCubemapLayersSubres,
                     outputCubemapExtent,
                     4, sizeof(float), pImgData);

        // Convert data from 4 elements to 3 elements data
        float* pImgData3Ele = new float[3 * m_inputCubemapExtent.width * m_inputCubemapExtent.height * 6];
        Img4EleTo3Ele(pImgData, pImgData3Ele, m_inputCubemapExtent.width * m_inputCubemapExtent.height * 6);

        SaveImgHdr(outputCubemapPathName, m_inputCubemapExtent.width, m_inputCubemapExtent.height * 6, 3, pImgData3Ele);

        // Cleanup resources
        delete[] pImgData;
        delete[] pImgData3Ele;
    }

    // ================================================================================================================
    void CopyImgToRam(
        VkCommandBuffer          cmdBuffer,
        VkDevice                 device,
        VkQueue                  gfxQueue,
        VmaAllocator             allocator,
        VkImage                  srcImg,
        VkImageSubresourceLayers srcImgSubres,
        VkExtent3D               srcImgExtent,
        uint32_t                 srcImgChannelCnt,
        uint32_t                 srcImgChannelByteCnt,
        void*                    pDst)
    {
        // Copy the rendered images to a buffer.
        VkBuffer stagingBuffer;
        VmaAllocation stagingBufferAlloc;
        uint32_t bufferBytesCnt = srcImgChannelByteCnt * srcImgChannelCnt * srcImgExtent.width * srcImgExtent.height * srcImgSubres.layerCount;

        VmaAllocationCreateInfo stagingBufAllocInfo{};
        {
            stagingBufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            stagingBufAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                                        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        }

        VkBufferCreateInfo stgBufInfo{};
        {
            stgBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            stgBufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            stgBufInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            stgBufInfo.size = bufferBytesCnt;
        }

        VK_CHECK(vmaCreateBuffer(allocator, &stgBufInfo, &stagingBufAllocInfo, &stagingBuffer, &stagingBufferAlloc, nullptr));

        VkBufferImageCopy imgToBufferCopy{};
        {
            imgToBufferCopy.bufferRowLength = srcImgExtent.width;
            imgToBufferCopy.imageSubresource = srcImgSubres;
            imgToBufferCopy.imageExtent = srcImgExtent;
        }

        // Fill the command buffer
        VkCommandBufferBeginInfo beginInfo{};
        {
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        }
        VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo));

        vkCmdCopyImageToBuffer(cmdBuffer,
                               srcImg,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               stagingBuffer,
                               1, &imgToBufferCopy);

        // Submit all the works recorded before
        VK_CHECK(vkEndCommandBuffer(cmdBuffer));

        SharedLib::SubmitCmdBufferAndWait(device, gfxQueue, cmdBuffer);

        // Copy the data from buffer out.
        void* pBufferMapped;
        vmaMapMemory(allocator, stagingBufferAlloc, &pBufferMapped);
        memcpy(pDst, pBufferMapped, bufferBytesCnt);
        vmaUnmapMemory(allocator, stagingBufferAlloc);

        vkResetCommandBuffer(cmdBuffer, 0);
        vmaDestroyBuffer(allocator, stagingBuffer, stagingBufferAlloc);
    }

    // ================================================================================================================
    void Img4EleTo3Ele(
        float* pSrc,
        float* pDst,
        uint32_t pixCnt)
    {
        for (uint32_t i = 0; i < pixCnt; i++)
        {
            uint32_t ele4Idx0 = i * 4;
            uint32_t ele4Idx1 = i * 4 + 1;
            uint32_t ele4Idx2 = i * 4 + 2;

            uint32_t ele3Idx0 = i * 3;
            uint32_t ele3Idx1 = i * 3 + 1;
            uint32_t ele3Idx2 = i * 3 + 2;

            pDst[ele3Idx0] = pSrc[ele4Idx0];
            pDst[ele3Idx1] = pSrc[ele4Idx1];
            pDst[ele3Idx2] = pSrc[ele4Idx2];
        }
    }

    // ================================================================================================================
    void PrintDeviceImageCapbility(
        VkPhysicalDevice phyDevice)
    {
        VkImageFormatProperties imgFormatProperties{};
        vkGetPhysicalDeviceImageFormatProperties(phyDevice,
                                                 VK_FORMAT_R32G32B32_SFLOAT,
                                                 VK_IMAGE_TYPE_2D,
                                                 VK_IMAGE_TILING_OPTIMAL,
                                                 // VK_IMAGE_TILING_LINEAR,
                                                 VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,
                                                 VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, &imgFormatProperties);

        std::cout << "Max array layers:" << imgFormatProperties.maxArrayLayers << std::endl;
        std::cout << "Max array layers:" << imgFormatProperties.maxMipLevels << std::endl;
    }
}