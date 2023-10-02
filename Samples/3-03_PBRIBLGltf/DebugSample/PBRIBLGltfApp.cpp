#include "PBRIBLGltfApp.h"
#include <glfw3.h>
#include "../../../SharedLibrary/Utils/VulkanDbgUtils.h"
#include "../../../SharedLibrary/Camera/Camera.h"
#include "../../../SharedLibrary/Event/Event.h"
#include "../../../SharedLibrary/Utils/StrPathUtils.h"
#include "../../../SharedLibrary/Utils/DiskOpsUtils.h"

#define TINYGLTF_IMPLEMENTATION
// #define STB_IMAGE_IMPLEMENTATION
// #define STB_IMAGE_WRITE_IMPLEMENTATION
// #define TINYGLTF_NOEXCEPTION // optional. disable exception handling.
#include "tiny_gltf.h"

#include "vk_mem_alloc.h"

static bool g_isDown = false;

static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS)
    {
        g_isDown = true;
    }

    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_RELEASE)
    {
        g_isDown = false;
    }
}

// ================================================================================================================
PBRIBLGltfApp::PBRIBLGltfApp() : 
    GlfwApplication(),
    m_hdrCubeMapImage(VK_NULL_HANDLE),
    m_hdrCubeMapView(VK_NULL_HANDLE),
    m_hdrSampler(VK_NULL_HANDLE),
    m_hdrCubeMapAlloc(VK_NULL_HANDLE),
    m_vsSkyboxShaderModule(VK_NULL_HANDLE),
    m_psSkyboxShaderModule(VK_NULL_HANDLE),
    m_skyboxPipelineDesSet0Layout(VK_NULL_HANDLE),
    m_skyboxPipelineLayout(VK_NULL_HANDLE),
    m_skyboxPipeline(),
    m_vsIblShaderModule(VK_NULL_HANDLE),
    m_psIblShaderModule(VK_NULL_HANDLE),
    m_iblPipelineDesSet0Layout(VK_NULL_HANDLE),
    m_iblPipelineLayout(VK_NULL_HANDLE),
    m_iblPipeline(),
    m_diffuseIrradianceCubemap(VK_NULL_HANDLE),
    m_diffuseIrradianceCubemapImgView(VK_NULL_HANDLE),
    m_diffuseIrradianceCubemapSampler(VK_NULL_HANDLE),
    m_diffuseIrradianceCubemapAlloc(VK_NULL_HANDLE),
    m_prefilterEnvCubemap(VK_NULL_HANDLE),
    m_prefilterEnvCubemapView(VK_NULL_HANDLE),
    m_prefilterEnvCubemapSampler(VK_NULL_HANDLE),
    m_prefilterEnvCubemapAlloc(VK_NULL_HANDLE),
    m_envBrdfImg(VK_NULL_HANDLE),
    m_envBrdfImgView(VK_NULL_HANDLE),
    m_envBrdfImgSampler(VK_NULL_HANDLE),
    m_envBrdfImgAlloc(VK_NULL_HANDLE),
    m_hdrImgCubemap(),
    m_diffuseIrradianceCubemapImgInfo(),
    m_envBrdfImgInfo()
{
    m_pCamera = new SharedLib::Camera();
}

// ================================================================================================================
PBRIBLGltfApp::~PBRIBLGltfApp()
{
    vkDeviceWaitIdle(m_device);
    delete m_pCamera;

    DestroyVpMatBuffer();
    DestroyHdrRenderObjs();
    DestroyCameraUboObjects();

    DestroySkyboxPipelineRes();
    DestroyIblPipelineRes();
}

// ================================================================================================================
void PBRIBLGltfApp::DestroyHdrRenderObjs()
{
    DestroyModelInfo();

    delete m_diffuseIrradianceCubemapImgInfo.pData;
    for (auto itr : m_prefilterEnvCubemapImgsInfo)
    {
        delete itr.pData;
    }
    delete m_envBrdfImgInfo.pData;

    vmaDestroyImage(*m_pAllocator, m_diffuseIrradianceCubemap, m_diffuseIrradianceCubemapAlloc);
    vkDestroyImageView(m_device, m_diffuseIrradianceCubemapImgView, nullptr);
    vkDestroySampler(m_device, m_diffuseIrradianceCubemapSampler, nullptr);

    vmaDestroyImage(*m_pAllocator, m_prefilterEnvCubemap, m_prefilterEnvCubemapAlloc);
    vkDestroyImageView(m_device, m_prefilterEnvCubemapView, nullptr);
    vkDestroySampler(m_device, m_prefilterEnvCubemapSampler, nullptr);

    vmaDestroyImage(*m_pAllocator, m_envBrdfImg, m_envBrdfImgAlloc);
    vkDestroyImageView(m_device, m_envBrdfImgView, nullptr);
    vkDestroySampler(m_device, m_envBrdfImgSampler, nullptr);

    vmaDestroyImage(*m_pAllocator, m_hdrCubeMapImage, m_hdrCubeMapAlloc);
    vkDestroyImageView(m_device, m_hdrCubeMapView, nullptr);
    vkDestroySampler(m_device, m_hdrSampler, nullptr);
}

// ================================================================================================================
void PBRIBLGltfApp::DestroyCameraUboObjects()
{
    for (uint32_t i = 0; i < SharedLib::MAX_FRAMES_IN_FLIGHT; i++)
    {
        vmaDestroyBuffer(*m_pAllocator, m_cameraParaBuffers[i], m_cameraParaBufferAllocs[i]);
    }
}

// ================================================================================================================
VkDeviceSize PBRIBLGltfApp::GetHdrByteNum()
{
    return 3 * sizeof(float) * m_hdrImgCubemap.pixWidth * m_hdrImgCubemap.pixHeight;
}

// ================================================================================================================
void PBRIBLGltfApp::GetCameraData(
    float* pBuffer)
{
    
}

// ================================================================================================================
void PBRIBLGltfApp::SendCameraDataToBuffer(
    uint32_t i)
{
    float cameraData[16] = {};
    m_pCamera->GetView(cameraData);
    m_pCamera->GetRight(&cameraData[4]);
    m_pCamera->GetUp(&cameraData[8]);

    m_pCamera->GetNearPlane(cameraData[12], cameraData[13], cameraData[11]);

    float vpMatData[16] = {};
    float tmpViewMat[16] = {};
    float tmpPersMat[16] = {};
    m_pCamera->GenViewPerspectiveMatrices(tmpViewMat, tmpPersMat, vpMatData);
    SharedLib::MatTranspose(vpMatData, 4);

    VkExtent2D swapchainImgExtent = GetSwapchainImageExtent();
    cameraData[14] = swapchainImgExtent.width;
    cameraData[15] = swapchainImgExtent.height;

    CopyRamDataToGpuBuffer(cameraData, m_cameraParaBuffers[i], m_cameraParaBufferAllocs[i], sizeof(cameraData));
    CopyRamDataToGpuBuffer(vpMatData, m_vpMatUboBuffer[i], m_vpMatUboAlloc[i], sizeof(vpMatData));
}

// ================================================================================================================
void PBRIBLGltfApp::UpdateCameraAndGpuBuffer()
{
    SharedLib::HEvent midMouseDownEvent = CreateMiddleMouseEvent(g_isDown);
    m_pCamera->OnEvent(midMouseDownEvent);
    SendCameraDataToBuffer(m_currentFrame);
}

// ================================================================================================================
void PBRIBLGltfApp::GetCameraPos(
    float* pOut)
{
    m_pCamera->GetPos(pOut);
}

