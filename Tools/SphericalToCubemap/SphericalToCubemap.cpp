#include "SphericalToCubemap.h"
#include <glfw3.h>
#include "../../SharedLibrary/Utils/VulkanDbgUtils.h"
#include "../../SharedLibrary/Camera/Camera.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <cassert>

#include "vk_mem_alloc.h"

// ================================================================================================================
SphericalToCubemap::SphericalToCubemap() :
    Application(),
    m_pCamera(nullptr),
    m_uboBuffer(VK_NULL_HANDLE),
    m_uboAlloc(VK_NULL_HANDLE),
    m_inputHdri(VK_NULL_HANDLE),
    m_inputHdriAlloc(VK_NULL_HANDLE),
    m_inputHdriImageView(VK_NULL_HANDLE),
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
    m_outputCubemapExtent()
{
    m_pCamera = new SharedLib::Camera();
}

// ================================================================================================================
SphericalToCubemap::~SphericalToCubemap()
{
    vkDeviceWaitIdle(m_device);
    delete m_pCamera;

    DestroyHdriGpuObjects();

    // Destroy shader modules
    vkDestroyShaderModule(m_device, m_vsShaderModule, nullptr);
    vkDestroyShaderModule(m_device, m_psShaderModule, nullptr);

    // Destroy the pipeline layout
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);

    // Destroy the descriptor set layout
    vkDestroyDescriptorSetLayout(m_device, m_pipelineDesSet0Layout, nullptr);
}

// ================================================================================================================
void SphericalToCubemap::DestroyHdriGpuObjects()
{
    vkDestroyImageView(m_device, m_inputHdriImageView, nullptr);
    vkDestroyImageView(m_device, m_outputCubemapImageView, nullptr);
    vmaDestroyImage(*m_pAllocator, m_inputHdri, m_inputHdriAlloc);
    vmaDestroyImage(*m_pAllocator, m_outputCubemap, m_outputCubemapAlloc);
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
void SphericalToCubemap::CreateHdriGpuObjects()
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
}

// ================================================================================================================
void SphericalToCubemap::InitPipelineDescriptorSetLayout()
{
    VkDescriptorSetLayoutCreateInfo pipelineDesSet0LayoutInfo{};
    {
        pipelineDesSet0LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
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
        // pipelineLayoutInfo.setLayoutCount = 1;
        // pipelineLayoutInfo.pSetLayouts = &m_skyboxPipelineDesSet0Layout;
        // pipelineLayoutInfo.pushConstantRangeCount = 0;
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
void SphericalToCubemap::InitPipelineDescriptorSets()
{

}

// ================================================================================================================
void SphericalToCubemap::InitPipeline()
{
    VkFormat colorAttachmentFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
    VkPipelineRenderingCreateInfoKHR pipelineRenderCreateInfo{};
    {
        pipelineRenderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
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

    // sizeof(VkPhysicalDeviceDynamicRenderingFeaturesKHR);
    // sizeof(VkPhysicalDeviceVulkan11Features);

    // void* pNext[2] = { &dynamicRenderingFeature, &vulkan11Features };

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
}