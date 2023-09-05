#include "GenIBL.h"
#include "../../SharedLibrary/Utils/VulkanDbgUtils.h"
#include "../../SharedLibrary/Camera/Camera.h"
#include "../../SharedLibrary/Event/Event.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STBI_MSC_SECURE_CRT
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

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
    m_diffuseIrradianceCubemapImageView(VK_NULL_HANDLE)
{
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
}

// ================================================================================================================
void GenIBL::DestroyInputCubemapRenderObjs()
{
    vmaDestroyImage(*m_pAllocator, m_hdrCubeMapImage, m_hdrCubeMapAlloc);
    vkDestroyImageView(m_device, m_hdrCubeMapView, nullptr);
    vkDestroySampler(m_device, m_hdrCubeMapSampler, nullptr);
}

// ================================================================================================================
void GenIBL::ReadInCubemap(
    const std::string& namePath)
{
    int nrComponents, width, height;
    m_hdrCubeMapInfo.pData = stbi_loadf(namePath.c_str(), &width, &height, &nrComponents, 0);

    m_hdrCubeMapInfo.width = (uint32_t)width;
    m_hdrCubeMapInfo.height = (uint32_t)height;
}

// ================================================================================================================
void GenIBL::InitInputCubemapObjects()
{
    
}

// ================================================================================================================
void GenIBL::InitCameraScreenUbo()
{
    // Allocate the GPU buffer

    // Prepare data

    // Send data to the GPU buffer
}

// ================================================================================================================
void GenIBL::DestroyCameraScreenUbo()
{
    vmaDestroyBuffer(*m_pAllocator, m_uboCameraScreenBuffer, m_uboCameraScreenAlloc);
}

// ================================================================================================================
void GenIBL::DumpDiffIrradianceImg(
    const std::string& outputPath)
{

}


// ================================================================================================================
void GenIBL::InitDiffIrrPreFilterEnvMapDescriptorSetLayout()
{

}

// ================================================================================================================
void GenIBL::InitDiffIrrPreFilterEnvMapDescriptorSets()
{

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
    InitDiffuseIrradianceOutputObjects();
    InitDiffuseIrradianceShaderModules();
    InitDiffuseIrradiancePipelineLayout();
    InitDiffuseIrradiancePipeline();

    // Pipeline and resources for the prefilter environment map gen.

    // Pipeline and resources for the environment brdf map gen.
}