// ================================================================================================================
void PBRIBLGltfApp::InitHdrRenderObjects()
{
    // Load the HDRI image into RAM
    std::string hdriFilePath = SOURCE_PATH;
    hdriFilePath += "/../data/";

    // Read in and init background cubemap
    {
        std::string cubemapPathName = hdriFilePath + "iblOutput/background_cubemap.hdr";

        int width, height, nrComponents;
        m_hdrImgCubemap.pData = stbi_loadf(cubemapPathName.c_str(), &width, &height, &nrComponents, 0);

        m_hdrImgCubemap.pixWidth = (uint32_t)width;
        m_hdrImgCubemap.pixHeight = (uint32_t)height;

        VmaAllocationCreateInfo hdrAllocInfo{};
        {
            hdrAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            hdrAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        }

        VkExtent3D extent{};
        {
            // extent.width = m_hdrImgWidth / 6;
            // extent.height = m_hdrImgHeight;
            extent.width = m_hdrImgCubemap.pixWidth;
            extent.height = m_hdrImgCubemap.pixWidth;
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
            sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler_info.minLod = -1000;
            sampler_info.maxLod = 1000;
            sampler_info.maxAnisotropy = 1.0f;
        }
        VK_CHECK(vkCreateSampler(m_device, &sampler_info, nullptr, &m_hdrSampler));
    }
    
    // Read in and init diffuse irradiance cubemap
    {
        std::string diffIrradiancePathName = hdriFilePath + "iblOutput/diffuse_irradiance_cubemap.hdr";
        int width, height, nrComponents;
        m_diffuseIrradianceCubemapImgInfo.pData = stbi_loadf(diffIrradiancePathName.c_str(),
                                                             &width, &height, &nrComponents, 0);

        m_diffuseIrradianceCubemapImgInfo.pixWidth = (uint32_t)width;
        m_diffuseIrradianceCubemapImgInfo.pixHeight = (uint32_t)height;

        VmaAllocationCreateInfo diffIrrAllocInfo{};
        {
            diffIrrAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            diffIrrAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        }

        VkExtent3D extent{};
        {
            extent.width = m_diffuseIrradianceCubemapImgInfo.pixWidth;
            extent.height = m_diffuseIrradianceCubemapImgInfo.pixWidth;
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
            &diffIrrAllocInfo,
            &m_diffuseIrradianceCubemap,
            &m_diffuseIrradianceCubemapAlloc,
            nullptr));

        VkImageViewCreateInfo info{};
        {
            info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            info.image = m_diffuseIrradianceCubemap;
            info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
            info.format = VK_FORMAT_R32G32B32_SFLOAT;
            info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            info.subresourceRange.levelCount = 1;
            info.subresourceRange.layerCount = 6;
        }
        VK_CHECK(vkCreateImageView(m_device, &info, nullptr, &m_diffuseIrradianceCubemapImgView));

        VkSamplerCreateInfo sampler_info{};
        {
            sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            sampler_info.magFilter = VK_FILTER_LINEAR;
            sampler_info.minFilter = VK_FILTER_LINEAR;
            sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler_info.minLod = -1000;
            sampler_info.maxLod = 1000;
            sampler_info.maxAnisotropy = 1.0f;
        }
        VK_CHECK(vkCreateSampler(m_device, &sampler_info, nullptr, &m_diffuseIrradianceCubemapSampler));
    }

    // Read in and init prefilter environment cubemap
    {
        std::string prefilterEnvPath = hdriFilePath + "iblOutput/prefilterEnvMaps/";
        std::vector<std::string> mipImgNames;
        SharedLib::GetAllFileNames(prefilterEnvPath, mipImgNames);

        const uint32_t mipCnts = mipImgNames.size();

        m_prefilterEnvCubemapImgsInfo.resize(mipCnts);

        for (uint32_t i = 0; i < mipCnts; i++)
        {
            int width, height, nrComponents;
            std::string prefilterEnvMipImgPathName = hdriFilePath +
                                                     "iblOutput/prefilterEnvMaps/prefilterMip" +
                                                     std::to_string(i) + ".hdr";

            m_prefilterEnvCubemapImgsInfo[i].pData = stbi_loadf(prefilterEnvMipImgPathName.c_str(),
                                                                &width, &height, &nrComponents, 0);
            m_prefilterEnvCubemapImgsInfo[i].pixWidth = width;
            m_prefilterEnvCubemapImgsInfo[i].pixHeight = height;
        }

        VmaAllocationCreateInfo prefilterEnvCubemapAllocInfo{};
        {
            prefilterEnvCubemapAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            prefilterEnvCubemapAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        }

        VkExtent3D extent{};
        {
            extent.width = m_prefilterEnvCubemapImgsInfo[0].pixWidth;
            extent.height = m_prefilterEnvCubemapImgsInfo[0].pixWidth;
            extent.depth = 1;
        }

        VkImageCreateInfo cubeMapImgInfo{};
        {
            cubeMapImgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            cubeMapImgInfo.imageType = VK_IMAGE_TYPE_2D;
            cubeMapImgInfo.format = VK_FORMAT_R32G32B32_SFLOAT;
            cubeMapImgInfo.extent = extent;
            cubeMapImgInfo.mipLevels = mipCnts;
            cubeMapImgInfo.arrayLayers = 6;
            cubeMapImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            cubeMapImgInfo.tiling = VK_IMAGE_TILING_LINEAR;
            cubeMapImgInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            cubeMapImgInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
            cubeMapImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        }

        VK_CHECK(vmaCreateImage(*m_pAllocator,
            &cubeMapImgInfo,
            &prefilterEnvCubemapAllocInfo,
            &m_prefilterEnvCubemap,
            &m_prefilterEnvCubemapAlloc,
            nullptr));

        VkImageViewCreateInfo info{};
        {
            info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            info.image = m_prefilterEnvCubemap;
            info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
            info.format = VK_FORMAT_R32G32B32_SFLOAT;
            info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            info.subresourceRange.levelCount = mipCnts;
            info.subresourceRange.layerCount = 6;
        }
        VK_CHECK(vkCreateImageView(m_device, &info, nullptr, &m_prefilterEnvCubemapView));

        VkSamplerCreateInfo sampler_info{};
        {
            sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            sampler_info.magFilter = VK_FILTER_LINEAR;
            sampler_info.minFilter = VK_FILTER_LINEAR;
            sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler_info.minLod = -1000;
            sampler_info.maxLod = 1000;
            sampler_info.maxAnisotropy = 1.0f;
        }
        VK_CHECK(vkCreateSampler(m_device, &sampler_info, nullptr, &m_prefilterEnvCubemapSampler));
    }

    // Read in and init environment brdf map
    {
        std::string envBrdfMapPathName = hdriFilePath + "iblOutput/envBrdf.hdr";
        int width, height, nrComponents;
        m_envBrdfImgInfo.pData = stbi_loadf(envBrdfMapPathName.c_str(), &width, &height, &nrComponents, 0);
        m_envBrdfImgInfo.pixWidth = width;
        m_envBrdfImgInfo.pixHeight = height;

        VmaAllocationCreateInfo envBrdfMapAllocInfo{};
        {
            envBrdfMapAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            envBrdfMapAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        }

        VkExtent3D extent{};
        {
            extent.width = m_envBrdfImgInfo.pixWidth;
            extent.height = m_envBrdfImgInfo.pixHeight;
            extent.depth = 1;
        }

        VkImageCreateInfo envBrdfImgInfo{};
        {
            envBrdfImgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            envBrdfImgInfo.imageType = VK_IMAGE_TYPE_2D;
            envBrdfImgInfo.format = VK_FORMAT_R32G32B32_SFLOAT;
            envBrdfImgInfo.extent = extent;
            envBrdfImgInfo.mipLevels = 1;
            envBrdfImgInfo.arrayLayers = 1;
            envBrdfImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            envBrdfImgInfo.tiling = VK_IMAGE_TILING_LINEAR;
            envBrdfImgInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            envBrdfImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        }

        VK_CHECK(vmaCreateImage(*m_pAllocator,
            &envBrdfImgInfo,
            &envBrdfMapAllocInfo,
            &m_envBrdfImg,
            &m_envBrdfImgAlloc,
            nullptr));

        VkImageViewCreateInfo info{};
        {
            info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            info.image = m_envBrdfImg;
            info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            info.format = VK_FORMAT_R32G32B32_SFLOAT;
            info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            info.subresourceRange.levelCount = 1;
            info.subresourceRange.layerCount = 1;
        }
        VK_CHECK(vkCreateImageView(m_device, &info, nullptr, &m_envBrdfImgView));

        VkSamplerCreateInfo sampler_info{};
        {
            sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            sampler_info.magFilter = VK_FILTER_LINEAR;
            sampler_info.minFilter = VK_FILTER_LINEAR;
            sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler_info.minLod = -1000;
            sampler_info.maxLod = 1000;
            sampler_info.maxAnisotropy = 1.0f;
        }
        VK_CHECK(vkCreateSampler(m_device, &sampler_info, nullptr, &m_envBrdfImgSampler));
    }
}

