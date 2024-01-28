#include "SkeletonSkinAnim.h"
#include <glfw3.h>
#include "../../../SharedLibrary/Utils/VulkanDbgUtils.h"
#include "../../../SharedLibrary/Camera/Camera.h"
#include "../../../SharedLibrary/Event/Event.h"
#include "../../../SharedLibrary/Utils/StrPathUtils.h"
#include "../../../SharedLibrary/Utils/DiskOpsUtils.h"
#include "../../../SharedLibrary/Utils/CmdBufUtils.h"
#include "../../../SharedLibrary/Utils/AppUtils.h"
#include <glm/ext/quaternion_common.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <cmath>

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
    m_currentAnimTime(-1.f),
    m_iblDir(iblPath),
    m_gltfPathName(gltfPathName),
    m_maxAnimTime(-1.f)
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

    DestroyGpuImgResource(m_diffuseIrradianceCubemap);
    DestroyGpuImgResource(m_prefilterEnvCubemap);
    DestroyGpuImgResource(m_envBrdfImg);
}

// ================================================================================================================
void SkinAnimGltfApp::UpdateCamera()
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

    // Read in and init diffuse irradiance cubemap
    {
        std::string diffIrradiancePathName = hdriFilePath + "ibl/diffuse_irradiance_cubemap.hdr";
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
        std::string prefilterEnvPath = hdriFilePath + "ibl/prefilterEnvMaps/";
        std::vector<std::string> mipImgNames;
        SharedLib::GetAllFileNames(prefilterEnvPath, mipImgNames);

        m_prefilterEnvMipsCnt = mipImgNames.size();

        for (uint32_t i = 0; i < m_prefilterEnvMipsCnt; i++)
        {
            int width, height, nrComponents;
            std::string prefilterEnvMipImgPathName = hdriFilePath +
                "ibl/prefilterEnvMaps/prefilterMip" +
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
        std::string envBrdfMapPathName = hdriFilePath + "ibl/envBrdf.hdr";
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
    InitGfxCommandBuffers(m_swapchainImgCnt);

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
// * TODO: The accessor data load can be abstracted to a function.
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

    // NOTE: (1): TinyGltf loader has already loaded the binary buffer data and the images data.
    //       (2): The gltf may has multiple buffers. The buffer idx should come from the buffer view.
    // const auto& binaryBuffer = model.buffers[0].data;
    // const unsigned char* pBufferData = binaryBuffer.data();

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

    const unsigned char* pPosBufferData = model.buffers[posBufferView.buffer].data.data();
    memcpy(vertPos.data(), &pPosBufferData[posBufferOffset], posBufferByteCnt);


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
    m_vertIdxCnt = idxAccessorEleCnt;
    vertIdx.resize(idxAccessorEleCnt);

    const unsigned char* pIdxBufferData = model.buffers[idxBufferView.buffer].data.data();
    memcpy(vertIdx.data(), &pIdxBufferData[idxBufferOffset], idxBufferByteCnt);


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

        const unsigned char* pNormalBufferData = model.buffers[normalBufferView.buffer].data.data();
        memcpy(vertNormal.data(), &pNormalBufferData[normalBufferOffset], normalBufferByteCnt);
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

        const unsigned char* pUvBufferData = model.buffers[uvBufferView.buffer].data.data();
        memcpy(vertUv.data(), &pUvBufferData[uvBufferOffset], uvBufferByteCnt);
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

    const unsigned char* pWeightBufferData = model.buffers[weightsBufferView.buffer].data.data();
    memcpy(vertWeights.data(), &pWeightBufferData[weightsBufferOffset], weightsBufferByteCnt);

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

    const unsigned char* pJointsBufferData = model.buffers[jointsBufferView.buffer].data.data();
    memcpy(vertJoints.data(), &pJointsBufferData[jointsBufferOffset], jointsBufferByteCnt);

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

        m_skeletalMesh.mesh.baseColorImg = CreateGpuImage(gpuImgCreateInfo);

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
                                    m_skeletalMesh.mesh.baseColorImg.image,
                                    tex2dSubResRange,
                                    VK_IMAGE_LAYOUT_UNDEFINED,
                                    baseColorBufToImgCopy,
                                    *m_pAllocator);
    }
    else
    {
        float white[3] = { 1.f, 1.f, 1.f };
        m_skeletalMesh.mesh.baseColorImg = CreateDummyPureColorImg(white);
    }


    // Read in the skin data
    const auto& skin = model.skins[0];

    std::vector<float> invBindMatricesData;
    int invBindMatricesIdx = skin.inverseBindMatrices;
    const auto& invBindMatAccessor = model.accessors[invBindMatricesIdx];

    assert(invBindMatAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, "The inverse bind matrices accessor data type should be float.");
    assert(invBindMatAccessor.type == TINYGLTF_TYPE_MAT4, "The inverse bind matrices accessor type should be mat4.");

    int invBindMatAccessorByteOffset = invBindMatAccessor.byteOffset;
    int invBindMatAccessorEleCnt = invBindMatAccessor.count;
    const auto& invBindMatBufferView = model.bufferViews[invBindMatAccessor.bufferView];

    int invBindMatBufferOffset = invBindMatAccessorByteOffset + invBindMatBufferView.byteOffset;
    int invBindMatBufferByteCnt = sizeof(float) * 16 * invBindMatAccessorEleCnt;
    invBindMatricesData.resize(16 * invBindMatAccessorEleCnt);

    const unsigned char* pInvBindMatBufferData = model.buffers[invBindMatBufferView.buffer].data.data();
    memcpy(invBindMatricesData.data(), &pInvBindMatBufferData[invBindMatBufferOffset], invBindMatBufferByteCnt);


    // Construct the skeleton in RAM
    m_skeletalMesh.skeleton.joints.resize(skin.joints.size());
    for (uint32_t i = 0; i < skin.joints.size(); i++)
    {
        const auto& jointI = model.nodes[skin.joints[i]];

        // Set joint translation from the gltf
        if (jointI.translation.size() != 0)
        {
            m_skeletalMesh.skeleton.joints[i].localTranslation[0] = jointI.translation[0];
            m_skeletalMesh.skeleton.joints[i].localTranslation[1] = jointI.translation[1];
            m_skeletalMesh.skeleton.joints[i].localTranslation[2] = jointI.translation[2];
        }
        else
        {
            m_skeletalMesh.skeleton.joints[i].localTranslation[0] = 0.f;
            m_skeletalMesh.skeleton.joints[i].localTranslation[1] = 0.f;
            m_skeletalMesh.skeleton.joints[i].localTranslation[2] = 0.f;
        }

        // Set joint rotation from the gltf -- quternion
        if (jointI.rotation.size() != 0)
        {
            m_skeletalMesh.skeleton.joints[i].localRotation[0] = jointI.rotation[0];
            m_skeletalMesh.skeleton.joints[i].localRotation[1] = jointI.rotation[1];
            m_skeletalMesh.skeleton.joints[i].localRotation[2] = jointI.rotation[2];
            m_skeletalMesh.skeleton.joints[i].localRotation[3] = jointI.rotation[3];
        }
        else
        {
            m_skeletalMesh.skeleton.joints[i].localRotation[0] = 0.f;
            m_skeletalMesh.skeleton.joints[i].localRotation[1] = 0.f;
            m_skeletalMesh.skeleton.joints[i].localRotation[2] = 0.f;
            m_skeletalMesh.skeleton.joints[i].localRotation[3] = 1.f;
        }

        // Set joint scaling from the gltf
        if (jointI.scale.size() != 0)
        {
            m_skeletalMesh.skeleton.joints[i].localScale[0] = jointI.scale[0];
            m_skeletalMesh.skeleton.joints[i].localScale[1] = jointI.scale[1];
            m_skeletalMesh.skeleton.joints[i].localScale[2] = jointI.scale[2];
        }
        else
        {
            m_skeletalMesh.skeleton.joints[i].localScale[0] = 1.f;
            m_skeletalMesh.skeleton.joints[i].localScale[1] = 1.f;
            m_skeletalMesh.skeleton.joints[i].localScale[2] = 1.f;
        }

        // Set joint inverse bind matrix data
        memcpy(m_skeletalMesh.skeleton.joints[i].inverseBindMatrix.data(),
               &invBindMatricesData[i * 16],
               16 * sizeof(float));

        // Set children
        for (uint32_t childIdx = 0; childIdx < jointI.children.size(); childIdx++)
        {
            uint32_t childJointIdx = jointI.children[childIdx] - 1; // Assume that the first node is always the mesh+skeleton.
            m_skeletalMesh.skeleton.joints[i].children.push_back(&m_skeletalMesh.skeleton.joints[childJointIdx]);
        }
    }


    // Create Gpu buffer for the skeleton
    m_skeletalMesh.skeleton.jointsMatsBuffers.resize(m_swapchainImgCnt);
    for (uint32_t i = 0; i < m_swapchainImgCnt; i++)
    {
        m_skeletalMesh.skeleton.jointsMatsBuffers[i] = CreateGpuBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                                       VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                                                                       m_skeletalMesh.skeleton.joints.size() * 16 * sizeof(float));
    }
    

    // Read in animation
    const auto& animation = model.animations[0];
    for (const auto& channel : animation.channels)
    {
        const auto& sampler = animation.samplers[channel.sampler];
        const auto& target = channel.target_path;

        // We always assume that the first node is a skeletonal mesh node and the subsequent nodes are joints nodes.
        uint32_t jointId = channel.target_node - 1; 

        const auto& timeAccessor = model.accessors[sampler.input];
        
        assert(timeAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, "The time accessor component type should be float.");
        assert(timeAccessor.type == TINYGLTF_TYPE_SCALAR, "The time accessor type should be SCALAR.");

        int timeAccessorByteOffset = timeAccessor.byteOffset;
        int timeAccessorEleCnt = timeAccessor.count;
        const auto& timeBufferView = model.bufferViews[timeAccessor.bufferView];

        int timeBufferOffset = timeAccessorByteOffset + timeBufferView.byteOffset;
        int timeBufferByteCnt = sizeof(float) * timeAccessorEleCnt;

        const unsigned char* pTimeBufferData = model.buffers[timeBufferView.buffer].data.data();

        if (target.compare("translation") == 0)
        {
            m_skeletalMesh.skeleton.joints[jointId].translationAnimation.keyframeTimes.resize(timeAccessorEleCnt);

            memcpy(m_skeletalMesh.skeleton.joints[jointId].translationAnimation.keyframeTimes.data(),
                   &pTimeBufferData[timeBufferOffset],
                   timeBufferByteCnt);

            m_maxAnimTime = std::max(m_maxAnimTime,
                                     m_skeletalMesh.skeleton.joints[jointId].translationAnimation.keyframeTimes[timeAccessorEleCnt - 1]);

            const auto& translationAccessor = model.accessors[sampler.output];

            assert(translationAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, "The translation accessor component type should be float.");
            assert(translationAccessor.type == TINYGLTF_TYPE_VEC3, "The translation accessor type should be VEC3.");

            int translationAccessorByteOffset = translationAccessor.byteOffset;
            int translationAccessorEleCnt = translationAccessor.count;
            const auto& translationBufferView = model.bufferViews[translationAccessor.bufferView];

            int translationBufferOffset = translationAccessorByteOffset + translationBufferView.byteOffset;
            int translationBufferByteCnt = sizeof(float) * translationAccessorEleCnt * 3;

            m_skeletalMesh.skeleton.joints[jointId].translationAnimation.keyframeTransformationsData.resize(translationAccessorEleCnt * 3);

            const unsigned char* pTranslationBufferData = model.buffers[translationBufferView.buffer].data.data();
            memcpy(m_skeletalMesh.skeleton.joints[jointId].translationAnimation.keyframeTransformationsData.data(),
                   &pTranslationBufferData[translationBufferOffset], translationBufferByteCnt);
        }
        else if (target.compare("rotation") == 0)
        {
            m_skeletalMesh.skeleton.joints[jointId].rotationAnimation.keyframeTimes.resize(timeAccessorEleCnt);

            memcpy(m_skeletalMesh.skeleton.joints[jointId].rotationAnimation.keyframeTimes.data(),
                   &pTimeBufferData[timeBufferOffset],
                   timeBufferByteCnt);

            m_maxAnimTime = std::max(m_maxAnimTime,
                                     m_skeletalMesh.skeleton.joints[jointId].rotationAnimation.keyframeTimes[timeAccessorEleCnt - 1]);

            const auto& rotationAccessor = model.accessors[sampler.output];

            assert(rotationAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, "The rotation accessor component type should be float.");
            assert(rotationAccessor.type == TINYGLTF_TYPE_VEC4, "The rotation accessor type should be VEC4.");

            int rotationAccessorByteOffset = rotationAccessor.byteOffset;
            int rotationAccessorEleCnt = rotationAccessor.count;
            const auto& rotationBufferView = model.bufferViews[rotationAccessor.bufferView];

            int rotationBufferOffset = rotationAccessorByteOffset + rotationBufferView.byteOffset;
            int rotationBufferByteCnt = sizeof(float) * rotationAccessorEleCnt * 4;

            m_skeletalMesh.skeleton.joints[jointId].rotationAnimation.keyframeTransformationsData.resize(rotationAccessorEleCnt * 4);

            const unsigned char* pRotationBufferData = model.buffers[rotationBufferView.buffer].data.data();
            memcpy(m_skeletalMesh.skeleton.joints[jointId].rotationAnimation.keyframeTransformationsData.data(),
                   &pRotationBufferData[rotationBufferOffset],
                   rotationBufferByteCnt);
        }
        else
        {
            std::cerr << "The animation example only supports translation/rotation animation" << std::endl;
            exit(1);
        }
    }
    
}

