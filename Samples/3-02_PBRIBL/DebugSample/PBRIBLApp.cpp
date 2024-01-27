#include "PBRIBLApp.h"
#include <glfw3.h>
#include "../../../SharedLibrary/Utils/VulkanDbgUtils.h"
#include "../../../SharedLibrary/Camera/Camera.h"
#include "../../../SharedLibrary/Event/Event.h"
#include "../../../SharedLibrary/Utils/StrPathUtils.h"
#include "../../../SharedLibrary/Utils/AppUtils.h"
#include "../../../SharedLibrary/Utils/CmdBufUtils.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

// #define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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
PBRIBLApp::PBRIBLApp() : 
    GlfwApplication(),
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
    m_diffuseIrradianceCubemap(),
    m_prefilterEnvCubemap(),
    m_envBrdfImg(),
    m_hdrCubeMap(),
    m_vertBufferData(),
    m_idxBufferData(),
    m_vertBuffer(VK_NULL_HANDLE),
    m_vertBufferAlloc(VK_NULL_HANDLE),
    m_idxBuffer(VK_NULL_HANDLE),
    m_idxBufferAlloc(VK_NULL_HANDLE),
    m_prefilterEnvMipsCnt(0)
{
    m_pCamera = new SharedLib::Camera();
}

// ================================================================================================================
PBRIBLApp::~PBRIBLApp()
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
void PBRIBLApp::DestroyHdrRenderObjs()
{
    DestroySphereVertexIndexBuffers();

    DestroyGpuImgResource(m_diffuseIrradianceCubemap);
    DestroyGpuImgResource(m_prefilterEnvCubemap);
    DestroyGpuImgResource(m_envBrdfImg);
    DestroyGpuImgResource(m_hdrCubeMap);
}

// ================================================================================================================
void PBRIBLApp::DestroyCameraUboObjects()
{
    for (uint32_t i = 0; i < m_swapchainImgCnt; i++)
    {
        DestroyGpuBufferResource(m_cameraParaBuffers[i]);
    }
}

// ================================================================================================================
void PBRIBLApp::SendCameraDataToBuffer()
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

    CopyRamDataToGpuBuffer(cameraData,
                           m_cameraParaBuffers[m_acqSwapchainImgIdx].buffer,
                           m_cameraParaBuffers[m_acqSwapchainImgIdx].bufferAlloc,
                           sizeof(cameraData));

    CopyRamDataToGpuBuffer(vpMatData,
                           m_vpMatUboBuffer[m_acqSwapchainImgIdx].buffer,
                           m_vpMatUboBuffer[m_acqSwapchainImgIdx].bufferAlloc,
                           sizeof(vpMatData));
}

// ================================================================================================================
void PBRIBLApp::UpdateCameraAndGpuBuffer()
{
    SharedLib::HEvent midMouseDownEvent = CreateMiddleMouseEvent(g_isDown);
    m_pCamera->OnEvent(midMouseDownEvent);
    SendCameraDataToBuffer();
}

// ================================================================================================================
void PBRIBLApp::GetCameraPos(
    float* pOut)
{
    m_pCamera->GetPos(pOut);
}

// ================================================================================================================
// - In the `cmftStudio`, you can choose hStrip. The code below is an example of using the hStrip.
// - The buffer data of the image cannot be interleaved (The data of a separate image should be continues in the buffer address space.)
// - However, our cubemap data (hStrip) is interleaved. 
// - So, we have multiple choices to put them into the cubemap image. Here, I choose to offset the buffer starting point, specify the
// -     long row length and copy that for 6 times.
        /*
        VkBufferImageCopy hdrBufToImgCopies[6];
        memset(hdrBufToImgCopies, 0, sizeof(hdrBufToImgCopies));
        for (uint32_t i = 0; i < 6; i++)
        {
            VkExtent3D extent{};
            {
                extent.width = hdrImgExtent.width / 6;
                extent.height = hdrImgExtent.height;
                extent.depth = 1;
            }

            hdrBufToImgCopies[i].bufferRowLength = hdrImgExtent.width;
            // hdrBufToImgCopies[i].bufferImageHeight = hdrImgExtent.height;
            hdrBufToImgCopies[i].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            hdrBufToImgCopies[i].imageSubresource.mipLevel = 0;
            hdrBufToImgCopies[i].imageSubresource.baseArrayLayer = i;
            hdrBufToImgCopies[i].imageSubresource.layerCount = 1;

            hdrBufToImgCopies[i].imageExtent = extent;
            // In the unit of bytes:
            hdrBufToImgCopies[i].bufferOffset = i * (hdrImgExtent.width / 6) * sizeof(float) * 3;
        }

        vkCmdCopyBufferToImage(
            stagingCmdBuffer,
            stagingBuffer,
            cubeMapImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            6, hdrBufToImgCopies);
        */
