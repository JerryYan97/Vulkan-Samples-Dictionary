#include "SkeletonSkinAnim.h"
#include <glfw3.h>
#include "../../../SharedLibrary/Utils/VulkanDbgUtils.h"
#include "../../../SharedLibrary/Camera/Camera.h"
#include "../../../SharedLibrary/Event/Event.h"
#include "../../../SharedLibrary/Utils/StrPathUtils.h"
#include "../../../SharedLibrary/Utils/DiskOpsUtils.h"
#include "../../../SharedLibrary/Utils/CmdBufUtils.h"
#include "../../../SharedLibrary/Utils/AppUtils.h"

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
SkinAnimGltfApp::SkinAnimGltfApp(
    const std::string& iblPath,
    const std::string& gltfPathName) :
    GlfwApplication(),
    m_vsSkinAnimShaderModule(VK_NULL_HANDLE),
    m_psSkinAnimShaderModule(VK_NULL_HANDLE),
    m_skinAnimPipelineLayout(VK_NULL_HANDLE),
    m_skinAnimPipeline(),
    m_diffuseIrradianceCubemap(),
    m_prefilterEnvCubemap(),
    m_envBrdfImg(),
    m_currentRadians(0.f),
    m_isFirstTimeRecord(true),
    m_lastTime(),
    m_skinAnimPipelineDesSetLayout(VK_NULL_HANDLE),
    m_skeletalMesh(),
    m_currentAnimTime(0.f),
    m_iblDir(iblPath),
    m_gltfPathName(gltfPathName)
{
    m_pCamera = new SharedLib::Camera();
    
    float cameraStartPos[3] = {-Radius, 0.f, 0.f};
    m_pCamera->SetPos(cameraStartPos);

    float cameraStartView[3] = {1.f, 0.f, 0.f};
    m_pCamera->SetView(cameraStartView);
}

// ================================================================================================================
SkinAnimGltfApp::~SkinAnimGltfApp()
{
    vkDeviceWaitIdle(m_device);
    delete m_pCamera;

    DestroyHdrRenderObjs();

    DestroySkinAnimPipelineRes();
}

// ================================================================================================================
void SkinAnimGltfApp::DestroyHdrRenderObjs()
{
    DestroyGltf();

    delete m_diffuseIrradianceCubemap.pData[0];
    vmaDestroyImage(*m_pAllocator,
                    m_diffuseIrradianceCubemap.gpuImg.image,
                    m_diffuseIrradianceCubemap.gpuImg.imageAllocation);

    vkDestroyImageView(m_device, m_diffuseIrradianceCubemap.gpuImg.imageView, nullptr);
    vkDestroySampler(m_device, m_diffuseIrradianceCubemap.gpuImg.imageSampler, nullptr);

    for (auto itr : m_prefilterEnvCubemap.pData)
    {
        delete itr;
    }
    vmaDestroyImage(*m_pAllocator,
                    m_prefilterEnvCubemap.gpuImg.image,
                    m_prefilterEnvCubemap.gpuImg.imageAllocation);

    vkDestroyImageView(m_device, m_prefilterEnvCubemap.gpuImg.imageView, nullptr);
    vkDestroySampler(m_device, m_prefilterEnvCubemap.gpuImg.imageSampler, nullptr);

    delete m_envBrdfImg.pData[0];
    vmaDestroyImage(*m_pAllocator, m_envBrdfImg.gpuImg.image, m_envBrdfImg.gpuImg.imageAllocation);
    vkDestroyImageView(m_device, m_envBrdfImg.gpuImg.imageView, nullptr);
    vkDestroySampler(m_device, m_envBrdfImg.gpuImg.imageSampler, nullptr);
}

// ================================================================================================================
/*
void SkinAnimGltfApp::SendCameraDataToBuffer(
    uint32_t i)
{
    float cameraData[16] = {};
    m_pCamera->GetView(cameraData);
    m_pCamera->GetRight(&cameraData[4]);
    m_pCamera->GetUp(&cameraData[8]);

    m_pCamera->GetNearPlane(cameraData[12], cameraData[13], cameraData[11]);

    float iblMvpMatsData[32] = {};
    float modelMatData[16] = {
        1.f, 0.f, 0.f, ModelWorldPos[0],
        0.f, 1.f, 0.f, ModelWorldPos[1],
        0.f, 0.f, 1.f, ModelWorldPos[2],
        0.f, 0.f, 0.f, 1.f
    };
    memcpy(iblMvpMatsData, modelMatData, sizeof(modelMatData));

    float vpMatData[16] = {};
    float tmpViewMat[16] = {};
    float tmpPersMat[16] = {};
    m_pCamera->GenViewPerspectiveMatrices(tmpViewMat, tmpPersMat, vpMatData);
    memcpy(&iblMvpMatsData[16], vpMatData, sizeof(vpMatData));

    SharedLib::MatTranspose(vpMatData, 4);

    VkExtent2D swapchainImgExtent = GetSwapchainImageExtent();
    cameraData[14] = swapchainImgExtent.width;
    cameraData[15] = swapchainImgExtent.height;

    CopyRamDataToGpuBuffer(cameraData, m_cameraParaBuffers[i], m_cameraParaBufferAllocs[i], sizeof(cameraData));
    CopyRamDataToGpuBuffer(vpMatData, m_vpMatUboBuffer[i], m_vpMatUboAlloc[i], sizeof(vpMatData));
    CopyRamDataToGpuBuffer(iblMvpMatsData,
                           m_iblMvpMatsUboBuffer[i],
                           m_iblMvpMatsUboAlloc[i],
                           sizeof(iblMvpMatsData));
}
*/

// ================================================================================================================
void SkinAnimGltfApp::UpdateCameraAndGpuBuffer()
{
    // TODO: Delete the mouse event.
    SharedLib::HEvent midMouseDownEvent = CreateMiddleMouseEvent(g_isDown);
    m_pCamera->OnEvent(midMouseDownEvent);
    
    // Animation
    if (m_isFirstTimeRecord)
    {
        m_lastTime = std::chrono::high_resolution_clock::now();
        m_isFirstTimeRecord = false;
    }

    auto thisTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(thisTime - m_lastTime);

    float delta = (float)duration.count() / 1000.f; // Delta is in second.
    float deltaRadians = delta * RotateRadiensPerSecond;

    m_currentRadians += deltaRadians;
    if (m_currentRadians > 3.1415926 * 2.f)
    {
        m_currentRadians -= 3.1415926 * 2.f;
    }

    float newCameraPos[3] = {
        -cosf(m_currentRadians) * Radius, 0.f, sinf(m_currentRadians) * Radius
    };

    float newCameraView[3] = {
        -newCameraPos[0], -newCameraPos[1], -newCameraPos[2]
    };

    m_pCamera->SetPos(newCameraPos);
    m_pCamera->SetView(newCameraView);

    m_lastTime = thisTime;
}

