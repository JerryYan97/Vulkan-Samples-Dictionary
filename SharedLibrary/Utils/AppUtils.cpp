#include "AppUtils.h"
#include "../../SharedLibrary/Utils/VulkanDbgUtils.h"
#include <fstream>

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
    CubemapFormatTransApp::CubemapFormatTransApp() :
        m_vsFormatShaderModule(VK_NULL_HANDLE),
        m_psFormatShaderModule(VK_NULL_HANDLE),
        m_formatPipelineDesSet0Layout(VK_NULL_HANDLE),
        m_formatPipelineLayout(VK_NULL_HANDLE),
        m_formatWidthHeightBuffer(VK_NULL_HANDLE),
        m_formatWidthHeightAlloc(VK_NULL_HANDLE),
        m_formatPipelineDescriptorSet0(VK_NULL_HANDLE),
        m_inputCubemap(VK_NULL_HANDLE),
        m_inputCubemapExtent(),
        m_outputCubemap(VK_NULL_HANDLE),
        m_outputCubemapImgView(VK_NULL_HANDLE),
        m_outputCubemapAlloc(VK_NULL_HANDLE)
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
        InitFormatPipelineDescriptorSet();
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
        m_vsFormatShaderModule = CreateShaderModule("/hlsl/CubeMapFormat_vert.spv");
        m_psFormatShaderModule = CreateShaderModule("/hlsl/CubeMapFormat_frag.spv");
    }

    // ================================================================================================================
    void CubemapFormatTransApp::InitFormatPipelineDescriptorSet()
    {
        // Create pipeline descirptor
        VkDescriptorSetAllocateInfo pipelineDesSet0AllocInfo{};
        {
            pipelineDesSet0AllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            pipelineDesSet0AllocInfo.descriptorPool = m_vkInfos.descriptorPool;
            pipelineDesSet0AllocInfo.pSetLayouts = &m_formatPipelineDesSet0Layout;
            pipelineDesSet0AllocInfo.descriptorSetCount = 1;
        }

        VK_CHECK(vkAllocateDescriptorSets(m_vkInfos.device,
            &pipelineDesSet0AllocInfo,
            &m_formatPipelineDescriptorSet0));

        // Link descriptors to the buffer and image
        VkDescriptorImageInfo formatImgInfo[6] = {};
        for (uint32_t i = 0; i < 6; i++)
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
        vkUpdateDescriptorSets(m_vkInfos.device, 2, writeFormatPipelineDescriptors, 0, NULL);
    }

    // ================================================================================================================
    void CubemapFormatTransApp::SetInputCubemapImg(
        VkImage    iCubemapImg, 
        VkExtent3D extent)
    {
        m_inputCubemap = iCubemapImg;
        m_inputCubemapExtent = extent;
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
            formatImgInfo.arrayLayers = 1;
            formatImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            formatImgInfo.tiling = VK_IMAGE_TILING_LINEAR;
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

        m_formatInputImages.resize(6);
        m_formatInputImagesViews.resize(6);
        m_formatInputImagesAllocs.resize(6);
        m_formatInputImagesSamplers.resize(6);
        for (uint32_t i = 0; i < 6; i++)
        {
            VK_CHECK(vmaCreateImage(*m_vkInfos.pAllocator,
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

            VK_CHECK(vkCreateImageView(m_vkInfos.device, &formatImgViewInfo, nullptr, &m_formatInputImagesViews[i]));
            VK_CHECK(vkCreateSampler(m_vkInfos.device, &samplerInfo, nullptr, &m_formatInputImagesSamplers[i]));
        }

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
            outputImgInfo.tiling = VK_IMAGE_TILING_LINEAR;
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
            outputImgViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
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
        for (uint32_t i = 0; i < m_formatInputImages.size(); i++)
        {
            vkDestroySampler(m_vkInfos.device, m_formatInputImagesSamplers[i], nullptr);
            vkDestroyImageView(m_vkInfos.device, m_formatInputImagesViews[i], nullptr);
            vmaDestroyImage(*m_vkInfos.pAllocator, m_formatInputImages[i], m_formatInputImagesAllocs[i]);
        }

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
    }

}