void PBRIBLApp::InitHdrRenderObjects()
{
    SharedLib::RAIICommandBuffer raiiCmdBuffer(m_gfxCmdPool, m_device);

    // Load the HDRI image into RAM
    std::string hdriFilePath = SOURCE_PATH;
    hdriFilePath += "/../data/";

    VkSamplerCreateInfo samplerInfo{};
    {
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.minLod = -1000;
        samplerInfo.maxLod = 1000;
        samplerInfo.maxAnisotropy = 1.0f;
    }

    // Read in and init background cubemap
    {
        std::string cubemapPathName = hdriFilePath + "iblOutput/background_cubemap.hdr";

        int width, height, nrComponents;
        float* pHdrImgCubemapData = stbi_loadf(cubemapPathName.c_str(), &width, &height, &nrComponents, 0);

        if (nrComponents == 3)
        {
            float* pExtentedData = new float[4 * width * height];
            SharedLib::Img3EleTo4Ele(pHdrImgCubemapData, pExtentedData, width * height);
            delete[] pHdrImgCubemapData;
            pHdrImgCubemapData = pExtentedData;
        }

        VkExtent3D extent{};
        {
            extent.width = width;
            extent.height = width;
            extent.depth = 1;
        }

        SharedLib::GpuImgCreateInfo backgroundCubemapInfo{};
        {
            backgroundCubemapInfo.allocFlags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
            backgroundCubemapInfo.hasSampler = true;
            backgroundCubemapInfo.imgCreateFlags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
            backgroundCubemapInfo.imgExtent = extent;
            backgroundCubemapInfo.imgFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
            backgroundCubemapInfo.imgSubresRange = GetImgSubrsrcRange(0, 1, 0, 6);
            backgroundCubemapInfo.imgUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            backgroundCubemapInfo.imgViewType = VK_IMAGE_VIEW_TYPE_CUBE;
            backgroundCubemapInfo.samplerInfo = samplerInfo;
        }

        m_hdrCubeMap = CreateGpuImage(backgroundCubemapInfo);

        VkBufferImageCopy backgroundBufToImgCopy{};
        {
            backgroundBufToImgCopy.bufferRowLength = width;
            backgroundBufToImgCopy.imageSubresource = GetImgSubrsrcLayers(0, 0, 6);
            backgroundBufToImgCopy.imageExtent = extent;
        }

        const uint32_t backgroundCubemapBytes = width * height * 4 * sizeof(float);

        SharedLib::SendImgDataToGpu(
            raiiCmdBuffer.m_cmdBuffer,
            m_device,
            m_graphicsQueue,
            pHdrImgCubemapData,
            backgroundCubemapBytes,
            m_hdrCubeMap.image,
            GetImgSubrsrcRange(0, 1, 0, 6),
            VK_IMAGE_LAYOUT_UNDEFINED,
            backgroundBufToImgCopy,
            *m_pAllocator
        );

        delete[] pHdrImgCubemapData;
    }
    
    // Read in and init diffuse irradiance cubemap
    {
        std::string diffIrradiancePathName = hdriFilePath + "iblOutput/diffuse_irradiance_cubemap.hdr";
        int width, height, nrComponents;
        float* pDiffuseIrradianceCubemapImgInfoData = stbi_loadf(diffIrradiancePathName.c_str(),
                                                                 &width, &height, &nrComponents, 0);

        if (nrComponents == 3)
        {
            float* pExtentedData = new float[4 * width * height];
            SharedLib::Img3EleTo4Ele(pDiffuseIrradianceCubemapImgInfoData, pExtentedData, width * height);
            delete[] pDiffuseIrradianceCubemapImgInfoData;
            pDiffuseIrradianceCubemapImgInfoData = pExtentedData;
        }

        VkExtent3D extent{};
        {
            extent.width = width;
            extent.height = width;
            extent.depth = 1;
        }

        SharedLib::GpuImgCreateInfo diffIrradianceInfo{};
        {
            diffIrradianceInfo.allocFlags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
            diffIrradianceInfo.hasSampler = true;
            diffIrradianceInfo.imgCreateFlags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
            diffIrradianceInfo.imgExtent = extent;
            diffIrradianceInfo.imgFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
            diffIrradianceInfo.imgSubresRange = GetImgSubrsrcRange(0, 1, 0, 6);
            diffIrradianceInfo.imgUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            diffIrradianceInfo.imgViewType = VK_IMAGE_VIEW_TYPE_CUBE;
            diffIrradianceInfo.samplerInfo = samplerInfo;
        }

        m_diffuseIrradianceCubemap = CreateGpuImage(diffIrradianceInfo);

        VkBufferImageCopy diffIrradianceBufToImgCopy{};
        {
            diffIrradianceBufToImgCopy.bufferRowLength = width;
            diffIrradianceBufToImgCopy.imageSubresource = GetImgSubrsrcLayers(0, 0, 6);
            diffIrradianceBufToImgCopy.imageExtent = extent;
        }

        const uint32_t diffIrradianceCubemapBytes = width * height * 4 * sizeof(float);

        SharedLib::SendImgDataToGpu(
            raiiCmdBuffer.m_cmdBuffer,
            m_device,
            m_graphicsQueue,
            pDiffuseIrradianceCubemapImgInfoData,
            diffIrradianceCubemapBytes,
            m_diffuseIrradianceCubemap.image,
            GetImgSubrsrcRange(0, 1, 0, 6),
            VK_IMAGE_LAYOUT_UNDEFINED,
            diffIrradianceBufToImgCopy,
            *m_pAllocator
        );

        delete[] pDiffuseIrradianceCubemapImgInfoData;
    }

    // Read in and init prefilter environment cubemap
    {
        std::string prefilterEnvPath = hdriFilePath + "iblOutput/prefilterEnvMaps/";
        std::vector<std::string> mipImgNames;
        SharedLib::GetAllFileNames(prefilterEnvPath, mipImgNames);

        m_prefilterEnvMipsCnt = mipImgNames.size();

        for (uint32_t i = 0; i < m_prefilterEnvMipsCnt; i++)
        {
            int width, height, nrComponents;
            std::string prefilterEnvMipImgPathName = hdriFilePath +
                "iblOutput/prefilterEnvMaps/prefilterMip" +
                std::to_string(i) + ".hdr";

            float* pPrefilterEnvCubemapImgsMipIData = stbi_loadf(prefilterEnvMipImgPathName.c_str(),
                &width, &height, &nrComponents, 0);

            VkExtent3D extent{};
            {
                extent.width = width;
                extent.height = width;
                extent.depth = 1;
            }

            if (nrComponents == 3)
            {
                float* pExtentedData = new float[4 * width * height];
                SharedLib::Img3EleTo4Ele(pPrefilterEnvCubemapImgsMipIData, pExtentedData, width * height);
                delete[] pPrefilterEnvCubemapImgsMipIData;
                pPrefilterEnvCubemapImgsMipIData = pExtentedData;
            }

            if (i == 0)
            {
                // Create the gpu image after the first mipmap read.
                SharedLib::GpuImgCreateInfo prefilterEnvInfo{};
                {
                    prefilterEnvInfo.allocFlags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
                    prefilterEnvInfo.hasSampler = true;
                    prefilterEnvInfo.imgCreateFlags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
                    prefilterEnvInfo.imgExtent = extent;
                    prefilterEnvInfo.imgFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
                    prefilterEnvInfo.imgSubresRange = GetImgSubrsrcRange(0, m_prefilterEnvMipsCnt, 0, 6);
                    prefilterEnvInfo.imgUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
                    prefilterEnvInfo.imgViewType = VK_IMAGE_VIEW_TYPE_CUBE;
                    prefilterEnvInfo.samplerInfo = samplerInfo;
                }

                m_prefilterEnvCubemap = CreateGpuImage(prefilterEnvInfo);
            }

            VkBufferImageCopy prefilterEnvBufToImgCopy{};
            {
                prefilterEnvBufToImgCopy.bufferRowLength = width;
                prefilterEnvBufToImgCopy.imageSubresource = GetImgSubrsrcLayers(i, 0, 6);
                prefilterEnvBufToImgCopy.imageExtent = extent;
            }

            const uint32_t prefilterEnvCubemapBytes = width * height * 4 * sizeof(float);

            SharedLib::SendImgDataToGpu(
                raiiCmdBuffer.m_cmdBuffer,
                m_device,
                m_graphicsQueue,
                pPrefilterEnvCubemapImgsMipIData,
                prefilterEnvCubemapBytes,
                m_prefilterEnvCubemap.image,
                GetImgSubrsrcRange(i, 1, 0, 6),
                VK_IMAGE_LAYOUT_UNDEFINED,
                prefilterEnvBufToImgCopy,
                *m_pAllocator
            );

            delete[] pPrefilterEnvCubemapImgsMipIData;
        }        
    }

    // Read in and init environment brdf map
    {
        std::string envBrdfMapPathName = hdriFilePath + "iblOutput/envBrdf.hdr";
        int width, height, nrComponents;
        float* pEnvBrdfImgInfoData = stbi_loadf(envBrdfMapPathName.c_str(), &width, &height, &nrComponents, 0);

        if (nrComponents == 3)
        {
            float* pExtentedData = new float[4 * width * height];
            SharedLib::Img3EleTo4Ele(pEnvBrdfImgInfoData, pExtentedData, width * height);
            delete[] pEnvBrdfImgInfoData;
            pEnvBrdfImgInfoData = pExtentedData;
        }

        VmaAllocationCreateInfo envBrdfMapAllocInfo{};
        {
            envBrdfMapAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            envBrdfMapAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        }

        VkExtent3D extent{};
        {
            extent.width = width;
            extent.height = height;
            extent.depth = 1;
        }

        // Create the gpu image after the first mipmap read.
        SharedLib::GpuImgCreateInfo envBrdfInfo{};
        {
            envBrdfInfo.allocFlags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
            envBrdfInfo.hasSampler = true;
            envBrdfInfo.imgExtent = extent;
            envBrdfInfo.imgFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
            envBrdfInfo.imgSubresRange = GetImgSubrsrcRange(0, 1, 0, 1);
            envBrdfInfo.imgUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            envBrdfInfo.imgViewType = VK_IMAGE_VIEW_TYPE_2D;
            envBrdfInfo.samplerInfo = samplerInfo;
        }

        m_envBrdfImg = CreateGpuImage(envBrdfInfo);
        
        VkBufferImageCopy envBrdfBufToImgCopy{};
        {
            envBrdfBufToImgCopy.bufferRowLength = width;
            envBrdfBufToImgCopy.imageSubresource = GetImgSubrsrcLayers(0, 0, 1);
            envBrdfBufToImgCopy.imageExtent = extent;
        }

        const uint32_t envBrdfBytes = width * height * 4 * sizeof(float);

        SharedLib::SendImgDataToGpu(
            raiiCmdBuffer.m_cmdBuffer,
            m_device,
            m_graphicsQueue,
            pEnvBrdfImgInfoData,
            envBrdfBytes,
            m_envBrdfImg.image,
            GetImgSubrsrcRange(0, 1, 0, 1),
            VK_IMAGE_LAYOUT_UNDEFINED,
            envBrdfBufToImgCopy,
            *m_pAllocator
        );

        delete[] pEnvBrdfImgInfoData;
    }
}