// ================================================================================================================
void SkinAnimGltfApp::GetCameraPos(
    float* pOut)
{
    m_pCamera->GetPos(pOut);
}

// ================================================================================================================
void SkinAnimGltfApp::ReadInInitIBL()
{
    SharedLib::RAIICommandBuffer raiiCmdBuffer(m_gfxCmdPool, m_device);

    // Load the IBL images into RAM and create gpu objects for them.
    std::string hdriFilePath = SOURCE_PATH;
    hdriFilePath += "/../data/";

    // Read in and init diffuse irradiance cubemap
    {
        std::string diffIrradiancePathName = hdriFilePath + "ibl/diffuse_irradiance_cubemap.hdr";
        int width, height, nrComponents;
        m_diffuseIrradianceCubemap.pData.push_back(stbi_loadf(diffIrradiancePathName.c_str(),
                                                             &width, &height, &nrComponents, 0));

        m_diffuseIrradianceCubemap.pixWidths.push_back(width);
        m_diffuseIrradianceCubemap.pixHeights.push_back(height);
        m_diffuseIrradianceCubemap.componentCnt = nrComponents;

        if (nrComponents == 3)
        {
            float* pNewData = new float[4 * width * height];
            SharedLib::Img3EleTo4Ele(m_diffuseIrradianceCubemap.pData[0], pNewData, width * height);
            delete[] m_diffuseIrradianceCubemap.pData[0];
            m_diffuseIrradianceCubemap.pData[0] = pNewData;
            m_diffuseIrradianceCubemap.componentCnt = 4;
        }

        VkImageSubresourceRange cubemapMip1SubResRange{};
        {
            cubemapMip1SubResRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            cubemapMip1SubResRange.baseMipLevel = 0;
            cubemapMip1SubResRange.levelCount = 1;
            cubemapMip1SubResRange.baseArrayLayer = 0;
            cubemapMip1SubResRange.layerCount = 6;
        }

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

        SharedLib::GpuImgCreateInfo gpuImgCreateInfo{};
        {
            gpuImgCreateInfo.allocFlags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
            gpuImgCreateInfo.hasSampler = true;
            gpuImgCreateInfo.imgSubresRange = cubemapMip1SubResRange;
            gpuImgCreateInfo.imgUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            gpuImgCreateInfo.imgViewType = VK_IMAGE_VIEW_TYPE_CUBE;
            gpuImgCreateInfo.samplerInfo = samplerInfo;
            gpuImgCreateInfo.imgExtent = VkExtent3D{ m_diffuseIrradianceCubemap.pixWidths[0],
                                                     m_diffuseIrradianceCubemap.pixWidths[0], 1 };
            gpuImgCreateInfo.imgFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
            gpuImgCreateInfo.imgCreateFlags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        }

        m_diffuseIrradianceCubemap.gpuImg = CreateGpuImage(gpuImgCreateInfo);

        // Send data to gpu diffuse irradiance cubemap
        VkBufferImageCopy diffIrrBufToImgCopy{};
        {
            VkExtent3D extent{};
            {
                extent.width = m_diffuseIrradianceCubemap.pixWidths[0];
                extent.height = m_diffuseIrradianceCubemap.pixWidths[0];
                extent.depth = 1;
            }

            diffIrrBufToImgCopy.bufferRowLength = extent.width;
            diffIrrBufToImgCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            diffIrrBufToImgCopy.imageSubresource.mipLevel = 0;
            diffIrrBufToImgCopy.imageSubresource.baseArrayLayer = 0;
            diffIrrBufToImgCopy.imageSubresource.layerCount = 6;
            diffIrrBufToImgCopy.imageExtent = extent;
        }

        const uint32_t diffIrrDwords = m_diffuseIrradianceCubemap.pixWidths[0] * 
                                       m_diffuseIrradianceCubemap.pixHeights[0] * 4;

        SharedLib::SendImgDataToGpu(
            raiiCmdBuffer.m_cmdBuffer,
            m_device,
            m_graphicsQueue,
            m_diffuseIrradianceCubemap.pData[0],
            diffIrrDwords * sizeof(float),
            m_diffuseIrradianceCubemap.gpuImg.image,
            cubemapMip1SubResRange,
            VK_IMAGE_LAYOUT_UNDEFINED,
            diffIrrBufToImgCopy,
            *m_pAllocator
        );
    }

    // Read in and init prefilter environment cubemap
    {
        std::string prefilterEnvPath = hdriFilePath + "ibl/prefilterEnvMaps/";
        std::vector<std::string> mipImgNames;
        SharedLib::GetAllFileNames(prefilterEnvPath, mipImgNames);

        const uint32_t mipCnts = mipImgNames.size();

        for (uint32_t i = 0; i < mipCnts; i++)
        {
            int width, height, nrComponents;
            std::string prefilterEnvMipImgPathName = prefilterEnvPath + "prefilterMip" + std::to_string(i) + ".hdr";

            m_prefilterEnvCubemap.pData.push_back(stbi_loadf(prefilterEnvMipImgPathName.c_str(),
                                                             &width, &height, &nrComponents, 0));
            m_prefilterEnvCubemap.pixWidths.push_back(width);
            m_prefilterEnvCubemap.pixHeights.push_back(height);

            if (nrComponents == 3)
            {
                float* pNewData = new float[4 * width * height];
                SharedLib::Img3EleTo4Ele(m_prefilterEnvCubemap.pData[i], pNewData, width * height);
                delete[] m_prefilterEnvCubemap.pData[i];
                m_prefilterEnvCubemap.pData[i] = pNewData;
            }
            m_prefilterEnvCubemap.componentCnt = 4;
        }

        VkImageSubresourceRange cubemapMipCntSubResRange{};
        {
            cubemapMipCntSubResRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            cubemapMipCntSubResRange.baseMipLevel = 0;
            cubemapMipCntSubResRange.levelCount = mipCnts;
            cubemapMipCntSubResRange.baseArrayLayer = 0;
            cubemapMipCntSubResRange.layerCount = 6;
        }

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

        SharedLib::GpuImgCreateInfo gpuImgCreateInfo{};
        {
            gpuImgCreateInfo.allocFlags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
            gpuImgCreateInfo.hasSampler = true;
            gpuImgCreateInfo.imgSubresRange = cubemapMipCntSubResRange;
            gpuImgCreateInfo.imgUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            gpuImgCreateInfo.imgViewType = VK_IMAGE_VIEW_TYPE_CUBE;
            gpuImgCreateInfo.samplerInfo = samplerInfo;
            gpuImgCreateInfo.imgExtent = VkExtent3D{ m_prefilterEnvCubemap.pixWidths[0],
                                                     m_prefilterEnvCubemap.pixWidths[0], 1 };
            gpuImgCreateInfo.imgFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
            gpuImgCreateInfo.imgCreateFlags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        }

        m_prefilterEnvCubemap.gpuImg = CreateGpuImage(gpuImgCreateInfo);

        // Send data to gpu prefilter environment map
        for (uint32_t i = 0; i < mipCnts; i++)
        {
            uint32_t mipDwordsCnt = 4 * m_prefilterEnvCubemap.pixWidths[i] * m_prefilterEnvCubemap.pixHeights[i];
            VkImageSubresourceRange prefilterEnvMipISubResRange{};
            {
                prefilterEnvMipISubResRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                prefilterEnvMipISubResRange.baseMipLevel = i;
                prefilterEnvMipISubResRange.levelCount = 1;
                prefilterEnvMipISubResRange.baseArrayLayer = 0;
                prefilterEnvMipISubResRange.layerCount = 6;
            }

            VkBufferImageCopy prefilterEnvMipIBufToImgCopy{};
            {
                VkExtent3D extent{};
                {
                    extent.width = m_prefilterEnvCubemap.pixWidths[i];
                    extent.height = m_prefilterEnvCubemap.pixWidths[i];
                    extent.depth = 1;
                }

                prefilterEnvMipIBufToImgCopy.bufferRowLength = m_prefilterEnvCubemap.pixWidths[i];
                prefilterEnvMipIBufToImgCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                prefilterEnvMipIBufToImgCopy.imageSubresource.mipLevel = i;
                prefilterEnvMipIBufToImgCopy.imageSubresource.baseArrayLayer = 0;
                prefilterEnvMipIBufToImgCopy.imageSubresource.layerCount = 6;
                prefilterEnvMipIBufToImgCopy.imageExtent = extent;
            }

            SharedLib::SendImgDataToGpu(raiiCmdBuffer.m_cmdBuffer,
                                        m_device,
                                        m_graphicsQueue,
                                        m_prefilterEnvCubemap.pData[i],
                                        mipDwordsCnt * sizeof(float),
                                        m_prefilterEnvCubemap.gpuImg.image,
                                        prefilterEnvMipISubResRange,
                                        VK_IMAGE_LAYOUT_UNDEFINED,
                                        prefilterEnvMipIBufToImgCopy,
                                        *m_pAllocator);
        }
    }

    // Read in and init environment brdf map
    {
        std::string envBrdfMapPathName = hdriFilePath + "ibl/envBrdf.hdr";
        int width, height, nrComponents;
        m_envBrdfImg.pData.push_back(stbi_loadf(envBrdfMapPathName.c_str(), &width, &height, &nrComponents, 0));
        m_envBrdfImg.pixWidths.push_back(width);
        m_envBrdfImg.pixHeights.push_back(height);

        if (nrComponents == 3)
        {
            float* pNewData = new float[4 * width * height];
            SharedLib::Img3EleTo4Ele(m_envBrdfImg.pData[0], pNewData, width * height);
            delete[] m_envBrdfImg.pData[0];
            m_envBrdfImg.pData[0] = pNewData;
        }
        m_envBrdfImg.componentCnt = 4;

        // The envBrdf 2D texture SubresourceRange
        VkImageSubresourceRange tex2dSubResRange{};
        {
            tex2dSubResRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            tex2dSubResRange.baseMipLevel = 0;
            tex2dSubResRange.levelCount = 1;
            tex2dSubResRange.baseArrayLayer = 0;
            tex2dSubResRange.layerCount = 1;
        }

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

        SharedLib::GpuImgCreateInfo gpuImgCreateInfo{};
        {
            gpuImgCreateInfo.allocFlags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
            gpuImgCreateInfo.hasSampler = true;
            gpuImgCreateInfo.imgSubresRange = tex2dSubResRange;
            gpuImgCreateInfo.imgUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            gpuImgCreateInfo.imgViewType = VK_IMAGE_VIEW_TYPE_2D;
            gpuImgCreateInfo.samplerInfo = samplerInfo;
            gpuImgCreateInfo.imgExtent = VkExtent3D{ (uint32_t)width, (uint32_t)height, 1 };
            gpuImgCreateInfo.imgFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
        }

        m_envBrdfImg.gpuImg = CreateGpuImage(gpuImgCreateInfo);

        const uint32_t envBrdfDwordsCnt = 4 * m_envBrdfImg.pixHeights[0] * m_envBrdfImg.pixWidths[0];

        VkBufferImageCopy envBrdfBufToImgCopy{};
        {
            VkExtent3D extent{};
            {
                extent.width = m_envBrdfImg.pixWidths[0];
                extent.height = m_envBrdfImg.pixWidths[0];
                extent.depth = 1;
            }

            envBrdfBufToImgCopy.bufferRowLength = extent.width;
            envBrdfBufToImgCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            envBrdfBufToImgCopy.imageSubresource.mipLevel = 0;
            envBrdfBufToImgCopy.imageSubresource.baseArrayLayer = 0;
            envBrdfBufToImgCopy.imageSubresource.layerCount = 1;
            envBrdfBufToImgCopy.imageExtent = extent;
        }

        SharedLib::SendImgDataToGpu(raiiCmdBuffer.m_cmdBuffer,
            m_device,
            m_graphicsQueue,
            m_envBrdfImg.pData[0],
            envBrdfDwordsCnt * sizeof(float),
            m_envBrdfImg.gpuImg.image,
            tex2dSubResRange,
            VK_IMAGE_LAYOUT_UNDEFINED,
            envBrdfBufToImgCopy,
            *m_pAllocator);
    }
}