// ================================================================================================================
void SkinAnimGltfApp::DestroyGltf()
{
    // Release mesh related resources
    Mesh& mesh = m_skeletalMesh.mesh;
    vmaDestroyBuffer(*m_pAllocator, mesh.idxBuffer.buffer, mesh.idxBuffer.bufferAlloc);
    vmaDestroyBuffer(*m_pAllocator, mesh.vertBuffer.buffer, mesh.vertBuffer.bufferAlloc);

    DestroyGpuImgResource(mesh.baseColorImg);

    // Release skeleton related resources
    Skeleton& skeleton = m_skeletalMesh.skeleton;
    for (uint32_t i = 0; i < m_swapchainImgCnt; i++)
    {
        DestroyGpuBufferResource(skeleton.jointsMatsBuffers[i]);
    }
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
    std::vector<VkDescriptorSetLayoutBinding> skinAnimRenderBindings;

    VkDescriptorSetLayoutBinding jointMatBinding{};
    {
        jointMatBinding.binding = 0;
        jointMatBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        jointMatBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        jointMatBinding.descriptorCount = 1;
    }
    skinAnimRenderBindings.push_back(jointMatBinding);

    VkDescriptorSetLayoutBinding diffuseIrradianceBinding{};
    {
        diffuseIrradianceBinding.binding = 1;
        diffuseIrradianceBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        diffuseIrradianceBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        diffuseIrradianceBinding.descriptorCount = 1;
    }
    skinAnimRenderBindings.push_back(diffuseIrradianceBinding);

    VkDescriptorSetLayoutBinding prefilterEnvBinding{};
    {
        prefilterEnvBinding.binding = 2;
        prefilterEnvBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        prefilterEnvBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        prefilterEnvBinding.descriptorCount = 1;
    }
    skinAnimRenderBindings.push_back(prefilterEnvBinding);

    VkDescriptorSetLayoutBinding envBrdfBinding{};
    {
        envBrdfBinding.binding = 3;
        envBrdfBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        envBrdfBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        envBrdfBinding.descriptorCount = 1;
    }
    skinAnimRenderBindings.push_back(envBrdfBinding);

    VkDescriptorSetLayoutBinding baseColorBinding{};
    {
        baseColorBinding.binding = 4;
        baseColorBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        baseColorBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        baseColorBinding.descriptorCount = 1;
    }
    skinAnimRenderBindings.push_back(baseColorBinding);

    VkDescriptorSetLayoutCreateInfo skinAnimRenderPipelineDesSetLayoutInfo{};
    {
        skinAnimRenderPipelineDesSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        skinAnimRenderPipelineDesSetLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
        skinAnimRenderPipelineDesSetLayoutInfo.bindingCount = skinAnimRenderBindings.size();
        skinAnimRenderPipelineDesSetLayoutInfo.pBindings = skinAnimRenderBindings.data();
    }

    VK_CHECK(vkCreateDescriptorSetLayout(m_device,
                                         &skinAnimRenderPipelineDesSetLayoutInfo,
                                         nullptr,
                                         &m_skinAnimPipelineDesSetLayout));
}