// ================================================================================================================
void PBRIBLApp::InitCameraUboObjects()
{
    // The alignment of a vec3 is 4 floats and the element alignment of a struct is the largest element alignment,
    // which is also the 4 float. Therefore, we need 16 floats as the buffer to store the Camera's parameters.
    m_cameraParaBuffers.resize(m_swapchainImgCnt);

    for (uint32_t i = 0; i < m_swapchainImgCnt; i++)
    {
        m_cameraParaBuffers[i] = CreateGpuBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                 VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
                                                 VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                                                 16 * sizeof(float));
    }
}

// ================================================================================================================
void PBRIBLApp::InitSkyboxPipelineLayout()
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
void PBRIBLApp::InitSkyboxShaderModules()
{
    // Create Shader Modules.
    m_vsSkyboxShaderModule = CreateShaderModule("./hlsl/skybox_vert.spv");
    m_psSkyboxShaderModule = CreateShaderModule("./hlsl/skybox_frag.spv");
}

// ================================================================================================================
void PBRIBLApp::InitSkyboxPipelineDescriptorSetLayout()
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
        skyboxPipelineDesSet0LayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
        skyboxPipelineDesSet0LayoutInfo.bindingCount = 2;
        skyboxPipelineDesSet0LayoutInfo.pBindings = skyboxPipelineDesSet0LayoutBindings;
    }
    
    VK_CHECK(vkCreateDescriptorSetLayout(m_device,
                                         &skyboxPipelineDesSet0LayoutInfo,
                                         nullptr,
                                         &m_skyboxPipelineDesSet0Layout));
}