// ================================================================================================================
void SkinAnimGltfApp::AppInit()
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
    InitKHRFuncPtrs();

    InitGfxCommandPool();
    InitGfxCommandBuffers(SharedLib::MAX_FRAMES_IN_FLIGHT);

    InitSwapchain();
    ReadInInitGltf();
    ReadInInitIBL();

    InitSkinAnimShaderModules();
    InitSkinAnimPipelineDescriptorSetLayout();
    InitSkinAnimPipelineLayout();
    InitSkinAnimPipeline();
    
    InitSwapchainSyncObjects();
}

// ================================================================================================================
// * Animation, skeleton, 1 mesh.
// * We only support triangle.
// * Texture samplers' type should follow the real data, but here we simply choose the repeat.
// * https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#accessor-data-types
// * TinyGltf has custom gltf type macros: https://github.com/syoyo/tinygltf/blob/release/tiny_gltf.h#L144-L152
// * A buffer view represents a contiguous segment of data in a buffer, defined by a byte offset into the buffer
//   specified in the byteOffset property and a total byte length specified by the byteLength property of the buffer
//   view.
void SkinAnimGltfApp::ReadInInitGltf()
{
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, m_gltfPathName);
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

    // This example only supports gltf that only has one mesh and one skin.
    assert(model.meshes.size() == 1, "This example only supports one mesh.");
    assert(model.skins.size() == 1, "This example only supports one skin.");

    // Load mesh and relevant info    
    const auto& mesh = model.meshes[0];


    // Load pos
    std::vector<float> vertPos;
    int posIdx = mesh.primitives[0].attributes.at("POSITION");
    const auto& posAccessor = model.accessors[posIdx];

    assert(posAccessor.componentType == TINYGLTF_PARAMETER_TYPE_FLOAT, "The pos accessor data type should be float.");
    assert(posAccessor.type == TINYGLTF_TYPE_VEC3, "The pos accessor type should be vec3.");

    int posAccessorByteOffset = posAccessor.byteOffset;
    int posAccessorEleCnt = posAccessor.count; // Assume a position element is a float3.
    const auto& posBufferView = model.bufferViews[posAccessor.bufferView];
    // Assmue the data and element type of the position is float3
    int posBufferOffset = posAccessorByteOffset + posBufferView.byteOffset;
    int posBufferByteCnt = sizeof(float) * 3 * posAccessor.count;
    vertPos.resize(3 * posAccessor.count);
    memcpy(vertPos.data(), &pBufferData[posBufferOffset], posBufferByteCnt);


    // Load indices
    std::vector<uint16_t> vertIdx;
    int indicesIdx = mesh.primitives[0].indices;
    const auto& idxAccessor = model.accessors[indicesIdx];

    assert(idxAccessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT, "The idx accessor data type should be uint16.");
    assert(idxAccessor.type == TINYGLTF_TYPE_SCALAR, "The idx accessor type should be scalar.");

    int idxAccessorByteOffset = idxAccessor.byteOffset;
    int idxAccessorEleCnt = idxAccessor.count;
    const auto& idxBufferView = model.bufferViews[idxAccessor.bufferView];

    int idxBufferOffset = idxAccessorByteOffset + idxBufferView.byteOffset;
    int idxBufferByteCnt = sizeof(uint16_t) * idxAccessorEleCnt;
    vertIdx.resize(idxAccessorEleCnt);
    memcpy(vertIdx.data(), &pBufferData[idxBufferOffset], idxBufferByteCnt);


    // Load normal
    int normalIdx = -1;
    std::vector<float> vertNormal;
    if (mesh.primitives[0].attributes.count("NORMAL") > 0)
    {
        normalIdx = mesh.primitives[0].attributes.at("NORMAL");
        const auto& normalAccessor = model.accessors[normalIdx];

        assert(normalAccessor.componentType == TINYGLTF_PARAMETER_TYPE_FLOAT, "The normal accessor data type should be float.");
        assert(normalAccessor.type == TINYGLTF_TYPE_VEC3, "The normal accessor type should be vec3.");

        int normalAccessorByteOffset = normalAccessor.byteOffset;
        int normalAccessorEleCnt = normalAccessor.count;
        const auto& normalBufferView = model.bufferViews[normalAccessor.bufferView];

        int normalBufferOffset = normalAccessorByteOffset + normalBufferView.byteOffset;
        int normalBufferByteCnt = sizeof(float) * 3 * normalAccessorEleCnt;
        
        vertNormal.resize(3 * normalAccessorEleCnt);
        memcpy(vertNormal.data(), &pBufferData[normalBufferOffset], normalBufferByteCnt);
    }
    else
    {
        // If we don't have any normal geo data, then we will just apply the first triangle's normal to all the other
        // triangles/vertices.
        uint16_t idx0 = vertIdx[0];
        float vertPos0[3] = {vertPos[3 * idx0], vertPos[3 * idx0 + 1], vertPos[3 * idx0 + 2]};

        uint16_t idx1 = vertIdx[1];
        float vertPos1[3] = {vertPos[3 * idx1], vertPos[3 * idx1 + 1], vertPos[3 * idx1 + 2]};

        uint16_t idx2 = vertIdx[2];
        float vertPos2[3] = {vertPos[3 * idx2], vertPos[3 * idx2 + 1], vertPos[3 * idx2 + 2]};

        float v1[3] = {vertPos1[0] - vertPos0[0], vertPos1[1] - vertPos0[1], vertPos1[2] - vertPos0[2]};
        float v2[3] = {vertPos2[0] - vertPos0[0], vertPos2[1] - vertPos0[1], vertPos2[2] - vertPos0[2]};
        
        float autoGenNormal[3] = {0.f};
        SharedLib::CrossProductVec3(v1, v2, autoGenNormal);
        SharedLib::NormalizeVec(autoGenNormal, 3);

        vertNormal.resize(3 * posAccessorEleCnt);
        for (uint32_t i = 0; i < posAccessorEleCnt; i++)
        {
            uint32_t normalStartingIdx = i * 3;
            vertNormal[normalStartingIdx]     = autoGenNormal[0];
            vertNormal[normalStartingIdx + 1] = autoGenNormal[1];
            vertNormal[normalStartingIdx + 2] = autoGenNormal[2];
        }
    }

    std::vector<float> vertUv;
    int uvIdx = -1;
    if (mesh.primitives[0].attributes.count("TEXCOORD_0") > 0)
    {
        uvIdx = mesh.primitives[0].attributes.at("TEXCOORD_0");
        const auto& uvAccessor = model.accessors[uvIdx];

        assert(uvAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, "The uv accessor data type should be float.");
        assert(uvAccessor.type == TINYGLTF_TYPE_VEC2, "The uv accessor type should be vec2.");

        int uvAccessorByteOffset = uvAccessor.byteOffset;
        int uvAccessorEleCnt = uvAccessor.count;
        const auto& uvBufferView = model.bufferViews[uvAccessor.bufferView];

        int uvBufferOffset = uvAccessorByteOffset + uvBufferView.byteOffset;
        int uvBufferByteCnt = sizeof(float) * 2 * uvAccessor.count;
        vertUv.resize(2 * uvAccessor.count);

        memcpy(vertUv.data(), &pBufferData[uvBufferOffset], uvBufferByteCnt);
    }
    else
    {
        vertUv = std::vector<float>(posAccessorEleCnt * 2, 0.f);
    }


    // Load weights -- The loaded gltf must have this.
    std::vector<float> vertWeights;
    int weightIdx = mesh.primitives[0].attributes.at("WEIGHTS_0");
    const auto& weightsAccessor = model.accessors[weightIdx];

    assert(weightsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, "The weights accessor data type should be float.");
    assert(weightsAccessor.type == TINYGLTF_TYPE_VEC4, "The weights accessor type should be vec4.");

    int weightsAccessorByteOffset = weightsAccessor.byteOffset;
    int weightsAccessorEleCnt = weightsAccessor.count;
    const auto& weightsBufferView = model.bufferViews[weightsAccessor.bufferView];

    int weightsBufferOffset = weightsAccessorByteOffset + weightsBufferView.byteOffset;
    int weightsBufferByteCnt = sizeof(float) * 4 * weightsAccessorEleCnt;
    vertWeights.resize(4 * weightsAccessorEleCnt);

    memcpy(vertWeights.data(), &pBufferData[weightsBufferOffset], weightsBufferByteCnt);

    // Load joints that affect this vert -- The loaded gltf must have this.
    std::vector<uint16_t> vertJoints;
    int jointsIdx = mesh.primitives[0].attributes.at("JOINTS_0");
    const auto& jointsAccessor = model.accessors[jointsIdx];

    assert(jointsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT, "The joints accessor data type shuold be uint16.");
    assert(jointsAccessor.type == TINYGLTF_TYPE_VEC4, "The joints accessor type should be vec4.");
    
    int jointsAccessorByteOffset = jointsAccessor.byteOffset;
    int jointsAccessorEleCnt = jointsAccessor.count;
    const auto& jointsBufferView = model.bufferViews[jointsAccessor.bufferView];

    int jointsBufferOffset = jointsAccessorByteOffset + jointsBufferView.byteOffset;
    int jointsBufferByteCnt = sizeof(uint16_t) * 4 * jointsAccessorEleCnt;
    vertJoints.resize(4 * jointsAccessorEleCnt);


    // Assemble the vertex buffer data.
    // The count of [pos, normal, uv, weights, jointsIdx] is equal to posAccessor/normalAccessor/uvAccessor/weightsAccessor/jointsAccessor.count.
    // [3 floats, 3 floats, 2 floats, 4 floats, 4 uints] --> 16 * sizeof(float).
    float* pVertBufferData = new float[16 * posAccessorEleCnt];
    uint32_t vertBufferByteCnt = 16 * posAccessorEleCnt * sizeof(float);

    for (int i = 0; i < posAccessorEleCnt; i++)
    {
        int vertDataStartingIdx = i * 16;

        memcpy(pVertBufferData + vertDataStartingIdx, &vertPos[i * 3], 3 * sizeof(float));
        memcpy(pVertBufferData + vertDataStartingIdx + 3, &vertNormal[i * 3], 3 * sizeof(float));
        memcpy(pVertBufferData + vertDataStartingIdx + 6, &vertUv[i * 2], 2 * sizeof(float));
        memcpy(pVertBufferData + vertDataStartingIdx + 8, &vertWeights[i * 4], 4 * sizeof(float));

        // The original joints idx are uint16, but the hlsl only takes the uint32. Thus, we have to convert the uint16 to uint32.
        uint32_t jointsIdx[4] = { vertJoints[i * 4], vertJoints[i * 4 + 1], vertJoints[i * 4 +2], vertJoints[i * 4 + 3] };
        memcpy(pVertBufferData + vertDataStartingIdx + 12, jointsIdx, sizeof(jointsIdx));
    }

    // Create the vertex gpu buffer and the index gpu buffer. Send their data to the vertex gpu buffer and index gpu buffer.
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
            &m_skeletalMesh.mesh.idxBuffer.buffer,
            &m_skeletalMesh.mesh.idxBuffer.bufferAlloc,
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
            &m_skeletalMesh.mesh.vertBuffer.buffer,
            &m_skeletalMesh.mesh.vertBuffer.bufferAlloc,
            nullptr);
    }

    // Send idx data and vert data to their VkBuffers.
    CopyRamDataToGpuBuffer(
        pVertBufferData,
        m_skeletalMesh.mesh.vertBuffer.buffer,
        m_skeletalMesh.mesh.vertBuffer.bufferAlloc,
        vertBufferByteCnt);

    CopyRamDataToGpuBuffer(
        vertIdx.data(),
        m_skeletalMesh.mesh.idxBuffer.buffer,
        m_skeletalMesh.mesh.idxBuffer.bufferAlloc,
        idxBufferByteCnt);

    delete[] pVertBufferData;


    // Load the base color texture or create a default pure color texture.
    int materialIdx = mesh.primitives[0].material;
   
    if (materialIdx != -1)
    {
        const auto& material = model.materials[materialIdx];
        int baseColorTexIdx = material.pbrMetallicRoughness.baseColorTexture.index;

        // A texture is defined by an image index, denoted by the source property and a sampler index (sampler).
        // Assmue that all textures are 8 bits per channel. They are all xxx / 255. They all have 4 components.
        const auto& baseColorTex = model.textures[baseColorTexIdx];
        int baseColorTexImgIdx = baseColorTex.source;
        const auto& baseColorImg = model.images[baseColorTexImgIdx];
        
        VkImageSubresourceRange tex2dSubResRange{};
        {
            tex2dSubResRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            tex2dSubResRange.baseMipLevel = 0;
            tex2dSubResRange.levelCount = 1;
            tex2dSubResRange.baseArrayLayer = 0;
            tex2dSubResRange.layerCount = 1;
        }

        VkSamplerCreateInfo samplerInfo{};
        {
            samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter = VK_FILTER_LINEAR;
            samplerInfo.minFilter = VK_FILTER_LINEAR;
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.minLod = -1000;
            samplerInfo.maxLod = 1000;
            samplerInfo.maxAnisotropy = 1.0f;
        }

        SharedLib::GpuImgCreateInfo gpuImgCreateInfo{};
        {
            gpuImgCreateInfo.allocFlags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
            gpuImgCreateInfo.hasSampler = true;
            gpuImgCreateInfo.imgSubresRange = tex2dSubResRange;
            gpuImgCreateInfo.imgUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            gpuImgCreateInfo.imgViewType = VK_IMAGE_VIEW_TYPE_2D;
            gpuImgCreateInfo.samplerInfo = samplerInfo;
            gpuImgCreateInfo.imgExtent = VkExtent3D{ (uint32_t)baseColorImg.width, (uint32_t)baseColorImg.height, 1 };
            gpuImgCreateInfo.imgFormat = VK_FORMAT_R8G8B8A8_SRGB;
        }

        m_skeletalMesh.mesh.baseColorImg.gpuImg = CreateGpuImage(gpuImgCreateInfo);

        VkBufferImageCopy baseColorBufToImgCopy{};
        {
            VkExtent3D extent{};
            {
                extent.width = baseColorImg.width;
                extent.height = baseColorImg.height;
                extent.depth = 1;
            }

            baseColorBufToImgCopy.bufferRowLength = extent.width;
            baseColorBufToImgCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            baseColorBufToImgCopy.imageSubresource.mipLevel = 0;
            baseColorBufToImgCopy.imageSubresource.baseArrayLayer = 0;
            baseColorBufToImgCopy.imageSubresource.layerCount = 1;
            baseColorBufToImgCopy.imageExtent = extent;
        }


        SharedLib::RAIICommandBuffer raiiCmdBuffer(m_gfxCmdPool, m_device);

        SharedLib::SendImgDataToGpu(raiiCmdBuffer.m_cmdBuffer,
                                    m_device,
                                    m_graphicsQueue,
                                    (void*)baseColorImg.image.data(),
                                    baseColorImg.image.size() * sizeof(unsigned char),
                                    m_skeletalMesh.mesh.baseColorImg.gpuImg.image,
                                    tex2dSubResRange,
                                    VK_IMAGE_LAYOUT_UNDEFINED,
                                    baseColorBufToImgCopy,
                                    *m_pAllocator);
    }
    else
    {
        float white[3] = { 1.f, 1.f, 1.f };
        m_skeletalMesh.mesh.baseColorImg.gpuImg = CreateDummyPureColorImg(white);
    }
}

