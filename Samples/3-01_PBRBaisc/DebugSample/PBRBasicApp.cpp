#include "PBRBasicApp.h"
#include <glfw3.h>
#include "../../3-00_SharedLibrary/VulkanDbgUtils.h"
#include "../../3-00_SharedLibrary/Camera.h"
#include "../../3-00_SharedLibrary/Event.h"
#include "tiny_obj_loader.h"

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
PBRBasicApp::PBRBasicApp() : 
    GlfwApplication(),
    m_vsShaderModule(VK_NULL_HANDLE),
    m_psShaderModule(VK_NULL_HANDLE),
    m_pipelineDesSet0Layout(VK_NULL_HANDLE),
    m_pipelineLayout(VK_NULL_HANDLE),
    m_pipeline(VK_NULL_HANDLE)
{
    m_pCamera = new SharedLib::Camera();
}

// ================================================================================================================
PBRBasicApp::~PBRBasicApp()
{
    vkDeviceWaitIdle(m_device);
    delete m_pCamera;

    DestroyCameraUboObjects();

    // Destroy shader modules
    vkDestroyShaderModule(m_device, m_vsShaderModule, nullptr);
    vkDestroyShaderModule(m_device, m_psShaderModule, nullptr);

    // Destroy the pipeline
    vkDestroyPipeline(m_device, m_pipeline, nullptr);

    // Destroy the pipeline layout
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);

    // Destroy the descriptor set layout
    vkDestroyDescriptorSetLayout(m_device, m_pipelineDesSet0Layout, nullptr);
}

// ================================================================================================================
void PBRBasicApp::DestroyCameraUboObjects()
{
    for (uint32_t i = 0; i < SharedLib::MAX_FRAMES_IN_FLIGHT; i++)
    {
        vmaDestroyBuffer(*m_pAllocator, m_cameraParaBuffers[i], m_cameraParaBufferAllocs[i]);
    }
}

// ================================================================================================================
void PBRBasicApp::GetCameraData(
    float* pBuffer)
{
    
}

// ================================================================================================================
void PBRBasicApp::SendCameraDataToBuffer(
    uint32_t i)
{
    float cameraData[16] = {};
    m_pCamera->GetView(cameraData);
    m_pCamera->GetRight(&cameraData[4]);
    m_pCamera->GetUp(&cameraData[8]);
    m_pCamera->GetNearPlane(cameraData[12], cameraData[13], cameraData[14]);

    CopyRamDataToGpuBuffer(cameraData, m_cameraParaBuffers[i], m_cameraParaBufferAllocs[i], sizeof(cameraData));
}

// ================================================================================================================
void PBRBasicApp::UpdateCameraAndGpuBuffer()
{
    SharedLib::HEvent midMouseDownEvent = CreateMiddleMouseEvent(g_isDown);
    m_pCamera->OnEvent(midMouseDownEvent);
    SendCameraDataToBuffer(m_currentFrame);
}

// ================================================================================================================
void PBRBasicApp::InitCameraUboObjects()
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
void PBRBasicApp::ReadInSphereData()
{

}

// ================================================================================================================
void PBRBasicApp::InitSphereUboObjects()
{

}

// ================================================================================================================
void PBRBasicApp::DestroySphereUboObjects()
{

}

// ================================================================================================================
// TODO: I may need to put most the content in this function to CreateXXXX(...) in the parent class.
void PBRBasicApp::InitPipelineDescriptorSets()
{
    // Create pipeline descirptor
    VkDescriptorSetAllocateInfo skyboxPipelineDesSet0AllocInfo{};
    {
        skyboxPipelineDesSet0AllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        skyboxPipelineDesSet0AllocInfo.descriptorPool = m_descriptorPool;
        skyboxPipelineDesSet0AllocInfo.pSetLayouts = &m_pipelineDesSet0Layout;
        skyboxPipelineDesSet0AllocInfo.descriptorSetCount = 1;
    }
    
    m_pipelineDescriptorSet0s.resize(SharedLib::MAX_FRAMES_IN_FLIGHT);
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
void PBRBasicApp::InitPipelineLayout()
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
void PBRBasicApp::InitShaderModules()
{
    // Create Shader Modules.
    m_vsShaderModule = CreateShaderModule("./sphere_vert.spv");
    m_psShaderModule = CreateShaderModule("./sphere_frag.spv");
}


// ================================================================================================================
void PBRBasicApp::InitPipelineDescriptorSetLayout()
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
void PBRBasicApp::InitPipeline()
{
    VkPipelineRenderingCreateInfoKHR pipelineRenderCreateInfo{};
    {
        pipelineRenderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        pipelineRenderCreateInfo.colorAttachmentCount = 1;
        pipelineRenderCreateInfo.pColorAttachmentFormats = &m_choisenSurfaceFormat.format;
    }

    m_skyboxPipeline = CreateGfxPipeline(m_vsSkyboxShaderModule,
                                         m_psSkyboxShaderModule,
                                         pipelineRenderCreateInfo,
                                         m_skyboxPipelineLayout);
}

// ================================================================================================================
void PBRBasicApp::AppInit()
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
    
    // Create the graphics pipeline
    InitSkyboxShaderModules();
    InitSkyboxPipelineDescriptorSetLayout();
    InitSkyboxPipelineLayout();
    InitSkyboxPipeline();

    InitHdrRenderObjects();
    InitCameraUboObjects();
    InitSkyboxPipelineDescriptorSets();
    InitSwapchainSyncObjects();
}