// ================================================================================================================
void PBRIBLApp::InitSkyboxPipeline()
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
void PBRIBLApp::DestroySkyboxPipelineRes()
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
void PBRIBLApp::AppInit()
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
    const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    InitDevice(deviceExtensions, deviceQueueInfos, nullptr);
    InitVmaAllocator();
    InitGraphicsQueue();
    InitPresentQueue();
    InitGfxCommandPool();
    InitKHRFuncPtrs();
    InitSwapchain();  

    InitGfxCommandBuffers(m_swapchainImgCnt);
    
    InitSphereVertexIndexBuffers();
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
    InitSwapchainSyncObjects();
}

// ================================================================================================================
void PBRIBLApp::InitSphereVertexIndexBuffers()
{
    std::string inputfile = SOURCE_PATH;
    inputfile += "/../data/uvNormalSphere.obj";

    tinyobj::ObjReaderConfig readerConfig;
    tinyobj::ObjReader sphereObjReader;

    sphereObjReader.ParseFromFile(inputfile, readerConfig);

    auto& shapes = sphereObjReader.GetShapes();
    auto& attrib = sphereObjReader.GetAttrib();

    // We assume that this test only has one shape
    assert(shapes.size() == 1, "This application only accepts one shape!");

    for (uint32_t s = 0; s < shapes.size(); s++)
    {
        // Loop over faces(polygon)
        uint32_t index_offset = 0;
        uint32_t idxBufIdx = 0;
        for (uint32_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
        {
            uint32_t fv = shapes[s].mesh.num_face_vertices[f];

            // Loop over vertices in the face.
            for (uint32_t v = 0; v < fv; v++)
            {
                // Access to vertex
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

                float vx = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
                float vy = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
                float vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

                // Transfer the vertex buffer's vertex index to the element index -- 6 * vertex index + xxx;
                m_vertBufferData.push_back(vx);
                m_vertBufferData.push_back(vy);
                m_vertBufferData.push_back(vz);

                // Check if `normal_index` is zero or positive. negative = no normal data
                assert(idx.normal_index >= 0, "The model doesn't have normal information but it is necessary.");
                float nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
                float ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
                float nz = attrib.normals[3 * size_t(idx.normal_index) + 2];

                m_vertBufferData.push_back(nx);
                m_vertBufferData.push_back(ny);
                m_vertBufferData.push_back(nz);

                m_idxBufferData.push_back(idxBufIdx);
                idxBufIdx++;
            }
            index_offset += fv;
        }
    }

    const uint32_t vertBufferByteCnt = m_vertBufferData.size() * sizeof(float);
    const uint32_t idxBufferByteCnt = m_idxBufferData.size() * sizeof(uint32_t);

    // Create sphere data GPU buffers
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
        &m_vertBuffer,
        &m_vertBufferAlloc,
        nullptr);

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
        &m_idxBuffer,
        &m_idxBufferAlloc,
        nullptr);

    // Send sphere data to the GPU buffers
    CopyRamDataToGpuBuffer(m_vertBufferData.data(), m_vertBuffer, m_vertBufferAlloc, vertBufferByteCnt);
    CopyRamDataToGpuBuffer(m_idxBufferData.data(), m_idxBuffer, m_idxBufferAlloc, idxBufferByteCnt);
}