// ================================================================================================================
void SkinAnimGltfApp::DestroyGltf()
{
    // Release mesh related resources
    Mesh& mesh = m_skeletalMesh.mesh;
    vmaDestroyBuffer(*m_pAllocator, mesh.idxBuffer.buffer, mesh.idxBuffer.bufferAlloc);
    vmaDestroyBuffer(*m_pAllocator, mesh.vertBuffer.buffer, mesh.vertBuffer.bufferAlloc);

    vmaDestroyImage(*m_pAllocator, mesh.baseColorImg.gpuImg.image, mesh.baseColorImg.gpuImg.imageAllocation);
    vkDestroyImageView(m_device, mesh.baseColorImg.gpuImg.imageView, nullptr);
    vkDestroySampler(m_device, mesh.baseColorImg.gpuImg.imageSampler, nullptr);

    // Release skeleton related resources
    Skeleton& skeleton = m_skeletalMesh.skeleton;
    vmaDestroyBuffer(*m_pAllocator, skeleton.jointsMatsBuffer.buffer, skeleton.jointsMatsBuffer.bufferAlloc);
}

// ================================================================================================================
void SkinAnimGltfApp::InitSkinAnimPipeline()
{
    VkPipelineRenderingCreateInfoKHR pipelineRenderCreateInfo{};
    {
        pipelineRenderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        pipelineRenderCreateInfo.colorAttachmentCount = 1;
        pipelineRenderCreateInfo.pColorAttachmentFormats = &m_choisenSurfaceFormat.format;
        pipelineRenderCreateInfo.depthAttachmentFormat = VK_FORMAT_D16_UNORM;
    }

    m_skinAnimPipeline.SetPNext(&pipelineRenderCreateInfo);

    VkPipelineShaderStageCreateInfo shaderStgsInfo[2] = {};
    shaderStgsInfo[0] = CreateDefaultShaderStgCreateInfo(m_vsSkinAnimShaderModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStgsInfo[1] = CreateDefaultShaderStgCreateInfo(m_psSkinAnimShaderModule, VK_SHADER_STAGE_FRAGMENT_BIT);

    VkPipelineVertexInputStateCreateInfo vertInputInfo = CreatePipelineVertexInputInfo();
    m_skinAnimPipeline.SetVertexInputInfo(&vertInputInfo);

    VkPipelineDepthStencilStateCreateInfo depthStencilInfo = CreateDepthStencilStateInfo();
    m_skinAnimPipeline.SetDepthStencilStateInfo(&depthStencilInfo);

    m_skinAnimPipeline.SetShaderStageInfo(shaderStgsInfo, 2);
    m_skinAnimPipeline.SetPipelineLayout(m_skinAnimPipelineLayout);
    m_skinAnimPipeline.CreatePipeline(m_device);
}

// ================================================================================================================
void SkinAnimGltfApp::InitSkinAnimPipelineDescriptorSetLayout()
{
    // Create pipeline's descriptors layout
    // The Vulkan spec states: The VkDescriptorSetLayoutBinding::binding members of the elements of the pBindings array 
    // must each have different values 
    // (https://vulkan.lunarg.com/doc/view/1.3.236.0/windows/1.3-extensions/vkspec.html#VUID-VkDescriptorSetLayoutCreateInfo-binding-00279)

    // Create pipeline binding and descriptor objects for the camera parameters
    std::vector<VkDescriptorSetLayoutBinding> iblModelRenderBindings;

    VkDescriptorSetLayoutBinding vpMatUboBinding{};
    {
        vpMatUboBinding.binding = 0;
        vpMatUboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        vpMatUboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        vpMatUboBinding.descriptorCount = 1;
    }
    iblModelRenderBindings.push_back(vpMatUboBinding);

    VkDescriptorSetLayoutBinding diffuseIrradianceBinding{};
    {
        diffuseIrradianceBinding.binding = 1;
        diffuseIrradianceBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        diffuseIrradianceBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        diffuseIrradianceBinding.descriptorCount = 1;
    }
    iblModelRenderBindings.push_back(diffuseIrradianceBinding);

    VkDescriptorSetLayoutBinding prefilterEnvBinding{};
    {
        prefilterEnvBinding.binding = 2;
        prefilterEnvBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        prefilterEnvBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        prefilterEnvBinding.descriptorCount = 1;
    }
    iblModelRenderBindings.push_back(prefilterEnvBinding);

    VkDescriptorSetLayoutBinding envBrdfBinding{};
    {
        envBrdfBinding.binding = 3;
        envBrdfBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        envBrdfBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        envBrdfBinding.descriptorCount = 1;
    }
    iblModelRenderBindings.push_back(envBrdfBinding);

    VkDescriptorSetLayoutBinding baseColorBinding{};
    {
        baseColorBinding.binding = 4;
        baseColorBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        baseColorBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        baseColorBinding.descriptorCount = 1;
    }
    iblModelRenderBindings.push_back(baseColorBinding);

    VkDescriptorSetLayoutBinding normalBinding{};
    {
        normalBinding.binding = 5;
        normalBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        normalBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        normalBinding.descriptorCount = 1;
    }
    iblModelRenderBindings.push_back(normalBinding);

    VkDescriptorSetLayoutBinding metallicRoughnessBinding{};
    {
        metallicRoughnessBinding.binding = 6;
        metallicRoughnessBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        metallicRoughnessBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        metallicRoughnessBinding.descriptorCount = 1;
    }
    iblModelRenderBindings.push_back(metallicRoughnessBinding);

    VkDescriptorSetLayoutBinding occlusionBinding{};
    {
        occlusionBinding.binding = 7;
        occlusionBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        occlusionBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        occlusionBinding.descriptorCount = 1;
    }
    iblModelRenderBindings.push_back(occlusionBinding);

    VkDescriptorSetLayoutCreateInfo iblRenderPipelineDesSetLayoutInfo{};
    {
        iblRenderPipelineDesSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        iblRenderPipelineDesSetLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
        iblRenderPipelineDesSetLayoutInfo.bindingCount = iblModelRenderBindings.size();
        iblRenderPipelineDesSetLayoutInfo.pBindings = iblModelRenderBindings.data();
    }

    VK_CHECK(vkCreateDescriptorSetLayout(m_device,
                                         &iblRenderPipelineDesSetLayoutInfo,
                                         nullptr,
                                         &m_skinAnimPipelineDesSetLayout));
}

// ================================================================================================================
void SkinAnimGltfApp::InitSkinAnimPipelineLayout()
{
    VkPushConstantRange range = {};
    {
        range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        range.offset = 0;
        range.size = 4 * sizeof(float); // Camera pos, Max IBL mipmap.
    }

    // Create pipeline layout
    // NOTE: pSetLayouts must not contain more than one descriptor set layout that was created with
    //       VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR set.
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    {
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_skinAnimPipelineDesSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &range;
    }

    VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_skinAnimPipelineLayout));
}