// ================================================================================================================
void SkinAnimGltfApp::InitSkinAnimPipelineLayout()
{
    std::vector<VkPushConstantRange> skinAnimPushConstantRanges(2);

    {
        skinAnimPushConstantRanges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        skinAnimPushConstantRanges[0].offset = 0;
        skinAnimPushConstantRanges[0].size = 16 * sizeof(float);
    }

    {
        skinAnimPushConstantRanges[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        skinAnimPushConstantRanges[1].offset = 0;
        skinAnimPushConstantRanges[1].size = 4 * sizeof(float); // Camera pos, Max IBL mipmap.
    }

    // Create pipeline layout
    // NOTE: pSetLayouts must not contain more than one descriptor set layout that was created with
    //       VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR set.
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    {
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_skinAnimPipelineDesSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 2;
        pipelineLayoutInfo.pPushConstantRanges = skinAnimPushConstantRanges.data();
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
    VkCommandBuffer cmdBuffer)
{
    std::vector<SharedLib::PushDescriptorInfo> skinAnimDescriptorsInfos;

    skinAnimDescriptorsInfos.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                         &m_skeletalMesh.skeleton.jointsMatsBuffers[m_acqSwapchainImgIdx].bufferDescInfo});
    
    skinAnimDescriptorsInfos.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                         &m_diffuseIrradianceCubemap.imageDescInfo });

    skinAnimDescriptorsInfos.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                         &m_prefilterEnvCubemap.imageDescInfo });

    skinAnimDescriptorsInfos.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                         &m_envBrdfImg.imageDescInfo });

    skinAnimDescriptorsInfos.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                         &m_skeletalMesh.mesh.baseColorImg });
    
    CmdAutoPushDescriptors(skinAnimDescriptorsInfos);
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
// Elements notes:
// [pos, normal, uv, weights, jointsIdx].
// [3 floats, 3 floats, 2 floats, 4 floats, 4 uints] --> 16 * sizeof(float).
VkPipelineVertexInputStateCreateInfo SkinAnimGltfApp::CreatePipelineVertexInputInfo()
{
    // Specifying all kinds of pipeline states
    // Vertex input state
    VkVertexInputBindingDescription* pVertBindingDesc = new VkVertexInputBindingDescription();
    memset(pVertBindingDesc, 0, sizeof(VkVertexInputBindingDescription));
    {
        pVertBindingDesc->binding = 0;
        pVertBindingDesc->stride = 16 * sizeof(float);
        pVertBindingDesc->inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    }
    m_heapMemPtrVec.push_back(pVertBindingDesc);

    VkVertexInputAttributeDescription* pVertAttrDescs = new VkVertexInputAttributeDescription[5];
    memset(pVertAttrDescs, 0, sizeof(VkVertexInputAttributeDescription) * 5);
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
        // Texcoord
        pVertAttrDescs[2].location = 2;
        pVertAttrDescs[2].binding = 0;
        pVertAttrDescs[2].format = VK_FORMAT_R32G32_SFLOAT;
        pVertAttrDescs[2].offset = 6 * sizeof(float);
        // Weights
        pVertAttrDescs[3].location = 3;
        pVertAttrDescs[3].binding = 0;
        pVertAttrDescs[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        pVertAttrDescs[3].offset = 8 * sizeof(float);
        // Joints indices
        pVertAttrDescs[4].location = 4;
        pVertAttrDescs[4].binding = 0;
        pVertAttrDescs[4].format = VK_FORMAT_R32G32B32A32_UINT;
        pVertAttrDescs[4].offset = 12 * sizeof(uint32_t);
    }
    m_heapArrayMemPtrVec.push_back(pVertAttrDescs);

    VkPipelineVertexInputStateCreateInfo vertInputInfo{};
    {
        vertInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertInputInfo.pNext = nullptr;
        vertInputInfo.vertexBindingDescriptionCount = 1;
        vertInputInfo.pVertexBindingDescriptions = pVertBindingDesc;
        vertInputInfo.vertexAttributeDescriptionCount = 5;
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
std::vector<float> SkinAnimGltfApp::GetSkinAnimPushConsant()
{
    // NOTE: Perspective Mat x View Mat x Model Mat x position.
    float vpMatData[16] = {};
    float tmpViewMatData[16] = {};
    float tmpPersMatData[16] = {};
    m_pCamera->GenViewPerspectiveMatrices(tmpViewMatData, tmpPersMatData, vpMatData);

    float fragPushConstantData[4] = {};
    float cameraPosData[3] = {};
    m_pCamera->GetPos(cameraPosData);
    memcpy(fragPushConstantData, cameraPosData, sizeof(cameraPosData));
    fragPushConstantData[3] = m_prefilterEnvMipsCnt;

    std::vector<float> pushConstData(vpMatData, vpMatData + 16);
    pushConstData.insert(pushConstData.end(), fragPushConstantData, fragPushConstantData + 4);

    return pushConstData;
}

// ================================================================================================================
float GetInterploationAndInterval(
    const std::vector<float>& keyframeTimes,
    const float curAnimTime,
    uint32_t& preIdx,
    uint32_t& postIdx)
{
    int idx0 = -1;
    int idx1 = -1;
    for (uint32_t i = 0; i < keyframeTimes.size(); i++)
    {
        float keyframeTime = keyframeTimes[i];
        if (curAnimTime > keyframeTime)
        {
            idx0 = i;
        }
        else
        {
            idx1 = i;
            break;
        }
    }

    if (idx0 == -1)
    {
        idx0 = idx1;
        idx1--;
        preIdx = idx0;
        postIdx = idx1;

        return 0.f;
    }
    else
    {
        preIdx = idx0;
        postIdx = idx1;

        float previousTime = keyframeTimes[preIdx];
        float nextTime = keyframeTimes[postIdx];
        
        float interpolationValue = (curAnimTime - previousTime) / (nextTime - previousTime);

        return interpolationValue;
    }
}

// ================================================================================================================
// A chain trans matrix transforms a point in the joint space to the world/model space.
// Joint i's joint matrix = Joint i's world (chain trans) matrix * joint i's inverse bind matrix.
// A model sapce vert multiples joint matrix generates a vert that is 100% connected/affected by the joint, so for
// a vertex affected by several joints, the vert final pos is the weight blend of these 100% affected vertices.
//
// The local transform matrix always has to be computed as M = T * R * S, where T is the matrix for the translation
// part, R is the matrix for the rotation part, and S is the matrix for the scale part.
void SkinAnimGltfApp::GenJointMatrix(
    float parentChainTransformationMat[16],
    uint32_t currentJoint,
    std::vector<float>& jointsMatBuffer)
{
    const auto& joint = m_skeletalMesh.skeleton.joints[currentJoint];

    float interpolatedTranslation[3];
    memcpy(interpolatedTranslation, joint.localTranslation, sizeof(interpolatedTranslation));

    glm::quat interpolatedRotQuat(joint.localRotation[3],
                                  joint.localRotation[0],
                                  joint.localRotation[1],
                                  joint.localRotation[2]);
    
    if (joint.translationAnimation.keyframeTimes.size() != 0)
    {
        uint32_t preIdx;
        uint32_t postIdx;
        float weight = GetInterploationAndInterval(joint.translationAnimation.keyframeTimes,
                                                   m_currentAnimTime, preIdx, postIdx);

        const std::vector<float>& translationAnimData = joint.translationAnimation.keyframeTransformationsData;

        float preTranslation[3] = { translationAnimData[preIdx * 3],
                                    translationAnimData[preIdx * 3 + 1],
                                    translationAnimData[preIdx * 3 + 2] };

        float postTranslation[3] = { translationAnimData[postIdx * 3],
                                     translationAnimData[postIdx * 3 + 1],
                                     translationAnimData[postIdx * 3 + 2] };

        // C++ 20
        interpolatedTranslation[0] = std::lerp(preTranslation[0], postTranslation[0], weight);
        interpolatedTranslation[1] = std::lerp(preTranslation[1], postTranslation[1], weight);
        interpolatedTranslation[2] = std::lerp(preTranslation[2], postTranslation[2], weight);
    }

    if (joint.rotationAnimation.keyframeTimes.size() != 0)
    {
        uint32_t preIdx;
        uint32_t postIdx;
        float weight = GetInterploationAndInterval(joint.rotationAnimation.keyframeTimes,
                                                   m_currentAnimTime, preIdx, postIdx);

        const std::vector<float>& rotationAnimData = joint.rotationAnimation.keyframeTransformationsData;

        glm::quat preRotQuat(rotationAnimData[preIdx * 4 + 3],
                             rotationAnimData[preIdx * 4],
                             rotationAnimData[preIdx * 4 + 1],
                             rotationAnimData[preIdx * 4 + 2]);

        glm::quat postRotQuat(rotationAnimData[postIdx * 4 + 3],
                              rotationAnimData[postIdx * 4],
                              rotationAnimData[postIdx * 4 + 1],
                              rotationAnimData[postIdx * 4 + 2]);

        interpolatedRotQuat = glm::slerp(preRotQuat, postRotQuat, weight);
    }

    glm::mat4 rotationMatrix = glm::toMat4(interpolatedRotQuat);
    glm::vec4 rotRow0 = glm::row(rotationMatrix, 0);
    glm::vec4 rotRow1 = glm::row(rotationMatrix, 1);
    glm::vec4 rotRow2 = glm::row(rotationMatrix, 2);

    float localTransformMat[16] = {
        rotRow0[0], rotRow0[1], rotRow0[2], interpolatedTranslation[0],
        rotRow1[0], rotRow1[1], rotRow1[2], interpolatedTranslation[1],
        rotRow2[0], rotRow2[1], rotRow2[2], interpolatedTranslation[2],
        0.f,        0.f,        0.f,        1.f
    };

    uint32_t jointMatStartIdx = 16 * currentJoint;
    memcpy(&jointsMatBuffer[jointMatStartIdx], localTransformMat, sizeof(localTransformMat));
}

// ================================================================================================================
void SkinAnimGltfApp::UpdateJointsTransAndMats()
{
    float modelMatData[16] = {
        1.f, 0.f, 0.f, ModelWorldPos[0],
        0.f, 1.f, 0.f, ModelWorldPos[1],
        0.f, 0.f, 1.f, ModelWorldPos[2],
        0.f, 0.f, 0.f, 1.f
    };

    std::vector<float> jointsMatBuffer(m_skeletalMesh.skeleton.joints.size() * 16);

    // Update all joints local transformation and generate the joint matrices for each joints into a RAM buffer.
    GenJointMatrix(modelMatData, 0, jointsMatBuffer);

    // Send the joint RAM buffer to the corresponding joint gpu buffer.
}

// ================================================================================================================
void SkinAnimGltfApp::FrameStart()
{
    GlfwApplication::FrameStart();
    UpdateCamera();

    // Update time stamp and the current anim time.
    if (m_currentAnimTime < 0.f)
    {
        // The first time render
        m_currentAnimTime = 0.f;
        m_lastAnimTimeStamp = std::chrono::high_resolution_clock::now();
    }
    else
    {
        const auto& thisTime = std::chrono::high_resolution_clock::now();
        auto durationMiliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(thisTime - m_lastTime).count();
        float delta = (float)durationMiliseconds / 1000.f; // Delta is in second.
        
        m_currentAnimTime += delta;
        if (m_currentAnimTime > m_maxAnimTime)
        {
            m_currentAnimTime -= m_maxAnimTime;
        }

        m_lastAnimTimeStamp = thisTime;
    }

    UpdateJointsTransAndMats();
}