// ================================================================================================================
void PBRIBLApp::DestroySphereVertexIndexBuffers()
{
    vmaDestroyBuffer(*m_pAllocator, m_vertBuffer, m_vertBufferAlloc);
    vmaDestroyBuffer(*m_pAllocator, m_idxBuffer, m_idxBufferAlloc);
}

// ================================================================================================================
void PBRIBLApp::InitIblPipeline()
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
void PBRIBLApp::InitIblPipelineDescriptorSetLayout()
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
        pipelineDesSetLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
        pipelineDesSetLayoutInfo.bindingCount = 4;
        pipelineDesSetLayoutInfo.pBindings = pipelineDesSetLayoutBindings;
    }

    VK_CHECK(vkCreateDescriptorSetLayout(m_device,
                                         &pipelineDesSetLayoutInfo,
                                         nullptr,
                                         &m_iblPipelineDesSet0Layout));
}

// ================================================================================================================
void PBRIBLApp::InitIblPipelineLayout()
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
void PBRIBLApp::InitIblShaderModules()
{
    m_vsIblShaderModule = CreateShaderModule("./hlsl/ibl_vert.spv");
    m_psIblShaderModule = CreateShaderModule("./hlsl/ibl_frag.spv");
}

// ================================================================================================================
void PBRIBLApp::DestroyIblPipelineRes()
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
void PBRIBLApp::InitVpMatBuffer()
{
    m_vpMatUboBuffer.resize(m_swapchainImgCnt);

    float vpMatData[16] = {};
    float tmpViewMatData[16] = {};
    float tmpPersMatData[16] = {};
    m_pCamera->GenViewPerspectiveMatrices(tmpViewMatData, tmpPersMatData, vpMatData);
    SharedLib::MatTranspose(vpMatData, 4);

    for (uint32_t i = 0; i < m_swapchainImgCnt; i++)
    {
        m_vpMatUboBuffer[i] = CreateGpuBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                        16 * sizeof(float));

        CopyRamDataToGpuBuffer(vpMatData,
                               m_vpMatUboBuffer[i].buffer,
                               m_vpMatUboBuffer[i].bufferAlloc,
                               sizeof(vpMatData));
    }
}

