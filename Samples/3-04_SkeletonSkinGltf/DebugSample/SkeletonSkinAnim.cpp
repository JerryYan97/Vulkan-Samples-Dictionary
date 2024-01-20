#include "SkeletonSkinAnim.h"
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

        VmaAllocationCreateInfo diffIrrAllocInfo{};
        {
            diffIrrAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            diffIrrAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        }

        VkExtent3D extent{};
        {
            extent.width = m_diffuseIrradianceCubemap.pixWidths[0];
            extent.height = m_diffuseIrradianceCubemap.pixWidths[0];
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
            &m_diffuseIrradianceCubemap.gpuImg.image,
            &m_diffuseIrradianceCubemap.gpuImg.imageAllocation,
            nullptr));

        VkImageViewCreateInfo info{};
        {
            info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            info.image = m_diffuseIrradianceCubemap.gpuImg.image;
            info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
            info.format = VK_FORMAT_R32G32B32_SFLOAT;
            info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            info.subresourceRange.levelCount = 1;
            info.subresourceRange.layerCount = 6;
        }
        VK_CHECK(vkCreateImageView(m_device, &info, nullptr, &m_diffuseIrradianceCubemap.gpuImg.imageView));

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
        VK_CHECK(vkCreateSampler(m_device, &sampler_info, nullptr, &m_diffuseIrradianceCubemap.gpuImg.imageSampler));

        m_diffuseIrradianceCubemap.gpuImg.imageDescInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        m_diffuseIrradianceCubemap.gpuImg.imageDescInfo.imageView = m_diffuseIrradianceCubemap.gpuImg.imageView;
        m_diffuseIrradianceCubemap.gpuImg.imageDescInfo.sampler = m_diffuseIrradianceCubemap.gpuImg.imageSampler;
    }

    // Read in and init prefilter environment cubemap
    {
        std::string prefilterEnvPath = hdriFilePath + "ibl/prefilterEnvMaps/";
        std::vector<std::string> mipImgNames;
        SharedLib::GetAllFileNames(prefilterEnvPath, mipImgNames);

        const uint32_t mipCnts = mipImgNames.size();

        m_prefilterEnvCubemap.pData.resize(mipCnts);
        m_prefilterEnvCubemap.pixWidths.resize(mipCnts);
        m_prefilterEnvCubemap.pixHeights.resize(mipCnts);

        for (uint32_t i = 0; i < mipCnts; i++)
        {
            int width, height, nrComponents;
            std::string prefilterEnvMipImgPathName = prefilterEnvPath + "prefilterMip" + std::to_string(i) + ".hdr";

            m_prefilterEnvCubemap.pData.push_back(stbi_loadf(prefilterEnvMipImgPathName.c_str(),
                                                             &width, &height, &nrComponents, 0));
            m_prefilterEnvCubemap.pixWidths.push_back(width);
            m_prefilterEnvCubemap.pixHeights.push_back(height);
        }

        VmaAllocationCreateInfo prefilterEnvCubemapAllocInfo{};
        {
            prefilterEnvCubemapAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            prefilterEnvCubemapAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        }

        VkExtent3D extent{};
        {
            extent.width = m_prefilterEnvCubemap.pixWidths[0];
            extent.height = m_prefilterEnvCubemap.pixHeights[0];
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
            &m_prefilterEnvCubemap.gpuImg.image,
            &m_prefilterEnvCubemap.gpuImg.imageAllocation,
            nullptr));

        VkImageViewCreateInfo info{};
        {
            info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            info.image = m_prefilterEnvCubemap.gpuImg.image;
            info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
            info.format = VK_FORMAT_R32G32B32_SFLOAT;
            info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            info.subresourceRange.levelCount = mipCnts;
            info.subresourceRange.layerCount = 6;
        }
        VK_CHECK(vkCreateImageView(m_device, &info, nullptr, &m_prefilterEnvCubemap.gpuImg.imageView));

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
        VK_CHECK(vkCreateSampler(m_device, &sampler_info, nullptr, &m_prefilterEnvCubemap.gpuImg.imageSampler));

        m_prefilterEnvCubemap.gpuImg.imageDescInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        m_prefilterEnvCubemap.gpuImg.imageDescInfo.imageView = m_prefilterEnvCubemap.gpuImg.imageView;
        m_prefilterEnvCubemap.gpuImg.imageDescInfo.sampler = m_prefilterEnvCubemap.gpuImg.imageSampler;
    }

    // Read in and init environment brdf map
    {
        std::string envBrdfMapPathName = hdriFilePath + "ibl/envBrdf.hdr";
        int width, height, nrComponents;
        m_envBrdfImg.pData.push_back(stbi_loadf(envBrdfMapPathName.c_str(), &width, &height, &nrComponents, 0));
        m_envBrdfImg.pixWidths.push_back(width);
        m_envBrdfImg.pixHeights.push_back(height);

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
            &m_envBrdfImg.gpuImg.image,
            &m_envBrdfImg.gpuImg.imageAllocation,
            nullptr));

        VkImageViewCreateInfo info{};
        {
            info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            info.image = m_envBrdfImg.gpuImg.image;
            info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            info.format = VK_FORMAT_R32G32B32_SFLOAT;
            info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            info.subresourceRange.levelCount = 1;
            info.subresourceRange.layerCount = 1;
        }
        VK_CHECK(vkCreateImageView(m_device, &info, nullptr, &m_envBrdfImg.gpuImg.imageView));

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
        VK_CHECK(vkCreateSampler(m_device, &sampler_info, nullptr, &m_envBrdfImg.gpuImg.imageSampler));

        m_envBrdfImg.gpuImg.imageDescInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        m_envBrdfImg.gpuImg.imageDescInfo.imageView = m_envBrdfImg.gpuImg.imageView;
        m_envBrdfImg.gpuImg.imageDescInfo.sampler = m_envBrdfImg.gpuImg.imageSampler;
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

    assert(model.meshes.size() == 1, "This example only supports one mesh.");

    uint32_t meshCnt = model.meshes.size();
    bool autoGenNormal = false;
    bool autoGenUv = false;
    
    const auto& mesh = model.meshes[0];
    int posIdx = mesh.primitives[0].attributes.at("POSITION");
    int normalIdx = mesh.primitives[0].attributes.at("NORMAL");

    int uvIdx = mesh.primitives[0].attributes.at("TEXCOORD_0");

    int indicesIdx = mesh.primitives[0].indices;
    int materialIdx = mesh.primitives[0].material;

    // Elements notes:
    // Position: float3, normal: float3, texcoord: float2, joints: int4, weights float4. -- If the gltf model doesn't have normal or uvs, we will just create dummy normal and uv.

    // Setup the vertex buffer and the index buffer
    const auto& posAccessor = model.accessors[posIdx];
    int posAccessorByteOffset = posAccessor.byteOffset;
    int posAccessorEleCnt = posAccessor.count; // Assume a position element is a float3.

    if (autoGenNormal == false)
    {
        const auto& normalAccessor = model.accessors[normalIdx];
        int normalAccessorByteOffset = normalAccessor.byteOffset;
        int normalAccessorEleCnt = normalAccessor.count;
    }
    
    if (autoGenUv == false)
    {
        const auto& uvAccessor = model.accessors[uvIdx];
        int uvAccessorByteOffset = uvAccessor.byteOffset;
        int uvAccessorEleCnt = uvAccessor.count;
    }

    const auto& idxAccessor = model.accessors[indicesIdx];
    int idxAccessorByteOffset = idxAccessor.byteOffset;
    int idxAccessorEleCnt = idxAccessor.count;

    /*
    // NOTE: Buffer views are just division of the buffer for a model.
    // SCALAR is in one buffer view. FLOAT2 in one. FLOAT3 in one. and FLOAT3 in one...
    // Maybe they can be more
    // A buffer view represents a contiguous segment of data in a buffer, defined by a byte offset into the buffer specified 
    // in the byteOffset property and a total byte length specified by the byteLength property of the buffer view.
    const auto& posBufferView = model.bufferViews[posAccessor.bufferView];
    const auto& normalBufferView = model.bufferViews[normalAccessor.bufferView];
    const auto& uvBufferView = model.bufferViews[uvAccessor.bufferView];
    const auto& idxBufferView = model.bufferViews[idxAccessor.bufferView];

    // We assume that the idx, position, normal, uv and tangent are not interleaved.
    // TODO: Even though they are interleaved, we can use a function to read out the data by making use of the stride bytes count.

    // Assmue the data and element type of the index is uint16_t.
    int idxBufferOffset = idxAccessorByteOffset + idxBufferView.byteOffset;
    int idxBufferByteCnt = sizeof(uint16_t) * idxAccessor.count;
    m_skeletalMesh.mesh[i].idxData.resize(idxAccessor.count);
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
        m_gltfModeMeshes[i].vertData[12 * vertIdx + 10] = pUvData[2 * vertIdx];
        m_gltfModeMeshes[i].vertData[12 * vertIdx + 11] = pUvData[2 * vertIdx + 1];
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
            sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            sampler_info.minLod = -1000;
            sampler_info.maxLod = 1000;
            sampler_info.maxAnisotropy = 1.0f;
        }
        VK_CHECK(vkCreateSampler(m_device, &sampler_info, nullptr, &m_gltfModeMeshes[i].baseColorImgSampler));

        m_gltfModeMeshes[i].baseColorImgDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        m_gltfModeMeshes[i].baseColorImgDescriptorInfo.imageView = m_gltfModeMeshes[i].baseColorImgView;
        m_gltfModeMeshes[i].baseColorImgDescriptorInfo.sampler = m_gltfModeMeshes[i].baseColorImgSampler;
    }
    */
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