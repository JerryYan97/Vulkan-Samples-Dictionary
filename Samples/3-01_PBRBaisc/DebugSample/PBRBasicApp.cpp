#include "PBRBasicApp.h"
#include <glfw3.h>
#include "../../3-00_SharedLibrary/VulkanDbgUtils.h"
#include "../../3-00_SharedLibrary/Camera.h"
#include "../../3-00_SharedLibrary/Event.h"

#define TINYOBJLOADER_IMPLEMENTATION
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
    m_pipelineDesSetLayout(VK_NULL_HANDLE),
    m_pipelineLayout(VK_NULL_HANDLE),
    m_pipeline(),
    m_lightPosBuffer(VK_NULL_HANDLE),
    m_lightPosBufferAlloc(VK_NULL_HANDLE)
{
    m_pCamera = new SharedLib::Camera();
}

// ================================================================================================================
PBRBasicApp::~PBRBasicApp()
{
    vkDeviceWaitIdle(m_device);
    delete m_pCamera;

    DestroyCameraUboObjects();
    DestroyLightsUboObjects();

    // Destroy shader modules
    vkDestroyShaderModule(m_device, m_vsShaderModule, nullptr);
    vkDestroyShaderModule(m_device, m_psShaderModule, nullptr);

    // Destroy the pipeline layout
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);

    // Destroy the descriptor set layout
    vkDestroyDescriptorSetLayout(m_device, m_pipelineDesSetLayout, nullptr);
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

    // Copy camera data to ubo buffer
    for (uint32_t i = 0; i < SharedLib::MAX_FRAMES_IN_FLIGHT; i++)
    {
        SendCameraDataToBuffer(i);
    }
}

// ================================================================================================================
void PBRBasicApp::ReadInSphereData()
{
    std::string inputfile = SOURCE_PATH;
    inputfile += "/../data/uvNormalSphere.obj";
    
    tinyobj::ObjReaderConfig readerConfig;
    tinyobj::ObjReader sphereObjReader;

    sphereObjReader.ParseFromFile(inputfile, readerConfig);

    auto& shapes = sphereObjReader.GetShapes();
    auto& attrib = sphereObjReader.GetAttrib();

    m_pVertData = static_cast<float*>(malloc(sizeof(float) * attrib.vertices.size() * 2));

    // We assume that this test only has one shape
    assert(shapes.size() == 1, "This application only accepts one shape!");
    m_pIdxData = static_cast<uint32_t*>(malloc(sizeof(uint32_t) * (shapes[0].mesh.indices.size())));

    for (uint32_t s = 0; s < shapes.size(); s++)
    {
        // Loop over faces(polygon)
        uint32_t index_offset = 0;
        for (uint32_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
        {
            uint32_t fv = shapes[s].mesh.num_face_vertices[f];

            // Loop over vertices in the face.
            for (uint32_t v = 0; v < fv; v++)
            {
                // Access to vertex
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

                // We use the vertex index of the tinyObj as our vertex buffer's vertex index.
                m_pIdxData[index_offset + v] = uint32_t(idx.vertex_index);

                uint32_t vx = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
                uint32_t vy = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
                uint32_t vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

                // Transfer the vertex buffer's vertex index to the element index -- 6 * vertex index + xxx;
                m_pVertData[6 * size_t(idx.vertex_index) + 0] = vx;
                m_pVertData[6 * size_t(idx.vertex_index) + 1] = vy;
                m_pVertData[6 * size_t(idx.vertex_index) + 2] = vz;

                // Check if `normal_index` is zero or positive. negative = no normal data
                assert(idx.normal_index >= 0, "The model doesn't have normal information but it is necessary.");
                uint32_t nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
                uint32_t ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
                uint32_t nz = attrib.normals[3 * size_t(idx.normal_index) + 2];

                m_pVertData[6 * size_t(idx.vertex_index) + 3] = nx;
                m_pVertData[6 * size_t(idx.vertex_index) + 4] = ny;
                m_pVertData[6 * size_t(idx.vertex_index) + 5] = nz;
            }
            index_offset += fv;
        }
    }
}

// ================================================================================================================
void PBRBasicApp::InitSphereVertexIndexBuffers()
{

}

// ================================================================================================================
void PBRBasicApp::DestroySphereVertexIndexBuffers()
{

}

// ================================================================================================================
void PBRBasicApp::InitLightsUboObjects()
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

    vmaCreateBuffer(
        *m_pAllocator,
        &bufferInfo,
        &bufferAllocInfo,
        &m_lightPosBuffer,
        &m_lightPosBufferAlloc,
        nullptr);

    // Copy lights data to ubo buffer
    // The last element of each lines is a padding float
    float lightPos[16] = {
        -1.f,  1.f, -1.f, 0.f,
        -1.f,  1.f,  1.f, 0.f,
        -1.f, -1.f, -1.f, 0.f,
        -1.f, -1.f,  1.f, 0.f
    };

    CopyRamDataToGpuBuffer(lightPos, m_lightPosBuffer, m_lightPosBufferAlloc, sizeof(lightPos));
}

// ================================================================================================================
void PBRBasicApp::DestroyLightsUboObjects()
{
    vmaDestroyBuffer(*m_pAllocator, m_lightPosBuffer, m_lightPosBufferAlloc);
}