// ================================================================================================================
void PBRIBLApp::DestroyVpMatBuffer()
{
    for (uint32_t i = 0; i < m_swapchainImgCnt; i++)
    {
        DestroyGpuBufferResource(m_vpMatUboBuffer[i]);
    }
}

// ================================================================================================================
VkPipelineVertexInputStateCreateInfo PBRIBLApp::CreatePipelineVertexInputInfo()
{
    // Specifying all kinds of pipeline states
    // Vertex input state
    VkVertexInputBindingDescription* pVertBindingDesc = new VkVertexInputBindingDescription();
    memset(pVertBindingDesc, 0, sizeof(VkVertexInputBindingDescription));
    {
        pVertBindingDesc->binding = 0;
        pVertBindingDesc->stride = 6 * sizeof(float);
        pVertBindingDesc->inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    }
    m_heapMemPtrVec.push_back(pVertBindingDesc);

    VkVertexInputAttributeDescription* pVertAttrDescs = new VkVertexInputAttributeDescription[2];
    memset(pVertAttrDescs, 0, sizeof(VkVertexInputAttributeDescription) * 2);
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
    }
    m_heapArrayMemPtrVec.push_back(pVertAttrDescs);

    VkPipelineVertexInputStateCreateInfo vertInputInfo{};
    {
        vertInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertInputInfo.pNext = nullptr;
        vertInputInfo.vertexBindingDescriptionCount = 1;
        vertInputInfo.pVertexBindingDescriptions = pVertBindingDesc;
        vertInputInfo.vertexAttributeDescriptionCount = 2;
        vertInputInfo.pVertexAttributeDescriptions = pVertAttrDescs;
    }

    return vertInputInfo;
}