// ================================================================================================================
void SkinAnimGltfApp::InitSkinAnimShaderModules()
{
    m_vsSkinAnimShaderModule = CreateShaderModule("./hlsl/skinAnim_vert.spv");
    m_psSkinAnimShaderModule = CreateShaderModule("./hlsl/skinAnim_frag.spv");
}

// ================================================================================================================
void SkinAnimGltfApp::CmdPushSkeletonSkinRenderingDescriptors(
    VkCommandBuffer cmdBuffer,
    const Mesh&     mesh)
{
    std::vector<VkWriteDescriptorSet> skinAnimSet0Infos;

    /*
    // Descriptor set 0 infos.
    VkWriteDescriptorSet writeIblMvpMatUboBufDesSet{};
    {
        writeIblMvpMatUboBufDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeIblMvpMatUboBufDesSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeIblMvpMatUboBufDesSet.dstBinding = 0;
        writeIblMvpMatUboBufDesSet.descriptorCount = 1;
        writeIblMvpMatUboBufDesSet.pBufferInfo = &m_iblMvpMatsUboDescriptorBuffersInfos[m_currentFrame];
    }
    iblRenderDescriptorSet0Infos.push_back(writeIblMvpMatUboBufDesSet);

    VkWriteDescriptorSet writeDiffIrrDesSet{};
    {
        writeDiffIrrDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDiffIrrDesSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeDiffIrrDesSet.dstBinding = 1;
        writeDiffIrrDesSet.pImageInfo = &m_diffuseIrradianceCubemapDescriptorImgInfo;
        writeDiffIrrDesSet.descriptorCount = 1;
    }
    iblRenderDescriptorSet0Infos.push_back(writeDiffIrrDesSet);

    VkWriteDescriptorSet writePrefilterEnvDesSet{};
    {
        writePrefilterEnvDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writePrefilterEnvDesSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writePrefilterEnvDesSet.dstBinding = 2;
        writePrefilterEnvDesSet.pImageInfo = &m_prefilterEnvCubemapDescriptorImgInfo;
        writePrefilterEnvDesSet.descriptorCount = 1;
    }
    iblRenderDescriptorSet0Infos.push_back(writePrefilterEnvDesSet);

    VkWriteDescriptorSet writeEnvBrdfDesSet{};
    {
        writeEnvBrdfDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeEnvBrdfDesSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeEnvBrdfDesSet.dstBinding = 3;
        writeEnvBrdfDesSet.pImageInfo = &m_envBrdfImgDescriptorImgInfo;
        writeEnvBrdfDesSet.descriptorCount = 1;
    }
    iblRenderDescriptorSet0Infos.push_back(writeEnvBrdfDesSet);

    VkWriteDescriptorSet writeBaseColorDesSet{};
    {
        writeBaseColorDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeBaseColorDesSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeBaseColorDesSet.dstBinding = 4;
        writeBaseColorDesSet.pImageInfo = &mesh.baseColorImgDescriptorInfo;
        writeBaseColorDesSet.descriptorCount = 1;
    }
    iblRenderDescriptorSet0Infos.push_back(writeBaseColorDesSet);

    VkWriteDescriptorSet writeNormalDesSet{};
    {
        writeNormalDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeNormalDesSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeNormalDesSet.dstBinding = 5;
        writeNormalDesSet.pImageInfo = &mesh.normalImgDescriptorInfo;
        writeNormalDesSet.descriptorCount = 1;
    }
    iblRenderDescriptorSet0Infos.push_back(writeNormalDesSet);

    VkWriteDescriptorSet writeRoughnessMetallicDesSet{};
    {
        writeRoughnessMetallicDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeRoughnessMetallicDesSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeRoughnessMetallicDesSet.dstBinding = 6;
        writeRoughnessMetallicDesSet.pImageInfo = &mesh.metallicRoughnessImgDescriptorInfo;
        writeRoughnessMetallicDesSet.descriptorCount = 1;
    }
    iblRenderDescriptorSet0Infos.push_back(writeRoughnessMetallicDesSet);

    VkWriteDescriptorSet writeOcclusionDesSet{};
    {
        writeOcclusionDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeOcclusionDesSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeOcclusionDesSet.dstBinding = 7;
        writeOcclusionDesSet.pImageInfo = &mesh.occlusionImgDescriptorInfo;
        writeOcclusionDesSet.descriptorCount = 1;
    }
    iblRenderDescriptorSet0Infos.push_back(writeOcclusionDesSet);
    */

    // Push decriptors
    m_vkCmdPushDescriptorSetKHR(cmdBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_skinAnimPipelineLayout,
                                0, skinAnimSet0Infos.size(), skinAnimSet0Infos.data());
}