// ================================================================================================================
// TODO: I may need to put most the content in this function to CreateXXXX(...) in the parent class.
void PBRBasicApp::InitPipelineDescriptorSets()
{
    // Create pipeline descirptor
    VkDescriptorSetAllocateInfo pipelineDesSet0AllocInfo{};
    {
        pipelineDesSet0AllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        pipelineDesSet0AllocInfo.descriptorPool = m_descriptorPool;
        pipelineDesSet0AllocInfo.pSetLayouts = &m_pipelineDesSetLayout;
        pipelineDesSet0AllocInfo.descriptorSetCount = 1;
    }

    m_pipelineDescriptorSet0s.resize(SharedLib::MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < SharedLib::MAX_FRAMES_IN_FLIGHT; i++)
    {
        VK_CHECK(vkAllocateDescriptorSets(m_device,
                                          &pipelineDesSet0AllocInfo,
                                          &m_pipelineDescriptorSet0s[i]));
    }

    VkDescriptorBufferInfo desLightsBufInfo{};
    {
        desLightsBufInfo.buffer = m_lightPosBuffer;
        desLightsBufInfo.offset = 0;
        desLightsBufInfo.range = sizeof(float) * 16;
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
            writeCameraBufDesSet.dstSet = m_pipelineDescriptorSet0s[i];
            writeCameraBufDesSet.dstBinding = 0;
            writeCameraBufDesSet.descriptorCount = 1;
            writeCameraBufDesSet.pBufferInfo = &desCameraParaBufInfo;
        }

        VkWriteDescriptorSet writeLightsDesSet{};
        {
            writeLightsDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeLightsDesSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writeLightsDesSet.dstSet = m_pipelineDescriptorSet0s[i];
            writeLightsDesSet.dstBinding = 0;
            writeLightsDesSet.descriptorCount = 1;
            writeLightsDesSet.pBufferInfo = &desLightsBufInfo;
        }

        // Linking skybox pipeline descriptors: skybox cubemap and camera buffer descriptors to their GPU memory
        // and info.
        VkWriteDescriptorSet writeSkyboxPipelineDescriptors[2] = { writeLightsDesSet, writeCameraBufDesSet };
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
        pipelineLayoutInfo.pSetLayouts = &m_pipelineDesSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
    }
    
    VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout));
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
        cameraUboBinding.binding = 0;
        cameraUboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        cameraUboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        cameraUboBinding.descriptorCount = 1;
    }

    // Binding for the lights
    VkDescriptorSetLayoutBinding lightsUboBinding{};
    {
        cameraUboBinding.binding = 0;
        cameraUboBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        cameraUboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        cameraUboBinding.descriptorCount = 1;
    }

    // Create pipeline's descriptors layout
    VkDescriptorSetLayoutBinding pipelineDesSetLayoutBindings[2] = { cameraUboBinding, lightsUboBinding };
    VkDescriptorSetLayoutCreateInfo pipelineDesSetLayoutInfo{};
    {
        pipelineDesSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        pipelineDesSetLayoutInfo.bindingCount = 2;
        pipelineDesSetLayoutInfo.pBindings = pipelineDesSetLayoutBindings;
    }

    VK_CHECK(vkCreateDescriptorSetLayout(m_device,
                                         &pipelineDesSetLayoutInfo,
                                         nullptr,
                                         &m_pipelineDesSetLayout));
}

// ================================================================================================================
VkPipelineVertexInputStateCreateInfo PBRBasicApp::CreatePipelineVertexInputInfo()
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
void PBRBasicApp::InitPipeline()
{
    
    VkPipelineRenderingCreateInfoKHR pipelineRenderCreateInfo{};
    {
        pipelineRenderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        pipelineRenderCreateInfo.colorAttachmentCount = 1;
        pipelineRenderCreateInfo.pColorAttachmentFormats = &m_choisenSurfaceFormat.format;
        pipelineRenderCreateInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;
    }

    m_pipeline.SetPNext(&pipelineRenderCreateInfo);
    m_pipeline.SetPipelineLayout(m_pipelineLayout);

    VkPipelineVertexInputStateCreateInfo vertInputInfo = CreatePipelineVertexInputInfo();
    m_pipeline.SetVertexInputInfo(&vertInputInfo);

    VkPipelineShaderStageCreateInfo shaderStgsInfo[2] = {};
    shaderStgsInfo[0] = CreateDefaultShaderStgCreateInfo(m_vsShaderModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStgsInfo[1] = CreateDefaultShaderStgCreateInfo(m_psShaderModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    m_pipeline.SetShaderStageInfo(shaderStgsInfo, 2);

    m_pipeline.CreatePipeline(m_device);
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
    ReadInSphereData();

    if (m_pVertData != nullptr)
    {
        free(m_pVertData);
        m_pVertData = nullptr;
    }

    if (m_pIdxData != nullptr)
    {
        free(m_pIdxData);
        m_pIdxData = nullptr;
    }

    InitShaderModules();
    InitPipelineDescriptorSetLayout();
    InitPipelineLayout();
    InitPipeline();

    InitCameraUboObjects();
    InitLightsUboObjects();
    InitPipelineDescriptorSets();

    /*
    InitSkyboxShaderModules();
    InitSkyboxPipelineDescriptorSetLayout();
    InitSkyboxPipelineLayout();
    InitSkyboxPipeline();

    InitHdrRenderObjects();
    
    InitSkyboxPipelineDescriptorSets();
    InitSwapchainSyncObjects();
    */
}