// ================================================================================================================
VkPipelineDepthStencilStateCreateInfo PBRIBLApp::CreateDepthStencilStateInfo()
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

// ================================================================================================================
void PBRIBLApp::CmdPushSkyboxDescriptors(
    VkCommandBuffer cmdBuffer)
{
    std::vector<VkWriteDescriptorSet> skyboxDescriptorsInfos;

    // Set 0 -- Binding 0: The skybox cubemap
    VkWriteDescriptorSet writeHdrDesSet{};
    {
        writeHdrDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeHdrDesSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeHdrDesSet.dstBinding = 0;
        writeHdrDesSet.pImageInfo = &m_hdrCubeMap.imageDescInfo;
        writeHdrDesSet.descriptorCount = 1;
    }
    skyboxDescriptorsInfos.push_back(writeHdrDesSet);

    // Set 0 -- Binding 1: The cubemap render camera buffer
    VkWriteDescriptorSet writeCameraBufDesSet{};
    {
        writeCameraBufDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeCameraBufDesSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeCameraBufDesSet.dstBinding = 1;
        writeCameraBufDesSet.descriptorCount = 1;
        writeCameraBufDesSet.pBufferInfo = &m_cameraParaBuffers[m_acqSwapchainImgIdx].bufferDescInfo;
    }
    skyboxDescriptorsInfos.push_back(writeCameraBufDesSet);

    m_vkCmdPushDescriptorSetKHR(cmdBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_skyboxPipelineLayout,
        0, skyboxDescriptorsInfos.size(), skyboxDescriptorsInfos.data());
}

// ================================================================================================================
// TODO: Push descriptors can be more automatic.
void PBRIBLApp::CmdPushSphereIBLDescriptors(
    VkCommandBuffer cmdBuffer)
{
    std::vector<VkWriteDescriptorSet> iblRenderDescriptorSet0Infos;

    // Descriptor set 0 infos.
    VkWriteDescriptorSet writeIblMvpMatUboBufDesSet{};
    {
        writeIblMvpMatUboBufDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeIblMvpMatUboBufDesSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeIblMvpMatUboBufDesSet.dstBinding = 0;
        writeIblMvpMatUboBufDesSet.descriptorCount = 1;
        writeIblMvpMatUboBufDesSet.pBufferInfo = &m_vpMatUboBuffer[m_acqSwapchainImgIdx].bufferDescInfo;
    }
    iblRenderDescriptorSet0Infos.push_back(writeIblMvpMatUboBufDesSet);

    VkWriteDescriptorSet writeDiffIrrDesSet{};
    {
        writeDiffIrrDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDiffIrrDesSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeDiffIrrDesSet.dstBinding = 1;
        writeDiffIrrDesSet.pImageInfo = &m_diffuseIrradianceCubemap.imageDescInfo;
        writeDiffIrrDesSet.descriptorCount = 1;
    }
    iblRenderDescriptorSet0Infos.push_back(writeDiffIrrDesSet);

    VkWriteDescriptorSet writePrefilterEnvDesSet{};
    {
        writePrefilterEnvDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writePrefilterEnvDesSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writePrefilterEnvDesSet.dstBinding = 2;
        writePrefilterEnvDesSet.pImageInfo = &m_prefilterEnvCubemap.imageDescInfo;
        writePrefilterEnvDesSet.descriptorCount = 1;
    }
    iblRenderDescriptorSet0Infos.push_back(writePrefilterEnvDesSet);

    VkWriteDescriptorSet writeEnvBrdfDesSet{};
    {
        writeEnvBrdfDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeEnvBrdfDesSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeEnvBrdfDesSet.dstBinding = 3;
        writeEnvBrdfDesSet.pImageInfo = &m_envBrdfImg.imageDescInfo;
        writeEnvBrdfDesSet.descriptorCount = 1;
    }
    iblRenderDescriptorSet0Infos.push_back(writeEnvBrdfDesSet);

    // Push decriptors
    m_vkCmdPushDescriptorSetKHR(cmdBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_iblPipelineLayout,
                                0, iblRenderDescriptorSet0Infos.size(), iblRenderDescriptorSet0Infos.data());
}