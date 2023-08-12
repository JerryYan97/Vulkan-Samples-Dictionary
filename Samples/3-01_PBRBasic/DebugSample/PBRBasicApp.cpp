#include "PBRBasicApp.h"
#include <glfw3.h>
#include "../../3-00_SharedLibrary/VulkanDbgUtils.h"
#include "../../3-00_SharedLibrary/Camera.h"
#include "../../3-00_SharedLibrary/Event.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include "vk_mem_alloc.h"

// ================================================================================================================
PBRBasicApp::PBRBasicApp() : 
    GlfwApplication(),
    m_vsShaderModule(VK_NULL_HANDLE),
    m_psShaderModule(VK_NULL_HANDLE),
    m_pipelineDesSetLayout(VK_NULL_HANDLE),
    m_pipelineLayout(VK_NULL_HANDLE),
    m_pipeline(),
    m_lightPosBuffer(VK_NULL_HANDLE),
    m_lightPosBufferAlloc(VK_NULL_HANDLE),
    m_vpUboBuffer(VK_NULL_HANDLE),
    m_vpUboAlloc(VK_NULL_HANDLE),
    m_idxBuffer(VK_NULL_HANDLE),
    m_idxBufferAlloc(VK_NULL_HANDLE),
    m_vertBuffer(VK_NULL_HANDLE),
    m_vertBufferAlloc(VK_NULL_HANDLE),
    m_vertBufferByteCnt(0),
    m_idxBufferByteCnt(0)
{
    m_pCamera = new SharedLib::Camera();
}

// ================================================================================================================
PBRBasicApp::~PBRBasicApp()
{
    vkDeviceWaitIdle(m_device);
    delete m_pCamera;

    DestroySphereVertexIndexBuffers();

    DestroyVpUboObjects();
    DestroyFragUboObjects();

    // Destroy shader modules
    vkDestroyShaderModule(m_device, m_vsShaderModule, nullptr);
    vkDestroyShaderModule(m_device, m_psShaderModule, nullptr);

    // Destroy the pipeline layout
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);

    // Destroy the descriptor set layout
    vkDestroyDescriptorSetLayout(m_device, m_pipelineDesSetLayout, nullptr);
}

// ================================================================================================================
void PBRBasicApp::DestroyVpUboObjects()
{
    vmaDestroyBuffer(*m_pAllocator, m_vpUboBuffer, m_vpUboAlloc);
}

// ================================================================================================================
void PBRBasicApp::InitVpUboObjects()
{
    // The alignment of a vec3 is 4 floats and the element alignment of a struct is the largest element alignment,
    // which is also the 4 float. Therefore, we need 32 floats as the buffer to store the VP's parameters.
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

    vmaCreateBuffer(*m_pAllocator,
                    &bufferInfo,
                    &bufferAllocInfo,
                    &m_vpUboBuffer,
                    &m_vpUboAlloc,
                    nullptr);

    float vpMat[16]   = {};
    float tmpViewMat[16]  = {};
    float tmpPersMat[16] = {};
    m_pCamera->GenViewPerspectiveMatrices(tmpViewMat, tmpPersMat, vpMat);
    SharedLib::MatTranspose(vpMat, 4);

    CopyRamDataToGpuBuffer(vpMat, m_vpUboBuffer, m_vpUboAlloc, 16 * sizeof(float));
}

// ================================================================================================================
// NOTE: A vert = pos + normal + uv.
void PBRBasicApp::ReadInSphereData()
{
    std::string inputfile = SOURCE_PATH;
    inputfile += "/../data/uvNormalSphere.obj";
    // inputfile += "/../data/normalCube.obj";
    
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
                m_vertData.push_back(vx);
                m_vertData.push_back(vy);
                m_vertData.push_back(vz);

                // Check if `normal_index` is zero or positive. negative = no normal data
                assert(idx.normal_index >= 0, "The model doesn't have normal information but it is necessary.");
                float nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
                float ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
                float nz = attrib.normals[3 * size_t(idx.normal_index) + 2];

                m_vertData.push_back(nx);
                m_vertData.push_back(ny);
                m_vertData.push_back(nz);

                m_idxData.push_back(idxBufIdx);
                idxBufIdx++;
            }
            index_offset += fv;
        }
    }

    m_vertBufferByteCnt = m_vertData.size() * sizeof(float);
    m_idxBufferByteCnt = m_idxData.size() * sizeof(uint32_t);
}

