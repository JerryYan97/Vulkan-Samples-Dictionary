#include "GenIBL.h"
#include "../../SharedLibrary/Utils/VulkanDbgUtils.h"
#include "../../SharedLibrary/Camera/Camera.h"
#include "../../SharedLibrary/Event/Event.h"
#include "../../SharedLibrary/Utils/DiskOpsUtils.h"
#include "../../SharedLibrary/Utils/CmdBufUtils.h"
#include <cassert>
#include <cmath>

#include "vk_mem_alloc.h"

// ================================================================================================================
GenIBL::GenIBL() :
    Application(),
    m_hdrCubeMapImage(VK_NULL_HANDLE),
    m_hdrCubeMapView(VK_NULL_HANDLE),
    m_hdrCubeMapSampler(VK_NULL_HANDLE),
    m_hdrCubeMapAlloc(VK_NULL_HANDLE),
    m_hdrCubeMapInfo(),
    m_diffuseIrradiancePipeline(),
    m_preFilterEnvMapPipeline(),
    m_envBrdfPipeline(),
    m_uboCameraScreenBuffer(VK_NULL_HANDLE),
    m_uboCameraScreenAlloc(VK_NULL_HANDLE),
    m_diffIrrPreFilterEnvMapDesSet0(VK_NULL_HANDLE),
    m_diffIrrPreFilterEnvMapDesSet0Layout(VK_NULL_HANDLE),
    m_diffuseIrradianceVsShaderModule(VK_NULL_HANDLE),
    m_diffuseIrradiancePsShaderModule(VK_NULL_HANDLE),
    m_diffuseIrradiancePipelineLayout(VK_NULL_HANDLE),
    m_diffuseIrradianceCubemap(VK_NULL_HANDLE),
    m_diffuseIrradianceCubemapAlloc(VK_NULL_HANDLE),
    m_diffuseIrradianceCubemapImageView(VK_NULL_HANDLE),
    m_preFilterEnvMapVsShaderModule(VK_NULL_HANDLE),
    m_preFilterEnvMapPsShaderModule(VK_NULL_HANDLE),
    m_preFilterEnvMapPipelineLayout(VK_NULL_HANDLE),
    m_preFilterEnvMapCubemap(VK_NULL_HANDLE),
    m_preFilterEnvMapCubemapAlloc(VK_NULL_HANDLE)
{
    memset(m_screenCameraData, 0, sizeof(m_screenCameraData));
}

// ================================================================================================================
GenIBL::~GenIBL()
{
    vkDeviceWaitIdle(m_device);

    vkDestroyDescriptorSetLayout(m_device, m_diffIrrPreFilterEnvMapDesSet0Layout, nullptr);

    DestroyCameraScreenUbo();
    DestroyInputCubemapRenderObjs();
    DestroyDiffuseIrradiancePipelineResourses();
    DestroyPrefilterEnvMapPipelineResourses();
    DestroyEnvBrdfPipelineResources();
}

// ================================================================================================================
void GenIBL::DestroyInputCubemapRenderObjs()
{
    for (uint32_t i = 1; i < m_pHdrCubemapMips.size(); i++)
    {
        delete m_pHdrCubemapMips[i];
    }

    vmaDestroyImage(*m_pAllocator, m_hdrCubeMapImage, m_hdrCubeMapAlloc);
    vkDestroyImageView(m_device, m_hdrCubeMapView, nullptr);
    vkDestroySampler(m_device, m_hdrCubeMapSampler, nullptr);
}

// ================================================================================================================
// Clamp the high radiance so that diffuse irradiance sampling is happy.
void DataPreprosess(
    float* pData,
    uint32_t width,
    uint32_t height)
{
    for (uint32_t i = 0; i < width * height * 3; i++)
    {
        if (pData[i] > 50.f)
        {
            pData[i] = 50.f;
        }
    }
}

// ================================================================================================================
void GenIBL::ReadInCubemap(
    const std::string& namePath)
{
    int nrComponents, width, height;
    m_hdrCubeMapInfo.pData = SharedLib::ReadImg(namePath.c_str(), nrComponents, width, height);

    m_hdrCubeMapInfo.width = (uint32_t)width;
    m_hdrCubeMapInfo.height = (uint32_t)height;

    DataPreprosess(m_hdrCubeMapInfo.pData, m_hdrCubeMapInfo.width, m_hdrCubeMapInfo.height);
}