// ================================================================================================================
void SkinAnimGltfApp::DestroySkinAnimPipelineRes()
{
    // Destroy shader modules
    vkDestroyShaderModule(m_device, m_vsSkinAnimShaderModule, nullptr);
    vkDestroyShaderModule(m_device, m_psSkinAnimShaderModule, nullptr);

    // Destroy the pipeline layout
    vkDestroyPipelineLayout(m_device, m_skinAnimPipelineLayout, nullptr);

    // Destroy the descriptor set layout
    vkDestroyDescriptorSetLayout(m_device, m_skinAnimPipelineDesSetLayout, nullptr);
}

// ================================================================================================================
/*
void SkinAnimGltfApp::InitVpMatBuffer()
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
*/

// ================================================================================================================
/*
void SkinAnimGltfApp::DestroyVpMatBuffer()
{
    for (uint32_t i = 0; i < SharedLib::MAX_FRAMES_IN_FLIGHT; i++)
    {
        vmaDestroyBuffer(*m_pAllocator, m_vpMatUboBuffer[i], m_vpMatUboAlloc[i]);
    }
}
*/

// ================================================================================================================
// Elements notes:
// Position: float3, normal: float3, tangent: float4, texcoord: float2.
VkPipelineVertexInputStateCreateInfo SkinAnimGltfApp::CreatePipelineVertexInputInfo()
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
VkPipelineDepthStencilStateCreateInfo SkinAnimGltfApp::CreateDepthStencilStateInfo()
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
// TODO: The model should be at the center of the scene and the camera should rotate the model to make the animation.
/*
void SkinAnimGltfApp::InitIblMvpMatsBuffer()
{
    VkBufferCreateInfo bufferInfo{};
    {
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = 32 * sizeof(float);
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo bufferAllocInfo{};
    {
        bufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        bufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
                                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    m_iblMvpMatsUboBuffer.resize(SharedLib::MAX_FRAMES_IN_FLIGHT);
    m_iblMvpMatsUboAlloc.resize(SharedLib::MAX_FRAMES_IN_FLIGHT);
    m_iblMvpMatsUboDescriptorBuffersInfos.resize(SharedLib::MAX_FRAMES_IN_FLIGHT);

    // NOTE: Perspective Mat x View Mat x Model Mat x position.
    float modelMatData[16] = {
        1.f, 0.f, 0.f, ModelWorldPos[0],
        0.f, 1.f, 0.f, ModelWorldPos[1],
        0.f, 0.f, 1.f, ModelWorldPos[2],
        0.f, 0.f, 0.f, 1.f
    };

    float vpMatData[16] = {};
    float tmpViewMatData[16] = {};
    float tmpPersMatData[16] = {};
    m_pCamera->GenViewPerspectiveMatrices(tmpViewMatData, tmpPersMatData, vpMatData);

    float iblUboData[32] = {};
    memcpy(iblUboData, modelMatData, sizeof(modelMatData));
    memcpy(&iblUboData[16], vpMatData, sizeof(vpMatData));

    for (uint32_t i = 0; i < SharedLib::MAX_FRAMES_IN_FLIGHT; i++)
    {
        vmaCreateBuffer(*m_pAllocator,
                        &bufferInfo,
                        &bufferAllocInfo,
                        &m_iblMvpMatsUboBuffer[i],
                        &m_iblMvpMatsUboAlloc[i],
                        nullptr);

        CopyRamDataToGpuBuffer(iblUboData,
                               m_iblMvpMatsUboBuffer[i],
                               m_iblMvpMatsUboAlloc[i],
                               sizeof(iblUboData));

        m_iblMvpMatsUboDescriptorBuffersInfos[i].buffer = m_iblMvpMatsUboBuffer[i];
        m_iblMvpMatsUboDescriptorBuffersInfos[i].offset = 0;
        m_iblMvpMatsUboDescriptorBuffersInfos[i].range = 32 * sizeof(float);
    }
}
*/

// ================================================================================================================
/*
void SkinAnimGltfApp::DestroyIblMvpMatsBuffer()
{
    for (uint32_t i = 0; i < m_iblMvpMatsUboBuffer.size(); i++)
    {
        vmaDestroyBuffer(*m_pAllocator, m_iblMvpMatsUboBuffer[i], m_iblMvpMatsUboAlloc[i]);
    }
}
*/

// ================================================================================================================
std::vector<float> SkinAnimGltfApp::GetVertPushConsants()
{
    return std::vector<float>(0, 0.f);
}

// ================================================================================================================
std::vector<float> SkinAnimGltfApp::GetFragPushConstants()
{
    return std::vector<float>(0, 0.f);
}