// ================================================================================================================
void PBRBasicApp::InitSphereVertexIndexBuffers()
{
    // Create sphere data GPU buffers
    VkBufferCreateInfo vertBufferInfo{};
    {
        vertBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        vertBufferInfo.size = m_vertBufferByteCnt;
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
        idxBufferInfo.size = m_idxBufferByteCnt;
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
    CopyRamDataToGpuBuffer(m_vertData.data(), m_vertBuffer, m_vertBufferAlloc, m_vertBufferByteCnt);
    CopyRamDataToGpuBuffer(m_idxData.data(), m_idxBuffer, m_idxBufferAlloc, m_idxBufferByteCnt);
}

// ================================================================================================================
void PBRBasicApp::DestroySphereVertexIndexBuffers()
{
    vmaDestroyBuffer(*m_pAllocator, m_vertBuffer, m_vertBufferAlloc);
    vmaDestroyBuffer(*m_pAllocator, m_idxBuffer, m_idxBufferAlloc);
}

// ================================================================================================================
void PBRBasicApp::InitFragUboObjects()
{
    // The alignment of a vec3 is 4 floats and the element alignment of a struct is the largest element alignment,
    // which is also the 4 float. Therefore, we need 16 floats as the buffer to store the Camera's parameters.
    VkBufferCreateInfo bufferInfo{};
    {
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = 20 * sizeof(float);
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

    float cameraPos[3] = {};
    m_pCamera->GetPos(cameraPos);

    // Copy light and camera data to ubo buffer
    // The last element of each lines is a padding float
    float lightPos[20] = {
        10.f,  3.f, -8.f, 0.f,
        10.f,  3.f,  8.f, 0.f,
        10.f, -3.f, -8.f, 0.f,
        10.f, -3.f,  8.f, 0.f,
        cameraPos[0], cameraPos[1], cameraPos[2], 0.f
    };

    CopyRamDataToGpuBuffer(lightPos, m_lightPosBuffer, m_lightPosBufferAlloc, sizeof(lightPos));
}

// ================================================================================================================
void PBRBasicApp::DestroyFragUboObjects()
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

    VkDescriptorBufferInfo desVpParaBufInfo{};
    {
        desVpParaBufInfo.buffer = m_vpUboBuffer;
        desVpParaBufInfo.offset = 0;
        desVpParaBufInfo.range = sizeof(float) * 16;
    }

    // I believe we can use the same descriptor but I am a little bit lazy to change to that...
    for (uint32_t i = 0; i < SharedLib::MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkWriteDescriptorSet writeVpBufDesSet{};
        {
            writeVpBufDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeVpBufDesSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writeVpBufDesSet.dstSet = m_pipelineDescriptorSet0s[i];
            writeVpBufDesSet.dstBinding = 0;
            writeVpBufDesSet.descriptorCount = 1;
            writeVpBufDesSet.pBufferInfo = &desVpParaBufInfo;
        }

        VkWriteDescriptorSet writeLightsDesSet{};
        {
            writeLightsDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeLightsDesSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writeLightsDesSet.dstSet = m_pipelineDescriptorSet0s[i];
            writeLightsDesSet.dstBinding = 1;
            writeLightsDesSet.descriptorCount = 1;
            writeLightsDesSet.pBufferInfo = &desLightsBufInfo;
        }

        // Linking pipeline descriptors: camera buffer descriptors to their GPU memory and info.
        VkWriteDescriptorSet writeSkyboxPipelineDescriptors[2] = { writeLightsDesSet, writeVpBufDesSet };
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
        lightsUboBinding.binding = 1;
        lightsUboBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        lightsUboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        lightsUboBinding.descriptorCount = 1;
    }

    // Create pipeline's descriptors layout
    // The Vulkan spec states: The VkDescriptorSetLayoutBinding::binding members of the elements of the pBindings array 
    // must each have different values 
    // (https://vulkan.lunarg.com/doc/view/1.3.236.0/windows/1.3-extensions/vkspec.html#VUID-VkDescriptorSetLayoutCreateInfo-binding-00279)
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
VkPipelineDepthStencilStateCreateInfo PBRBasicApp::CreateDepthStencilStateInfo()
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
void PBRBasicApp::InitPipeline()
{
    VkPipelineRenderingCreateInfoKHR pipelineRenderCreateInfo{};
    {
        pipelineRenderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        pipelineRenderCreateInfo.colorAttachmentCount = 1;
        pipelineRenderCreateInfo.pColorAttachmentFormats = &m_choisenSurfaceFormat.format;
        pipelineRenderCreateInfo.depthAttachmentFormat = VK_FORMAT_D16_UNORM;
    }

    m_pipeline.SetPNext(&pipelineRenderCreateInfo);
    m_pipeline.SetPipelineLayout(m_pipelineLayout);

    VkPipelineVertexInputStateCreateInfo vertInputInfo = CreatePipelineVertexInputInfo();
    m_pipeline.SetVertexInputInfo(&vertInputInfo);

    VkPipelineDepthStencilStateCreateInfo depthStencilInfo = CreateDepthStencilStateInfo();
    m_pipeline.SetDepthStencilStateInfo(&depthStencilInfo);

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
    InitSphereVertexIndexBuffers();

    /*
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
    */

    InitShaderModules();
    InitPipelineDescriptorSetLayout();
    InitPipelineLayout();
    InitPipeline();

    InitVpUboObjects();
    InitFragUboObjects();
    InitPipelineDescriptorSets();
    InitSwapchainSyncObjects();
}