// ================================================================================================================
void PBRIBLGltfApp::InitCameraUboObjects()
{
    // The alignment of a vec3 is 4 floats and the element alignment of a struct is the largest element alignment,
    // which is also the 4 float. Therefore, we need 16 floats as the buffer to store the Camera's parameters.
    VkBufferCreateInfo bufferInfo{};
    {
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = 16 * sizeof(float);
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo bufferAllocInfo{};
    {
        bufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        bufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    m_cameraParaBuffers.resize(SharedLib::MAX_FRAMES_IN_FLIGHT);
    m_cameraParaBufferAllocs.resize(SharedLib::MAX_FRAMES_IN_FLIGHT);

    for (uint32_t i = 0; i < SharedLib::MAX_FRAMES_IN_FLIGHT; i++)
    {
        vmaCreateBuffer(*m_pAllocator,
                        &bufferInfo,
                        &bufferAllocInfo,
                        &m_cameraParaBuffers[i],
                        &m_cameraParaBufferAllocs[i],
                        nullptr);
    }
}

// ================================================================================================================
// TODO: I may need to put most the content in this function to CreateXXXX(...) in the parent class.
void PBRIBLGltfApp::InitSkyboxPipelineDescriptorSets()
{
    // Create pipeline descirptor
    VkDescriptorSetAllocateInfo skyboxPipelineDesSet0AllocInfo{};
    {
        skyboxPipelineDesSet0AllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        skyboxPipelineDesSet0AllocInfo.descriptorPool = m_descriptorPool;
        skyboxPipelineDesSet0AllocInfo.pSetLayouts = &m_skyboxPipelineDesSet0Layout;
        skyboxPipelineDesSet0AllocInfo.descriptorSetCount = 1;
    }
    
    m_skyboxPipelineDescriptorSet0s.resize(SharedLib::MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < SharedLib::MAX_FRAMES_IN_FLIGHT; i++)
    {
        VK_CHECK(vkAllocateDescriptorSets(m_device,
                                          &skyboxPipelineDesSet0AllocInfo,
                                          &m_skyboxPipelineDescriptorSet0s[i]));
    }

    // Link descriptors to the buffer and image
    VkDescriptorImageInfo hdriDesImgInfo{};
    {
        hdriDesImgInfo.imageView = m_hdrCubeMapView;
        hdriDesImgInfo.sampler = m_hdrSampler;
        hdriDesImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    for (uint32_t i = 0; i < SharedLib::MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkDescriptorBufferInfo desCameraParaBufInfo{};
        {
            desCameraParaBufInfo.buffer = m_cameraParaBuffers[i];
            desCameraParaBufInfo.offset = 0;
            desCameraParaBufInfo.range = sizeof(float) * 16;
        }

        VkWriteDescriptorSet writeCameraBufDesSet{};
        {
            writeCameraBufDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeCameraBufDesSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writeCameraBufDesSet.dstSet = m_skyboxPipelineDescriptorSet0s[i];
            writeCameraBufDesSet.dstBinding = 1;
            writeCameraBufDesSet.descriptorCount = 1;
            writeCameraBufDesSet.pBufferInfo = &desCameraParaBufInfo;
        }

        VkWriteDescriptorSet writeHdrDesSet{};
        {
            writeHdrDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeHdrDesSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writeHdrDesSet.dstSet = m_skyboxPipelineDescriptorSet0s[i];
            writeHdrDesSet.dstBinding = 0;
            writeHdrDesSet.pImageInfo = &hdriDesImgInfo;
            writeHdrDesSet.descriptorCount = 1;
        }

        // Linking skybox pipeline descriptors: skybox cubemap and camera buffer descriptors to their GPU memory
        // and info.
        VkWriteDescriptorSet writeSkyboxPipelineDescriptors[2] = { writeHdrDesSet, writeCameraBufDesSet };
        vkUpdateDescriptorSets(m_device, 2, writeSkyboxPipelineDescriptors, 0, NULL);
    }
}

// ================================================================================================================
void PBRIBLGltfApp::InitSkyboxPipelineLayout()
{
    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    {
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_skyboxPipelineDesSet0Layout;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
    }
    
    VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_skyboxPipelineLayout));
}

// ================================================================================================================
void PBRIBLGltfApp::InitSkyboxShaderModules()
{
    // Create Shader Modules.
    m_vsSkyboxShaderModule = CreateShaderModule("./hlsl/skybox_vert.spv");
    m_psSkyboxShaderModule = CreateShaderModule("./hlsl/skybox_frag.spv");
}

// ================================================================================================================
void PBRIBLGltfApp::InitSkyboxPipelineDescriptorSetLayout()
{
    // Create pipeline binding and descriptor objects for the camera parameters
    VkDescriptorSetLayoutBinding cameraUboBinding{};
    {
        cameraUboBinding.binding = 1;
        cameraUboBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        cameraUboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        cameraUboBinding.descriptorCount = 1;
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
    VkDescriptorSetLayoutBinding skyboxPipelineDesSet0LayoutBindings[2] = { hdriSamplerBinding, cameraUboBinding };
    VkDescriptorSetLayoutCreateInfo skyboxPipelineDesSet0LayoutInfo{};
    {
        skyboxPipelineDesSet0LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        skyboxPipelineDesSet0LayoutInfo.bindingCount = 2;
        skyboxPipelineDesSet0LayoutInfo.pBindings = skyboxPipelineDesSet0LayoutBindings;
    }
    
    VK_CHECK(vkCreateDescriptorSetLayout(m_device,
                                         &skyboxPipelineDesSet0LayoutInfo,
                                         nullptr,
                                         &m_skyboxPipelineDesSet0Layout));
}

// ================================================================================================================
void PBRIBLGltfApp::InitSkyboxPipeline()
{
    VkPipelineRenderingCreateInfoKHR pipelineRenderCreateInfo{};
    {
        pipelineRenderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        pipelineRenderCreateInfo.colorAttachmentCount = 1;
        pipelineRenderCreateInfo.pColorAttachmentFormats = &m_choisenSurfaceFormat.format;
    }

    m_skyboxPipeline.SetPNext(&pipelineRenderCreateInfo);
    
    VkPipelineShaderStageCreateInfo shaderStgsInfo[2] = {};
    shaderStgsInfo[0] = CreateDefaultShaderStgCreateInfo(m_vsSkyboxShaderModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStgsInfo[1] = CreateDefaultShaderStgCreateInfo(m_psSkyboxShaderModule, VK_SHADER_STAGE_FRAGMENT_BIT);

    m_skyboxPipeline.SetShaderStageInfo(shaderStgsInfo, 2);
    m_skyboxPipeline.SetPipelineLayout(m_skyboxPipelineLayout);
    m_skyboxPipeline.CreatePipeline(m_device);
}

// ================================================================================================================
void PBRIBLGltfApp::DestroySkyboxPipelineRes()
{
    // Destroy shader modules
    vkDestroyShaderModule(m_device, m_vsSkyboxShaderModule, nullptr);
    vkDestroyShaderModule(m_device, m_psSkyboxShaderModule, nullptr);

    // Destroy the pipeline layout
    vkDestroyPipelineLayout(m_device, m_skyboxPipelineLayout, nullptr);

    // Destroy the descriptor set layout
    vkDestroyDescriptorSetLayout(m_device, m_skyboxPipelineDesSet0Layout, nullptr);
}

// ================================================================================================================
void PBRIBLGltfApp::AppInit()
{
    glfwInit();
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> instExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    InitInstance(instExtensions, glfwExtensionCount);

    // Init glfw window.
    InitGlfwWindowAndCallbacks();
    glfwSetMouseButtonCallback(m_pWindow, MouseButtonCallback);

    // Create vulkan surface from the glfw window.
    VK_CHECK(glfwCreateWindowSurface(m_instance, m_pWindow, nullptr, &m_surface));

    InitPhysicalDevice();
    InitGfxQueueFamilyIdx();
    InitPresentQueueFamilyIdx();

    // Queue family index should be unique in vk1.2:
    // https://vulkan.lunarg.com/doc/view/1.2.198.0/windows/1.2-extensions/vkspec.html#VUID-VkDeviceCreateInfo-queueFamilyIndex-02802
    std::vector<VkDeviceQueueCreateInfo> deviceQueueInfos = CreateDeviceQueueInfos({ m_graphicsQueueFamilyIdx,
                                                                                     m_presentQueueFamilyIdx });
    // We need the swap chain device extension and the dynamic rendering extension.
    const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                                        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME };

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeature{};
    {
        dynamicRenderingFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
        dynamicRenderingFeature.dynamicRendering = VK_TRUE;
    }

    InitDevice(deviceExtensions, 2, deviceQueueInfos, &dynamicRenderingFeature);
    InitVmaAllocator();
    InitGraphicsQueue();
    InitPresentQueue();
    InitDescriptorPool();

    InitGfxCommandPool();
    InitGfxCommandBuffers(SharedLib::MAX_FRAMES_IN_FLIGHT);

    InitSwapchain();
    InitModelInfo();
    InitVpMatBuffer();

    // Create the graphics pipeline
    InitSkyboxShaderModules();
    InitSkyboxPipelineDescriptorSetLayout();
    InitSkyboxPipelineLayout();
    InitSkyboxPipeline();

    InitIblShaderModules();
    InitIblPipelineDescriptorSetLayout();
    InitIblPipelineLayout();
    InitIblPipeline();

    InitHdrRenderObjects();
    InitCameraUboObjects();
    InitSkyboxPipelineDescriptorSets();
    InitIblPipelineDescriptorSets();
    InitSwapchainSyncObjects();
}

// ================================================================================================================
// NOTE: Currently, we don't support gltf nodes's position, rotation and scale information. 
//       * We only support triangle.
// TODO: A formal gltf model loader should load a model from it's node and apply its attributes recursively.
// TODO: The gltf model loader should put into the shared library.
void PBRIBLGltfApp::InitModelInfo()
{
    std::string inputfile = SOURCE_PATH;
    inputfile += "/../data/glTF/FlightHelmet.gltf";

    std::string modelPath = SOURCE_PATH;
    modelPath += "/../data/glTF/";

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, inputfile);
    //bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, argv[1]); // for binary glTF(.glb)
    if (!warn.empty()) {
        printf("Warn: %s\n", warn.c_str());
    }

    if (!err.empty()) {
        printf("Err: %s\n", err.c_str());
    }

    if (!ret) {
        printf("Failed to parse glTF\n");
        exit(1);
    }

    // NOTE: TinyGltf loader has already loaded the binary buffer data and the images data.
    const auto& binaryBuffer = model.buffers[0].data;
    const unsigned char* pBufferData = binaryBuffer.data();

    uint32_t meshCnt = model.meshes.size();
    m_gltfModeMeshes.resize(meshCnt);
    for (uint32_t i = 0; i < meshCnt; i++)
    {
        const auto& mesh = model.meshes[i];
        int posIdx = mesh.primitives[0].attributes.at("POSITION");
        int normalIdx = mesh.primitives[0].attributes.at("NORMAL");
        int tangentIdx = mesh.primitives[0].attributes.at("TANGENT");
        int uvIdx = mesh.primitives[0].attributes.at("TEXCOORD_0");
        int indicesIdx = mesh.primitives[0].indices;
        int materialIdx = mesh.primitives[0].material;

        // Elements notes:
        // Position: float3, normal: float3, tangent: float4, texcoord: float2.

        // Setup the vertex buffer and the index buffer
        const auto& posAccessor = model.accessors[posIdx];
        int posAccessorByteOffset = posAccessor.byteOffset;
        int posAccessorEleCnt = posAccessor.count; // Assume a position element is a float3.

        const auto& normalAccessor = model.accessors[normalIdx];
        int normalAccessorByteOffset = normalAccessor.byteOffset;
        int normalAccessorEleCnt = normalAccessor.count;

        const auto& tangentAccessor = model.accessors[tangentIdx];
        int tangentAccessorByteOffset = tangentAccessor.byteOffset;
        int tangentAccessorEleCnt = tangentAccessor.count;

        const auto& uvAccessor = model.accessors[uvIdx];
        int uvAccessorByteOffset = uvAccessor.byteOffset;
        int uvAccessorEleCnt = uvAccessor.count;

        const auto& idxAccessor = model.accessors[indicesIdx];
        int idxAccessorByteOffset = idxAccessor.byteOffset;
        int idxAccessorEleCnt = idxAccessor.count;

        // NOTE: Buffer views are just division of the buffer for the flight helmet model.
        // SCALAR is in one buffer view. FLOAT2 in one. FLOAT3 in one. and FLOAT3 in one...
        // Maybe they can be more
        // A buffer view represents a contiguous segment of data in a buffer, defined by a byte offset into the buffer specified 
        // in the byteOffset property and a total byte length specified by the byteLength property of the buffer view.
        const auto& posBufferView = model.bufferViews[posAccessor.bufferView];
        const auto& normalBufferView = model.bufferViews[normalAccessor.bufferView];
        const auto& tangentBufferView = model.bufferViews[tangentAccessor.bufferView];
        const auto& uvBufferView = model.bufferViews[uvAccessor.bufferView];
        const auto& idxBufferView = model.bufferViews[idxAccessor.bufferView];

        // We assume that the idx, position, normal, uv and tangent are not interleaved.
        // TODO: Even though they are interleaved, we can use a function to read out the data by making use of the stride bytes count.

        // Assmue the data and element type of the index is uint16_t.
        int idxBufferOffset = idxAccessorByteOffset + idxBufferView.byteOffset;
        int idxBufferByteCnt = sizeof(uint16_t) * idxAccessor.count;
        m_gltfModeMeshes[i].idxData.resize(idxAccessor.count);
        memcpy(m_gltfModeMeshes[i].idxData.data(), &pBufferData[idxBufferOffset], idxBufferByteCnt);
        
        // Assmue the data and element type of the position is float3
        int posBufferOffset = posAccessorByteOffset + posBufferView.byteOffset;
        int posBufferByteCnt = sizeof(float) * 3 * posAccessor.count;
        float* pPosData = new float[3 * posAccessor.count];
        memcpy(pPosData, &pBufferData[posBufferOffset], posBufferByteCnt);

        // Assmue the data and element type of the normal is float3.
        int normalBufferOffset = normalAccessorByteOffset + normalBufferView.byteOffset;
        int normalBufferByteCnt = sizeof(float) * 3 * normalAccessor.count;
        float* pNomralData = new float[3 * normalAccessor.count];
        memcpy(pNomralData, &pBufferData[normalBufferOffset], normalBufferByteCnt);

        // Assmue the data and element type of the tangent is float4.
        int tangentBufferOffset = tangentAccessorByteOffset + tangentBufferView.byteOffset;
        int tangentBufferByteCnt = sizeof(float) * 4 * tangentAccessor.count;
        float* pTangentData = new float[4 * tangentAccessor.count];
        memcpy(pTangentData, &pBufferData[tangentBufferOffset], tangentBufferByteCnt);

        // Assume the data and element type of the texcoord is float2.
        int uvBufferOffset = uvAccessorByteOffset + uvBufferView.byteOffset;
        int uvBufferByteCnt = sizeof(float) * 2 * uvAccessor.count;
        float* pUvData = new float[2 * uvAccessor.count];
        memcpy(pUvData, &pBufferData[uvBufferOffset], uvBufferByteCnt);

        // Assemble the vert buffer, fill the mesh information and send vert buffer and idx buffer to VkBuffer.

        // Fill the vert buffer
        int vertBufferByteCnt = posBufferByteCnt + normalBufferByteCnt + tangentBufferByteCnt + uvBufferByteCnt;
        int vertBufferDwordCnt = vertBufferByteCnt / sizeof(float);
        
        m_gltfModeMeshes[i].vertData.resize(vertBufferDwordCnt);

        // The count of [pos, normal, tangent, uv] is equal to posAccessor/normalAccessor/tangentAccessor/uvAccessor.count.
        // [3 floats, 3 floats, 4 floats, 2 floats] --> 12 floats.
        for (uint32_t vertIdx = 0; vertIdx < posAccessor.count; vertIdx++)
        {
            // pos -- 3 floats
            m_gltfModeMeshes[i].vertData[12 * vertIdx] = pPosData[3 * vertIdx];
            m_gltfModeMeshes[i].vertData[12 * vertIdx + 1] = pPosData[3 * vertIdx + 1];
            m_gltfModeMeshes[i].vertData[12 * vertIdx + 2] = pPosData[3 * vertIdx + 2];

            // normal -- 3 floats
            m_gltfModeMeshes[i].vertData[12 * vertIdx + 3] = pNomralData[3 * vertIdx];
            m_gltfModeMeshes[i].vertData[12 * vertIdx + 4] = pNomralData[3 * vertIdx + 1];
            m_gltfModeMeshes[i].vertData[12 * vertIdx + 5] = pNomralData[3 * vertIdx + 2];

            // tangent -- 4 floats
            m_gltfModeMeshes[i].vertData[12 * vertIdx + 6] = pTangentData[4 * vertIdx];
            m_gltfModeMeshes[i].vertData[12 * vertIdx + 7] = pTangentData[4 * vertIdx + 1];
            m_gltfModeMeshes[i].vertData[12 * vertIdx + 8] = pTangentData[4 * vertIdx + 2];
            m_gltfModeMeshes[i].vertData[12 * vertIdx + 9] = pTangentData[4 * vertIdx + 3];

            // uv -- 2 floats
            m_gltfModeMeshes[i].vertData[12 * vertIdx + 10] = pTangentData[2 * vertIdx];
            m_gltfModeMeshes[i].vertData[12 * vertIdx + 11] = pTangentData[2 * vertIdx + 1];
        }

        // Create the VkBuffer for the idx buffer.
        {
            VkBufferCreateInfo idxBufferInfo{};
            {
                idxBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                idxBufferInfo.size = idxBufferByteCnt;
                idxBufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
                idxBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            }

            VmaAllocationCreateInfo idxBufferAllocInfo{};
            {
                idxBufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
                idxBufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
                                           VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            }

            vmaCreateBuffer(*m_pAllocator,
                            &idxBufferInfo,
                            &idxBufferAllocInfo,
                            &m_gltfModeMeshes[i].modelIdxBuffer,
                            &m_gltfModeMeshes[i].modelIdxBufferAlloc,
                            nullptr);
        }

        // Create the VkBuffer for the vert buffer.
        {
            VkBufferCreateInfo vertBufferInfo{};
            {
                vertBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                vertBufferInfo.size = vertBufferByteCnt;
                vertBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
                vertBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            }

            VmaAllocationCreateInfo vertBufferAllocInfo{};
            {
                vertBufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
                vertBufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
                                            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            }

            vmaCreateBuffer(*m_pAllocator,
                            &vertBufferInfo,
                            &vertBufferAllocInfo,
                            &m_gltfModeMeshes[i].modelVertBuffer,
                            &m_gltfModeMeshes[i].modelVertBufferAlloc,
                            nullptr);
        }

        // Send idx data and vert data to their VkBuffers.
        CopyRamDataToGpuBuffer(m_gltfModeMeshes[i].vertData.data(),
                               m_gltfModeMeshes[i].modelVertBuffer,
                               m_gltfModeMeshes[i].modelVertBufferAlloc,
                               vertBufferByteCnt);

        CopyRamDataToGpuBuffer(m_gltfModeMeshes[i].idxData.data(),
                               m_gltfModeMeshes[i].modelIdxBuffer,
                               m_gltfModeMeshes[i].modelIdxBufferAlloc,
                               idxBufferByteCnt);

        // Send image info to GPU and set relevant data
        const auto& material = model.materials[materialIdx];

        // A texture binding is defined by an index of a texture object and an optional index of texture coordinates.
        // Its green channel contains roughness values and its blue channel contains metalness values.
        int baseColorTexIdx = material.pbrMetallicRoughness.baseColorTexture.index;
        int metallicRoughnessTexIdx = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
        int occlusionTexIdx = material.occlusionTexture.index;
        int normalTexIdx = material.normalTexture.index;
        // material.emissiveTexture -- Let forget emissive. The renderer doesn't support emissive textures.

        // A texture is defined by an image index, denoted by the source property and a sampler index (sampler).
        // Assmue that all textures are 8 bits per channel. They are all xxx / 255. They all have 4 components.
        const auto& baseColorTex = model.textures[baseColorTexIdx];
        const auto& metallicRoughnessTex = model.textures[metallicRoughnessTexIdx];
        const auto& occlusionTex = model.textures[occlusionTexIdx];
        const auto& normalTex = model.textures[normalTexIdx];

        int baseColorTexImgIdx = baseColorTex.source;
        int metallicRoughnessTexImgIdx = metallicRoughnessTex.source;
        int occlusionTexImgIdx = occlusionTex.source;
        int normalTexImgIdx = normalTex.source;

        const auto& baseColorImg = model.images[baseColorTexImgIdx];
        const auto& metalllicRoughnessImg = model.images[metallicRoughnessTexImgIdx];
        const auto& occlusionImg = model.images[occlusionTexImgIdx];
        const auto& normalImg = model.images[normalTexImgIdx];

        // Base Color Img
        {
            m_gltfModeMeshes[i].baseColorTex.pixWidth = baseColorImg.width;
            m_gltfModeMeshes[i].baseColorTex.pixHeight = baseColorImg.height;
            m_gltfModeMeshes[i].baseColorTex.componentCnt = 4;
            m_gltfModeMeshes[i].baseColorTex.dataVec.resize(baseColorImg.width * baseColorImg.height * 4);
            m_gltfModeMeshes[i].baseColorTex.dataVec = baseColorImg.image;
            
            VmaAllocationCreateInfo baseColorAllocInfo{};
            {
                baseColorAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
                baseColorAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
            }

            VkExtent3D extent{};
            {
                extent.width = baseColorImg.width;
                extent.height = baseColorImg.height;
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

            vmaCreateImage(*m_pAllocator,
                           &baseColorImgInfo,
                           &baseColorAllocInfo,
                           &m_gltfModeMeshes[i].baseColorImg,
                           &m_gltfModeMeshes[i].baseColorImgAlloc,
                           nullptr);

            VkImageViewCreateInfo info{};
            {
                info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                info.image = m_gltfModeMeshes[i].baseColorImg;
                info.viewType = VK_IMAGE_VIEW_TYPE_2D;
                info.format = VK_FORMAT_R8G8B8A8_SRGB;
                info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                info.subresourceRange.levelCount = 1;
                info.subresourceRange.layerCount = 1;
            }
            VK_CHECK(vkCreateImageView(m_device, &info, nullptr, &m_gltfModeMeshes[i].baseColorImgView));

            VkSamplerCreateInfo sampler_info{};
            {
                sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
                sampler_info.magFilter = VK_FILTER_LINEAR;
                sampler_info.minFilter = VK_FILTER_LINEAR;
                sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
                sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                sampler_info.minLod = -1000;
                sampler_info.maxLod = 1000;
                sampler_info.maxAnisotropy = 1.0f;
            }
            VK_CHECK(vkCreateSampler(m_device, &sampler_info, nullptr, &m_gltfModeMeshes[i].baseColorImgSampler));
        }

        // Metallic roughness
        {
            m_gltfModeMeshes[i].metallicRoughnessTex.pixWidth = metalllicRoughnessImg.width;
            m_gltfModeMeshes[i].metallicRoughnessTex.pixHeight = metalllicRoughnessImg.height;
            m_gltfModeMeshes[i].metallicRoughnessTex.componentCnt = 4;
            m_gltfModeMeshes[i].metallicRoughnessTex.dataVec.resize(metalllicRoughnessImg.width * metalllicRoughnessImg.height * 4);
            m_gltfModeMeshes[i].metallicRoughnessTex.dataVec = metalllicRoughnessImg.image;

            VmaAllocationCreateInfo metallicRoughnessAllocInfo{};
            {
                metallicRoughnessAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
                metallicRoughnessAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
            }

            VkExtent3D extent{};
            {
                extent.width = metalllicRoughnessImg.width;
                extent.height = metalllicRoughnessImg.height;
                extent.depth = 1;
            }

            VkImageCreateInfo metalllicRoughnessImgInfo{};
            {
                metalllicRoughnessImgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                metalllicRoughnessImgInfo.imageType = VK_IMAGE_TYPE_2D;
                metalllicRoughnessImgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
                metalllicRoughnessImgInfo.extent = extent;
                metalllicRoughnessImgInfo.mipLevels = 1;
                metalllicRoughnessImgInfo.arrayLayers = 1;
                metalllicRoughnessImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
                metalllicRoughnessImgInfo.tiling = VK_IMAGE_TILING_LINEAR;
                metalllicRoughnessImgInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
                metalllicRoughnessImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            }

            vmaCreateImage(*m_pAllocator,
                           &metalllicRoughnessImgInfo,
                           &metallicRoughnessAllocInfo,
                           &m_gltfModeMeshes[i].metallicRoughnessImg,
                           &m_gltfModeMeshes[i].metallicRoughnessImgAlloc,
                           nullptr);

            VkImageViewCreateInfo info{};
            {
                info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                info.image = m_gltfModeMeshes[i].metallicRoughnessImg;
                info.viewType = VK_IMAGE_VIEW_TYPE_2D;
                info.format = VK_FORMAT_R8G8B8A8_UNORM;
                info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                info.subresourceRange.levelCount = 1;
                info.subresourceRange.layerCount = 1;
            }
            VK_CHECK(vkCreateImageView(m_device, &info, nullptr, &m_gltfModeMeshes[i].metallicRoughnessImgView));

            VkSamplerCreateInfo sampler_info{};
            {
                sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
                sampler_info.magFilter = VK_FILTER_LINEAR;
                sampler_info.minFilter = VK_FILTER_LINEAR;
                sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
                sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                sampler_info.minLod = -1000;
                sampler_info.maxLod = 1000;
                sampler_info.maxAnisotropy = 1.0f;
            }
            VK_CHECK(vkCreateSampler(m_device, &sampler_info, nullptr, &m_gltfModeMeshes[i].metallicRoughnessImgSampler));
        }

        // Occlusion
        {
            m_gltfModeMeshes[i].occlusionTex.pixWidth = occlusionImg.width;
            m_gltfModeMeshes[i].occlusionTex.pixHeight = occlusionImg.height;
            m_gltfModeMeshes[i].occlusionTex.componentCnt = 4;
            m_gltfModeMeshes[i].occlusionTex.dataVec.resize(occlusionImg.width * occlusionImg.height * 4);
            m_gltfModeMeshes[i].occlusionTex.dataVec = occlusionImg.image;

            VmaAllocationCreateInfo occlusionAllocInfo{};
            {
                occlusionAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
                occlusionAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
            }

            VkExtent3D extent{};
            {
                extent.width = occlusionImg.width;
                extent.height = occlusionImg.height;
                extent.depth = 1;
            }

            VkImageCreateInfo occlusionImgInfo{};
            {
                occlusionImgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                occlusionImgInfo.imageType = VK_IMAGE_TYPE_2D;
                occlusionImgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
                occlusionImgInfo.extent = extent;
                occlusionImgInfo.mipLevels = 1;
                occlusionImgInfo.arrayLayers = 1;
                occlusionImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
                occlusionImgInfo.tiling = VK_IMAGE_TILING_LINEAR;
                occlusionImgInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
                occlusionImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            }

            vmaCreateImage(*m_pAllocator,
                           &occlusionImgInfo,
                           &occlusionAllocInfo,
                           &m_gltfModeMeshes[i].occlusionImg,
                           &m_gltfModeMeshes[i].occlusionImgAlloc,
                           nullptr);

            VkImageViewCreateInfo info{};
            {
                info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                info.image = m_gltfModeMeshes[i].occlusionImg;
                info.viewType = VK_IMAGE_VIEW_TYPE_2D;
                info.format = VK_FORMAT_R8G8B8A8_UNORM;
                info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                info.subresourceRange.levelCount = 1;
                info.subresourceRange.layerCount = 1;
            }
            VK_CHECK(vkCreateImageView(m_device, &info, nullptr, &m_gltfModeMeshes[i].occlusionImgView));

            VkSamplerCreateInfo sampler_info{};
            {
                sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
                sampler_info.magFilter = VK_FILTER_LINEAR;
                sampler_info.minFilter = VK_FILTER_LINEAR;
                sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
                sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                sampler_info.minLod = -1000;
                sampler_info.maxLod = 1000;
                sampler_info.maxAnisotropy = 1.0f;
            }
            VK_CHECK(vkCreateSampler(m_device, &sampler_info, nullptr, &m_gltfModeMeshes[i].occlusionImgSampler));
        }

        // Normal
        {
            m_gltfModeMeshes[i].normalTex.pixWidth = normalImg.width;
            m_gltfModeMeshes[i].normalTex.pixHeight = normalImg.height;
            m_gltfModeMeshes[i].normalTex.componentCnt = 4;
            m_gltfModeMeshes[i].normalTex.dataVec.resize(normalImg.width * normalImg.height * 4);
            m_gltfModeMeshes[i].normalTex.dataVec = normalImg.image;

            VmaAllocationCreateInfo normalAllocInfo{};
            {
                normalAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
                normalAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
            }

            VkExtent3D extent{};
            {
                extent.width = normalImg.width;
                extent.height = normalImg.height;
                extent.depth = 1;
            }

            VkImageCreateInfo normalImgInfo{};
            {
                normalImgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                normalImgInfo.imageType = VK_IMAGE_TYPE_2D;
                normalImgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
                normalImgInfo.extent = extent;
                normalImgInfo.mipLevels = 1;
                normalImgInfo.arrayLayers = 1;
                normalImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
                normalImgInfo.tiling = VK_IMAGE_TILING_LINEAR;
                normalImgInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
                normalImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            }

            vmaCreateImage(*m_pAllocator,
                           &normalImgInfo,
                           &normalAllocInfo,
                           &m_gltfModeMeshes[i].normalImg,
                           &m_gltfModeMeshes[i].normalImgAlloc,
                           nullptr);

            VkImageViewCreateInfo info{};
            {
                info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                info.image = m_gltfModeMeshes[i].normalImg;
                info.viewType = VK_IMAGE_VIEW_TYPE_2D;
                info.format = VK_FORMAT_R8G8B8A8_UNORM;
                info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                info.subresourceRange.levelCount = 1;
                info.subresourceRange.layerCount = 1;
            }
            VK_CHECK(vkCreateImageView(m_device, &info, nullptr, &m_gltfModeMeshes[i].normalImgView));

            VkSamplerCreateInfo sampler_info{};
            {
                sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
                sampler_info.magFilter = VK_FILTER_LINEAR;
                sampler_info.minFilter = VK_FILTER_LINEAR;
                sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
                sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                sampler_info.minLod = -1000;
                sampler_info.maxLod = 1000;
                sampler_info.maxAnisotropy = 1.0f;
            }
            VK_CHECK(vkCreateSampler(m_device, &sampler_info, nullptr, &m_gltfModeMeshes[i].normalImgSampler));
        }
    }
}

// ================================================================================================================
uint32_t PBRIBLGltfApp::GetModelTexCnt()
{
    uint32_t cnt = 0;
    for (const auto& mesh : m_gltfModeMeshes)
    {
        if (mesh.baseColorImg != VK_NULL_HANDLE)
        {
            cnt++;
        }

        if (mesh.normalImg != VK_NULL_HANDLE)
        {
            cnt++;
        }

        if (mesh.metallicRoughnessImg != VK_NULL_HANDLE)
        {
            cnt++;
        }

        if (mesh.occlusionImg != VK_NULL_HANDLE)
        {
            cnt++;
        }

        if (mesh.emissiveImg != VK_NULL_HANDLE)
        {
            cnt++;
        }
    }
    return cnt;
}

// ================================================================================================================
void PBRIBLGltfApp::SendModelTexDataToGPU(
    VkCommandBuffer cmdBuffer)
{

}

// ================================================================================================================
void PBRIBLGltfApp::DestroyModelInfo()
{
    for (const auto& mesh : m_gltfModeMeshes)
    {
        vmaDestroyBuffer(*m_pAllocator, mesh.modelIdxBuffer, mesh.modelIdxBufferAlloc);
        vmaDestroyBuffer(*m_pAllocator, mesh.modelVertBuffer, mesh.modelVertBufferAlloc);

        if (mesh.baseColorImg != VK_NULL_HANDLE)
        {
            vmaDestroyImage(*m_pAllocator, mesh.baseColorImg, mesh.baseColorImgAlloc);
            vkDestroyImageView(m_device, mesh.baseColorImgView, nullptr);
            vkDestroySampler(m_device, mesh.baseColorImgSampler, nullptr);
        }

        if (mesh.normalImg != VK_NULL_HANDLE)
        {
            vmaDestroyImage(*m_pAllocator, mesh.normalImg, mesh.normalImgAlloc);
            vkDestroyImageView(m_device, mesh.normalImgView, nullptr);
            vkDestroySampler(m_device, mesh.normalImgSampler, nullptr);
        }

        if (mesh.metallicRoughnessImg != VK_NULL_HANDLE)
        {
            vmaDestroyImage(*m_pAllocator, mesh.metallicRoughnessImg, mesh.metallicRoughnessImgAlloc);
            vkDestroyImageView(m_device, mesh.metallicRoughnessImgView, nullptr);
            vkDestroySampler(m_device, mesh.metallicRoughnessImgSampler, nullptr);
        }

        if (mesh.occlusionImg != VK_NULL_HANDLE)
        {
            vmaDestroyImage(*m_pAllocator, mesh.occlusionImg, mesh.occlusionImgAlloc);
            vkDestroyImageView(m_device, mesh.occlusionImgView, nullptr);
            vkDestroySampler(m_device, mesh.occlusionImgSampler, nullptr);
        }

        if (mesh.emissiveImg != VK_NULL_HANDLE)
        {
            vmaDestroyImage(*m_pAllocator, mesh.emissiveImg, mesh.emissiveImgAlloc);
            vkDestroyImageView(m_device, mesh.emissiveImgView, nullptr);
            vkDestroySampler(m_device, mesh.emissiveImgSampler, nullptr);
        }
    }
}

// ================================================================================================================
void PBRIBLGltfApp::InitIblPipeline()
{
    VkPipelineRenderingCreateInfoKHR pipelineRenderCreateInfo{};
    {
        pipelineRenderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        pipelineRenderCreateInfo.colorAttachmentCount = 1;
        pipelineRenderCreateInfo.pColorAttachmentFormats = &m_choisenSurfaceFormat.format;
        pipelineRenderCreateInfo.depthAttachmentFormat = VK_FORMAT_D16_UNORM;
    }

    m_iblPipeline.SetPNext(&pipelineRenderCreateInfo);

    VkPipelineShaderStageCreateInfo shaderStgsInfo[2] = {};
    shaderStgsInfo[0] = CreateDefaultShaderStgCreateInfo(m_vsIblShaderModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStgsInfo[1] = CreateDefaultShaderStgCreateInfo(m_psIblShaderModule, VK_SHADER_STAGE_FRAGMENT_BIT);

    VkPipelineVertexInputStateCreateInfo vertInputInfo = CreatePipelineVertexInputInfo();
    m_iblPipeline.SetVertexInputInfo(&vertInputInfo);

    VkPipelineDepthStencilStateCreateInfo depthStencilInfo = CreateDepthStencilStateInfo();
    m_iblPipeline.SetDepthStencilStateInfo(&depthStencilInfo);

    m_iblPipeline.SetShaderStageInfo(shaderStgsInfo, 2);
    m_iblPipeline.SetPipelineLayout(m_iblPipelineLayout);
    m_iblPipeline.CreatePipeline(m_device);
}

// ================================================================================================================
void PBRIBLGltfApp::InitIblPipelineDescriptorSetLayout()
{
    // Create pipeline binding and descriptor objects for the camera parameters
    VkDescriptorSetLayoutBinding vpMatUboBinding{};
    {
        vpMatUboBinding.binding = 0;
        vpMatUboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        vpMatUboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        vpMatUboBinding.descriptorCount = 1;
    }

    VkDescriptorSetLayoutBinding diffuseIrradianceBinding{};
    {
        diffuseIrradianceBinding.binding = 1;
        diffuseIrradianceBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        diffuseIrradianceBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        diffuseIrradianceBinding.descriptorCount = 1;
    }

    VkDescriptorSetLayoutBinding prefilterEnvBinding{};
    {
        prefilterEnvBinding.binding = 2;
        prefilterEnvBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        prefilterEnvBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        prefilterEnvBinding.descriptorCount = 1;
    }

    VkDescriptorSetLayoutBinding envBrdfBinding{};
    {
        envBrdfBinding.binding = 3;
        envBrdfBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        envBrdfBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        envBrdfBinding.descriptorCount = 1;
    }

    // Create pipeline's descriptors layout
    // The Vulkan spec states: The VkDescriptorSetLayoutBinding::binding members of the elements of the pBindings array 
    // must each have different values 
    // (https://vulkan.lunarg.com/doc/view/1.3.236.0/windows/1.3-extensions/vkspec.html#VUID-VkDescriptorSetLayoutCreateInfo-binding-00279)
    VkDescriptorSetLayoutBinding pipelineDesSetLayoutBindings[4] = { vpMatUboBinding,
                                                                     diffuseIrradianceBinding,
                                                                     prefilterEnvBinding,
                                                                     envBrdfBinding };

    VkDescriptorSetLayoutCreateInfo pipelineDesSetLayoutInfo{};
    {
        pipelineDesSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        pipelineDesSetLayoutInfo.bindingCount = 4;
        pipelineDesSetLayoutInfo.pBindings = pipelineDesSetLayoutBindings;
    }

    VK_CHECK(vkCreateDescriptorSetLayout(m_device,
                                         &pipelineDesSetLayoutInfo,
                                         nullptr,
                                         &m_iblPipelineDesSet0Layout));
}

// ================================================================================================================
void PBRIBLGltfApp::InitIblPipelineLayout()
{
    VkPushConstantRange range = {};
    {
        range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        range.offset = 0;
        range.size = sizeof(float); // Camera position.
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    {
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_iblPipelineDesSet0Layout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &range;
    }

    VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_iblPipelineLayout));
}

// ================================================================================================================
void PBRIBLGltfApp::InitIblShaderModules()
{
    m_vsIblShaderModule = CreateShaderModule("./hlsl/ibl_vert.spv");
    m_psIblShaderModule = CreateShaderModule("./hlsl/ibl_frag.spv");
}

// ================================================================================================================
void PBRIBLGltfApp::InitIblPipelineDescriptorSets()
{
    // Create pipeline descirptor
    VkDescriptorSetAllocateInfo pipelineDesSet0AllocInfo{};
    {
        pipelineDesSet0AllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        pipelineDesSet0AllocInfo.descriptorPool = m_descriptorPool;
        pipelineDesSet0AllocInfo.pSetLayouts = &m_iblPipelineDesSet0Layout;
        pipelineDesSet0AllocInfo.descriptorSetCount = 1;
    }

    // Link descriptors to the buffer and image
    VkDescriptorImageInfo diffIrrDesImgInfo{};
    {
        diffIrrDesImgInfo.imageView = m_diffuseIrradianceCubemapImgView;
        diffIrrDesImgInfo.sampler = m_diffuseIrradianceCubemapSampler;
        diffIrrDesImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    VkDescriptorImageInfo prefilterEnvDesImgInfo{};
    {
        prefilterEnvDesImgInfo.imageView = m_prefilterEnvCubemapView;
        prefilterEnvDesImgInfo.sampler = m_prefilterEnvCubemapSampler;
        prefilterEnvDesImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    VkDescriptorImageInfo envBrdfDesImgInfo{};
    {
        envBrdfDesImgInfo.imageView = m_envBrdfImgView;
        envBrdfDesImgInfo.sampler = m_envBrdfImgSampler;
        envBrdfDesImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    VkDescriptorImageInfo baseColorDesImgInfo{};
    {
        baseColorDesImgInfo.imageView = VK_NULL_HANDLE;
    }

    m_iblPipelineDescriptorSet0s.resize(SharedLib::MAX_FRAMES_IN_FLIGHT * m_gltfModeMeshes.size());

    for (uint32_t i = 0; i < SharedLib::MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkDescriptorBufferInfo vpMatDesBufInfo{};
        {
            vpMatDesBufInfo.buffer = m_vpMatUboBuffer[i];
            vpMatDesBufInfo.offset = 0;
            vpMatDesBufInfo.range = VpMatBytesCnt;
        }

        VK_CHECK(vkAllocateDescriptorSets(m_device,
                                          &pipelineDesSet0AllocInfo,
                                          &m_iblPipelineDescriptorSet0s[i]));

        std::vector<VkWriteDescriptorSet> writeIblPipelineDescriptors;

        VkWriteDescriptorSet writeVpMatUboBufDesSet{};
        {
            writeVpMatUboBufDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeVpMatUboBufDesSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writeVpMatUboBufDesSet.dstSet = m_iblPipelineDescriptorSet0s[i];
            writeVpMatUboBufDesSet.dstBinding = 0;
            writeVpMatUboBufDesSet.descriptorCount = 1;
            writeVpMatUboBufDesSet.pBufferInfo = &vpMatDesBufInfo;
        }
        writeIblPipelineDescriptors.push_back(writeVpMatUboBufDesSet);

        VkWriteDescriptorSet writeDiffIrrDesSet{};
        {
            writeDiffIrrDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDiffIrrDesSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writeDiffIrrDesSet.dstSet = m_iblPipelineDescriptorSet0s[i];
            writeDiffIrrDesSet.dstBinding = 1;
            writeDiffIrrDesSet.pImageInfo = &diffIrrDesImgInfo;
            writeDiffIrrDesSet.descriptorCount = 1;
        }
        writeIblPipelineDescriptors.push_back(writeDiffIrrDesSet);

        VkWriteDescriptorSet writePrefilterEnvDesSet{};
        {
            writePrefilterEnvDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writePrefilterEnvDesSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writePrefilterEnvDesSet.dstSet = m_iblPipelineDescriptorSet0s[i];
            writePrefilterEnvDesSet.dstBinding = 2;
            writePrefilterEnvDesSet.pImageInfo = &prefilterEnvDesImgInfo;
            writePrefilterEnvDesSet.descriptorCount = 1;
        }
        writeIblPipelineDescriptors.push_back(writePrefilterEnvDesSet);

        VkWriteDescriptorSet writeEnvBrdfDesSet{};
        {
            writeEnvBrdfDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeEnvBrdfDesSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writeEnvBrdfDesSet.dstSet = m_iblPipelineDescriptorSet0s[i];
            writeEnvBrdfDesSet.dstBinding = 3;
            writeEnvBrdfDesSet.pImageInfo = &envBrdfDesImgInfo;
            writeEnvBrdfDesSet.descriptorCount = 1;
        }
        writeIblPipelineDescriptors.push_back(writeEnvBrdfDesSet);

        VkWriteDescriptorSet writeBaseColorDesSet{};
        {
            writeBaseColorDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeBaseColorDesSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writeBaseColorDesSet.dstSet = m_iblPipelineDescriptorSet0s[i];
            writeBaseColorDesSet.dstBinding = 4;
            writeEnvBrdfDesSet.pImageInfo = nullptr;
            writeEnvBrdfDesSet.descriptorCount = 1;
        }
        writeIblPipelineDescriptors.push_back(writeBaseColorDesSet);



        // Linking pipeline descriptors: cubemap and scene buffer descriptors to their GPU memory and info.

        vkUpdateDescriptorSets(m_device,
                               writeIblPipelineDescriptors.size(),
                               writeIblPipelineDescriptors.data(), 0, NULL);
    }
}

// ================================================================================================================
void PBRIBLGltfApp::DestroyIblPipelineRes()
{
    // Destroy shader modules
    vkDestroyShaderModule(m_device, m_vsIblShaderModule, nullptr);
    vkDestroyShaderModule(m_device, m_psIblShaderModule, nullptr);

    // Destroy the pipeline layout
    vkDestroyPipelineLayout(m_device, m_iblPipelineLayout, nullptr);

    // Destroy the descriptor set layout
    vkDestroyDescriptorSetLayout(m_device, m_iblPipelineDesSet0Layout, nullptr);
}

// ================================================================================================================
void PBRIBLGltfApp::InitVpMatBuffer()
{
    VkBufferCreateInfo bufferInfo{};
    {
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = 16 * sizeof(float);
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo bufferAllocInfo{};
    {
        bufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        bufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
                                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    m_vpMatUboBuffer.resize(SharedLib::MAX_FRAMES_IN_FLIGHT);
    m_vpMatUboAlloc.resize(SharedLib::MAX_FRAMES_IN_FLIGHT);

    float vpMatData[16] = {};
    float tmpViewMatData[16] = {};
    float tmpPersMatData[16] = {};
    m_pCamera->GenViewPerspectiveMatrices(tmpViewMatData, tmpPersMatData, vpMatData);
    SharedLib::MatTranspose(vpMatData, 4);

    for (uint32_t i = 0; i < SharedLib::MAX_FRAMES_IN_FLIGHT; i++)
    {
        vmaCreateBuffer(*m_pAllocator,
                        &bufferInfo,
                        &bufferAllocInfo,
                        &m_vpMatUboBuffer[i],
                        &m_vpMatUboAlloc[i],
                        nullptr);

        CopyRamDataToGpuBuffer(vpMatData,
                               m_vpMatUboBuffer[i],
                               m_vpMatUboAlloc[i],
                               sizeof(vpMatData));
    }
}

// ================================================================================================================
void PBRIBLGltfApp::DestroyVpMatBuffer()
{
    for (uint32_t i = 0; i < SharedLib::MAX_FRAMES_IN_FLIGHT; i++)
    {
        vmaDestroyBuffer(*m_pAllocator, m_vpMatUboBuffer[i], m_vpMatUboAlloc[i]);
    }
}

// ================================================================================================================
// Elements notes:
// Position: float3, normal: float3, tangent: float4, texcoord: float2.
VkPipelineVertexInputStateCreateInfo PBRIBLGltfApp::CreatePipelineVertexInputInfo()
{
    // Specifying all kinds of pipeline states
    // Vertex input state
    VkVertexInputBindingDescription* pVertBindingDesc = new VkVertexInputBindingDescription();
    memset(pVertBindingDesc, 0, sizeof(VkVertexInputBindingDescription));
    {
        pVertBindingDesc->binding = 0;
        pVertBindingDesc->stride = 12 * sizeof(float);
        pVertBindingDesc->inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    }
    m_heapMemPtrVec.push_back(pVertBindingDesc);

    VkVertexInputAttributeDescription* pVertAttrDescs = new VkVertexInputAttributeDescription[4];
    memset(pVertAttrDescs, 0, sizeof(VkVertexInputAttributeDescription) * 4);
    {
        // Position
        pVertAttrDescs[0].location = 0;
        pVertAttrDescs[0].binding = 0;
        pVertAttrDescs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        pVertAttrDescs[0].offset = 0;
        // Normal
        pVertAttrDescs[1].location = 1;
        pVertAttrDescs[1].binding = 0;
        pVertAttrDescs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        pVertAttrDescs[1].offset = 3 * sizeof(float);
        // Tangent
        pVertAttrDescs[2].location = 2;
        pVertAttrDescs[2].binding = 0;
        pVertAttrDescs[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        pVertAttrDescs[2].offset = 6 * sizeof(float);
        // Texcoord
        pVertAttrDescs[3].location = 3;
        pVertAttrDescs[3].binding = 0;
        pVertAttrDescs[3].format = VK_FORMAT_R32G32_SFLOAT;
        pVertAttrDescs[3].offset = 10 * sizeof(float);
    }
    m_heapArrayMemPtrVec.push_back(pVertAttrDescs);

    VkPipelineVertexInputStateCreateInfo vertInputInfo{};
    {
        vertInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertInputInfo.pNext = nullptr;
        vertInputInfo.vertexBindingDescriptionCount = 1;
        vertInputInfo.pVertexBindingDescriptions = pVertBindingDesc;
        vertInputInfo.vertexAttributeDescriptionCount = 4;
        vertInputInfo.pVertexAttributeDescriptions = pVertAttrDescs;
    }

    return vertInputInfo;
}

// ================================================================================================================
VkPipelineDepthStencilStateCreateInfo PBRIBLGltfApp::CreateDepthStencilStateInfo()
{
    VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};
    {
        depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencilInfo.depthTestEnable = VK_TRUE;
        depthStencilInfo.depthWriteEnable = VK_TRUE;
        depthStencilInfo.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL; // Reverse depth for higher precision. 
        depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
        depthStencilInfo.stencilTestEnable = VK_FALSE;
    }

    return depthStencilInfo;
}