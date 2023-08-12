#include "SphericalToCubemap.h"
#include <glfw3.h>
#include "../../SharedLibrary/Utils/VulkanDbgUtils.h"
#include "../../SharedLibrary/Camera/Camera.h"

#include "vk_mem_alloc.h"

// ================================================================================================================
SphericalToCubemap::SphericalToCubemap() :
    GlfwApplication(),
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
    m_pipelineDesSetLayout(VK_NULL_HANDLE),
    m_pipelineLayout(VK_NULL_HANDLE),
    m_pipeline()
{
    m_pCamera = new SharedLib::Camera();
}

// ================================================================================================================
SphericalToCubemap::~SphericalToCubemap()
{
    vkDeviceWaitIdle(m_device);
    delete m_pCamera;
}

// ================================================================================================================
void SphericalToCubemap::AppInit()
{
    glfwInit();
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> instExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    InitInstance(instExtensions, glfwExtensionCount);

    // Init glfw window.
    InitGlfwWindowAndCallbacks();

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
}