// ================================================================================================================
void GenIBL::InitInputCubemapObjects()
{
    assert(m_hdrCubeMapInfo.pData != nullptr);

    VmaAllocationCreateInfo hdrAllocInfo{};
    {
        hdrAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        hdrAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
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
        cubeMapImgInfo.format = VK_FORMAT_R32G32B32_SFLOAT;
        cubeMapImgInfo.extent = extent;
        cubeMapImgInfo.mipLevels = 10;
        cubeMapImgInfo.arrayLayers = 6;
        cubeMapImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        cubeMapImgInfo.tiling = VK_IMAGE_TILING_LINEAR;
        cubeMapImgInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
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
        info.subresourceRange.levelCount = 10;
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
        sampler_info.minLod = 0.f;
        sampler_info.maxLod = 10.f;
        sampler_info.maxAnisotropy = 1.0f;
    }
    VK_CHECK(vkCreateSampler(m_device, &sampler_info, nullptr, &m_hdrCubeMapSampler));
}

// ================================================================================================================
void GenIBL::InitCameraScreenUbo()
{
    // Allocate the GPU buffer
    // The alignment of a vec3 is 4 floats and the element alignment of a struct is the largest element alignment.
    VkBufferCreateInfo bufferInfo{};
    {
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = CameraScreenBufferSizeInBytes;
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo bufferAllocInfo{};
    {
        bufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        bufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
                                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    vmaCreateBuffer(*m_pAllocator,
                    &bufferInfo,
                    &bufferAllocInfo,
                    &m_uboCameraScreenBuffer,
                    &m_uboCameraScreenAlloc,
                    nullptr);

    // Prepare data
    // Front, back, top, bottom, right, left.
    float views[6 * 4] = 
    {
         1.f,  0.f,  0.f, 0.f,
        -1.f,  0.f,  0.f, 0.f,
         0.f,  1.f,  0.f, 0.f,
         0.f, -1.f,  0.f, 0.f,
         0.f,  0.f,  1.f, 0.f,
         0.f,  0.f, -1.f, 0.f
    };

    float rights[6 * 4] =
    {
         0.f,  0.f,  1.f, 0.f,
         0.f,  0.f, -1.f, 0.f,
         0.f,  0.f,  1.f, 0.f,
         0.f,  0.f,  1.f, 0.f,
        -1.f,  0.f,  0.f, 0.f,
         1.f,  0.f,  0.f, 0.f
    };

    float ups[6 * 4] =
    {
         0.f,  1.f,  0.f, 0.f,
         0.f,  1.f,  0.f, 0.f,
        -1.f,  0.f,  0.f, 0.f,
         1.f,  0.f,  0.f, 0.f,
         0.f,  1.f,  0.f, 0.f,
         0.f,  1.f,  0.f, 0.f
    };

    float near = 1.f;
    float nearWidthHeight[2] = {2.f, 2.f};
    float viewportWidthHeight[2] = { m_hdrCubeMapInfo.width, m_hdrCubeMapInfo.width };

    memcpy(m_screenCameraData, views, sizeof(views));
    memcpy(&m_screenCameraData[24], rights, sizeof(rights));
    memcpy(&m_screenCameraData[48], ups, sizeof(ups));
    m_screenCameraData[72] = near;
    memcpy(&m_screenCameraData[73], nearWidthHeight, sizeof(nearWidthHeight));
    memcpy(&m_screenCameraData[76], viewportWidthHeight, sizeof(viewportWidthHeight));

    // Send data to the GPU buffer
    CopyRamDataToGpuBuffer(m_screenCameraData,
                           m_uboCameraScreenBuffer,
                           m_uboCameraScreenAlloc,
                           sizeof(m_screenCameraData));
}

// ================================================================================================================
void GenIBL::DestroyCameraScreenUbo()
{
    vmaDestroyBuffer(*m_pAllocator, m_uboCameraScreenBuffer, m_uboCameraScreenAlloc);
}

// ================================================================================================================
void GenIBL::InitDiffIrrPreFilterEnvMapDescriptorSetLayout()
{
    // Create pipeline binding and descriptor objects for the camera parameters
    VkDescriptorSetLayoutBinding cameraScreenInfoUboBinding{};
    {
        cameraScreenInfoUboBinding.binding = 1;
        cameraScreenInfoUboBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        cameraScreenInfoUboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        cameraScreenInfoUboBinding.descriptorCount = 1;
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
    VkDescriptorSetLayoutBinding pipelineDesSet0LayoutBindings[2] = { hdriSamplerBinding, cameraScreenInfoUboBinding };

    VkDescriptorSetLayoutCreateInfo pipelineDesSet0LayoutInfo{};
    {
        pipelineDesSet0LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        pipelineDesSet0LayoutInfo.bindingCount = 2;
        pipelineDesSet0LayoutInfo.pBindings = pipelineDesSet0LayoutBindings;
    }

    VK_CHECK(vkCreateDescriptorSetLayout(m_device,
                                         &pipelineDesSet0LayoutInfo,
                                         nullptr,
                                         &m_diffIrrPreFilterEnvMapDesSet0Layout));
}

// ================================================================================================================
void GenIBL::InitDiffIrrPreFilterEnvMapDescriptorSets()
{
    // Create pipeline descirptor
    VkDescriptorSetAllocateInfo pipelineDesSet0AllocInfo{};
    {
        pipelineDesSet0AllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        pipelineDesSet0AllocInfo.descriptorPool = m_descriptorPool;
        pipelineDesSet0AllocInfo.pSetLayouts = &m_diffIrrPreFilterEnvMapDesSet0Layout;
        pipelineDesSet0AllocInfo.descriptorSetCount = 1;
    }

    VK_CHECK(vkAllocateDescriptorSets(m_device,
        &pipelineDesSet0AllocInfo,
        &m_diffIrrPreFilterEnvMapDesSet0));

    // Link descriptors to the buffer and image
    VkDescriptorImageInfo hdriDesImgInfo{};
    {
        hdriDesImgInfo.imageView = m_hdrCubeMapView;
        hdriDesImgInfo.sampler = m_hdrCubeMapSampler;
        hdriDesImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    VkDescriptorBufferInfo cameraScreenBufferInfo{};
    {
        cameraScreenBufferInfo.buffer = m_uboCameraScreenBuffer;
        cameraScreenBufferInfo.offset = 0;
        cameraScreenBufferInfo.range = CameraScreenBufferSizeInBytes;
    }

    VkWriteDescriptorSet writeUboBufDesSet{};
    {
        writeUboBufDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeUboBufDesSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeUboBufDesSet.dstSet = m_diffIrrPreFilterEnvMapDesSet0;
        writeUboBufDesSet.dstBinding = 1;
        writeUboBufDesSet.descriptorCount = 1;
        writeUboBufDesSet.pBufferInfo = &cameraScreenBufferInfo;
    }

    VkWriteDescriptorSet writeHdrDesSet{};
    {
        writeHdrDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeHdrDesSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeHdrDesSet.dstSet = m_diffIrrPreFilterEnvMapDesSet0;
        writeHdrDesSet.dstBinding = 0;
        writeHdrDesSet.pImageInfo = &hdriDesImgInfo;
        writeHdrDesSet.descriptorCount = 1;
    }

    // Linking pipeline descriptors: cubemap and scene buffer descriptors to their GPU memory and info.
    VkWriteDescriptorSet writeSkyboxPipelineDescriptors[2] = { writeHdrDesSet, writeUboBufDesSet };
    vkUpdateDescriptorSets(m_device, 2, writeSkyboxPipelineDescriptors, 0, NULL);
}

// ================================================================================================================
void GenIBL::AppInit()
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

    InitInputCubemapObjects();
    InitCameraScreenUbo();

    // Shared pipeline resources
    InitDiffIrrPreFilterEnvMapDescriptorSetLayout();
    InitDiffIrrPreFilterEnvMapDescriptorSets();

    // Pipeline and resources for the diffuse irradiance map gen.
    InitDiffuseIrradianceOutputObjects(); // The diffuse irradiance map itself.
    InitDiffuseIrradianceShaderModules();
    InitDiffuseIrradiancePipelineLayout();
    InitDiffuseIrradiancePipeline();

    // Pipeline and resources for the prefilter environment map gen.
    InitPrefilterEnvMapOutputObjects();
    InitPrefilterEnvMapShaderModules();
    InitPrefilterEnvMapPipelineLayout();
    InitPrefilterEnvMapPipeline();

    // Pipeline and resources for the environment brdf map gen.
    InitEnvBrdfOutputObjects();
    InitEnvBrdfShaderModules();
    InitEnvBrdfPipelineLayout();
    InitEnvBrdfPipeline();
}

// ================================================================================================================
// Only for the rectangle pic now. 3 channels per color element.
void GenHalfMipmapLinearMean(
    float* pSrc,
    uint32_t srcDim,
    float* pDst)
{
    uint32_t faceWidth = srcDim / 2;
    uint32_t faceHeight = srcDim / 2;
    for (uint32_t row = 0; row < faceHeight; row++)
    {
        for (uint32_t col = 0; col < faceWidth; col++)
        {
            uint32_t dstPixelId = row * faceHeight + col;

            uint32_t srcRowId = 2 * row;
            uint32_t srcColId = 2 * col;
            uint32_t srcPixelId0 = srcRowId * srcDim + srcColId;
            uint32_t srcPixelId1 = srcRowId * srcDim + srcColId + 1;
            uint32_t srcPixelId2 = (srcRowId + 1) * srcDim + srcColId;
            uint32_t srcPixelId3 = (srcRowId + 1) * srcDim + srcColId + 1;

            pDst[3 * dstPixelId] = (pSrc[srcPixelId0 * 3] +
                                    pSrc[srcPixelId1 * 3] +
                                    pSrc[srcPixelId2 * 3] +
                                    pSrc[srcPixelId3 * 3]) / 4.f;

            pDst[3 * dstPixelId + 1] = (pSrc[srcPixelId0 * 3 + 1] +
                                        pSrc[srcPixelId1 * 3 + 1] +
                                        pSrc[srcPixelId2 * 3 + 1] +
                                        pSrc[srcPixelId3 * 3 + 1]) / 4.f;

            pDst[3 * dstPixelId + 2] = (pSrc[srcPixelId0 * 3 + 2] +
                                        pSrc[srcPixelId1 * 3 + 2] +
                                        pSrc[srcPixelId2 * 3 + 2] +
                                        pSrc[srcPixelId3 * 3 + 2]) / 4.f;
        }
    }
}

// ================================================================================================================
// Assume vertical format cubemap
void GenHalfCubemapMipmapLinearMean(
    float* pSrc,
    uint32_t srcDim,
    float* pDst)
{
    uint32_t dstDim = srcDim / 2;
    for (uint32_t face = 0; face < 6; face++)
    {
        uint32_t faceSrcPixelStartId = face * (srcDim * srcDim);
        float* pFaceSrc = &pSrc[3 * faceSrcPixelStartId];

        uint32_t faceDstPixelStartId = face * (dstDim * dstDim);
        float* pFaceDst = &pDst[3 * faceDstPixelStartId];

        GenHalfMipmapLinearMean(pFaceSrc, srcDim, pFaceDst);
    }
}

// ================================================================================================================
void GenIBL::CmdGenInputCubemapMipMaps(
    VkCommandBuffer cmdBuffer)
{
    constexpr bool DbgDump = false;

    // Prepare 9 mipmap chain data.
    uint32_t mipMapNum = 9;

    m_pHdrCubemapMips.resize(mipMapNum + 1);
    m_pHdrCubemapMips[0] = m_hdrCubeMapInfo.pData;

    for (uint32_t mipLevel = 0; mipLevel < mipMapNum; mipLevel++)
    {
        uint32_t dstDivFactor = 2 << mipLevel;
        uint32_t mipPixelCnt = (m_hdrCubeMapInfo.width / dstDivFactor) * (m_hdrCubeMapInfo.height / dstDivFactor);
        float* pDstMip = new float[3 * mipPixelCnt];

        m_pHdrCubemapMips[mipLevel + 1] = pDstMip;

        uint32_t srcDivFactor = 1 << mipLevel;
        GenHalfCubemapMipmapLinearMean(m_pHdrCubemapMips[mipLevel],
                                       m_hdrCubeMapInfo.width / srcDivFactor,
                                       pDstMip);

        if (DbgDump)
        {
            std::string outputPathName = SOURCE_PATH;
            outputPathName += ("/mip" + std::to_string(mipLevel + 1) + ".hdr");
            SharedLib::SaveImgHdr(outputPathName,
                                  m_hdrCubeMapInfo.width / dstDivFactor,
                                  m_hdrCubeMapInfo.height / dstDivFactor,
                                  3, pDstMip);
        }

        uint32_t mipBytesCnt = 3 * sizeof(float) *
                               (m_hdrCubeMapInfo.width / dstDivFactor) *
                               (m_hdrCubeMapInfo.height / dstDivFactor);

        VkImageSubresourceRange mipSubResRangeForLayoutTrans{};
        {
            mipSubResRangeForLayoutTrans.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            mipSubResRangeForLayoutTrans.baseArrayLayer = 0;
            mipSubResRangeForLayoutTrans.layerCount = 6;
            mipSubResRangeForLayoutTrans.baseMipLevel = mipLevel + 1;
            mipSubResRangeForLayoutTrans.levelCount = 1;
        }

        VkBufferImageCopy mipCopy{};
        {
            VkExtent3D extent{};
            {
                extent.width = m_hdrCubeMapInfo.width / dstDivFactor;
                extent.height = m_hdrCubeMapInfo.width / dstDivFactor;
                extent.depth = 1;
            }

            mipCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            mipCopy.imageSubresource.mipLevel = mipLevel + 1;
            mipCopy.imageSubresource.baseArrayLayer = 0;
            mipCopy.imageSubresource.layerCount = 6;

            mipCopy.imageExtent = extent;
        }

        SharedLib::SendImgDataToGpu(cmdBuffer,
                                    m_device,
                                    m_graphicsQueue,
                                    m_pHdrCubemapMips[mipLevel + 1],
                                    mipBytesCnt,
                                    m_hdrCubeMapImage,
                                    mipSubResRangeForLayoutTrans,
                                    VK_IMAGE_LAYOUT_UNDEFINED,
                                    mipCopy,
                                    *m_pAllocator);
    }
}