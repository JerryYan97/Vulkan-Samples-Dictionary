#include "VulkanRTApp.h"
#include <glfw3.h>
#include <cstdlib>
#include <math.h>
#include <algorithm>
#include <chrono>
#include "../../../ThirdPartyLibs/DearImGUI/imgui.h"
#include "../../../ThirdPartyLibs/DearImGUI/backends/imgui_impl_glfw.h"
#include "../../../ThirdPartyLibs/DearImGUI/backends/imgui_impl_vulkan.h"
#include "../../../SharedLibrary/Utils/VulkanDbgUtils.h"
#include "../../../SharedLibrary/Utils/AppUtils.h"

#include "vk_mem_alloc.h"

/*
static PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
static PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
static PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
static PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
static PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
static PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
static PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
static PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
*/
// ================================================================================================================
VulkanRTApp::VulkanRTApp() :
    ImGuiApplication(),
    m_pCamera(nullptr)
{}

// ================================================================================================================
VulkanRTApp::~VulkanRTApp()
{
    vmaDestroyBuffer(*m_pAllocator, m_vertBuffer.buffer, m_vertBuffer.bufferAlloc);
    vmaDestroyBuffer(*m_pAllocator, m_idxBuffer.buffer, m_idxBuffer.bufferAlloc);
    vmaDestroyBuffer(*m_pAllocator, m_transformMatBuffer.buffer, m_transformMatBuffer.bufferAlloc);
}

// ================================================================================================================
void VulkanRTApp::AppInit()
{
    glfwInit();
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> instExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    instExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

    InitInstance(instExtensions, instExtensions.size());

    // Init glfw window.
    InitGlfwWindowAndCallbacks();

    // Create vulkan surface from the glfw window.
    VK_CHECK(glfwCreateWindowSurface(m_instance, m_pWindow, nullptr, &m_surface));

    InitPhysicalDevice();

    VkPhysicalDeviceAccelerationStructurePropertiesKHR phyDevAccStructProperties{};
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR    phyDevRtPipelineProperties{};
    SharedLib::GetVulkanRtPhyDeviceProperties(m_physicalDevice, &phyDevAccStructProperties, &phyDevRtPipelineProperties);

    InitGfxQueueFamilyIdx();
    InitPresentQueueFamilyIdx();

    // Queue family index should be unique in vk1.2:
    // https://vulkan.lunarg.com/doc/view/1.2.198.0/windows/1.2-extensions/vkspec.html#VUID-VkDeviceCreateInfo-queueFamilyIndex-02802
    std::vector<VkDeviceQueueCreateInfo> deviceQueueInfos = CreateDeviceQueueInfos({ m_graphicsQueueFamilyIdx,
                                                                                     m_presentQueueFamilyIdx });
    // Dummy device extensions vector. Swapchain, dynamic rendering and push descriptors are enabled by default.
    // We have tools that don't need the swapchain extension and the swapchain extension requires surface instance extensions.
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_QUERY_EXTENSION_NAME,
        VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
        VK_KHR_MAINTENANCE3_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_EXT_MESH_SHADER_EXTENSION_NAME, // NOTE: I don't know why the SPIRV compiled from hlsl needs it...
        VK_KHR_SPIRV_1_4_EXTENSION_NAME,
        VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME
    };

    // Enable raytracing features for the device.
    VkPhysicalDeviceRayQueryFeaturesKHR phyDevRayQueryFeatures = {};
    {
        phyDevRayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
        phyDevRayQueryFeatures.rayQuery = VK_TRUE;
    }

    VkPhysicalDeviceBufferDeviceAddressFeatures phyDeviceBufferDeviceAddrFeatures = {};
    {
        phyDeviceBufferDeviceAddrFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
        phyDeviceBufferDeviceAddrFeatures.pNext = &phyDevRayQueryFeatures;
        phyDeviceBufferDeviceAddrFeatures.bufferDeviceAddress = true;
        phyDeviceBufferDeviceAddrFeatures.bufferDeviceAddressCaptureReplay = false;
        phyDeviceBufferDeviceAddrFeatures.bufferDeviceAddressMultiDevice = false;
    }

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR phyDeviceRTPipelineFeatures = {};
    {
        phyDeviceRTPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
        phyDeviceRTPipelineFeatures.pNext = &phyDeviceBufferDeviceAddrFeatures;
        phyDeviceRTPipelineFeatures.rayTracingPipeline = true;
    }

    VkPhysicalDeviceAccelerationStructureFeaturesKHR phyDeviceAccStructureFeatures = {};
    {
        phyDeviceAccStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        phyDeviceAccStructureFeatures.pNext = &phyDeviceRTPipelineFeatures;
        phyDeviceAccStructureFeatures.accelerationStructure = true;
        phyDeviceAccStructureFeatures.accelerationStructureCaptureReplay = true;
        phyDeviceAccStructureFeatures.accelerationStructureIndirectBuild = false;
        phyDeviceAccStructureFeatures.accelerationStructureHostCommands = false;
        phyDeviceAccStructureFeatures.descriptorBindingAccelerationStructureUpdateAfterBind = false;
    }

    InitDevice(deviceExtensions, deviceQueueInfos, &phyDeviceAccStructureFeatures);
    InitKHRFuncPtrs();
    InitVmaAllocator(VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT);
    InitGraphicsQueue();
    InitPresentQueue();

    InitSwapchain();
    InitGfxCommandPool();
    InitGfxCommandBuffers(m_swapchainImgCnt);
    SwapchainColorImgsLayoutTrans(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    SwapchainDepthImgsLayoutTrans(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    InitImGui();
    
    InitSwapchainSyncObjects();

    InitVertIdxTransMatBuffers();

    InitRayTracingFuncPtrs();
}

// ================================================================================================================
void VulkanRTApp::ImGuiFrame(VkCommandBuffer cmdBuffer)
{
    // Start the Dear ImGui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::ShowDemoWindow();

    ImGui::Render();

    ImDrawData* drawData = ImGui::GetDrawData();
    const bool is_minimized = (drawData->DisplaySize.x <= 0.0f || drawData->DisplaySize.y <= 0.0f);

    // Begin the render pass and record relevant commands
    // Link framebuffer into the render pass
    // Note that the render pass doesn't clear the attachments, so the clear color is not used.
    VkRenderPassBeginInfo renderPassInfo{};
    {
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_guiRenderPass;
        renderPassInfo.framebuffer = m_imGuiFramebuffers[m_acqSwapchainImgIdx];
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = m_swapchainImageExtent;
    }
    vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Record the GUI rendering commands.
    ImGui_ImplVulkan_RenderDrawData(drawData, cmdBuffer);

    vkCmdEndRenderPass(cmdBuffer);
}

// ================================================================================================================
void VulkanRTApp::InitVertIdxTransMatBuffers()
{
    //
    //
    // BLAS - Bottom Level Acceleration Structure (Verts/Tris)
    //
    //
    const uint32_t numTriangles = 1;
    const uint32_t idxCount = 3;

    float vertices[9] = {
        1.f, 1.f, 0.f,
        -1.f, 1.f, 0.f,
        0.f, -1.f, 0.f
    };

    uint32_t indices[3] = {0, 1, 2};

    // Note: RT doesn't have perspective so it's just <3, 4> x <4, 1>
    VkTransformMatrixKHR transformMatrix = {
        1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f
    };

    VkBufferCreateInfo vertBufferInfo = {};
    {
        vertBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        vertBufferInfo.size = sizeof(vertices);
        vertBufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        vertBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo vertBufferAllocInfo = {};
    {
        vertBufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        vertBufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
                                    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    VK_CHECK(vmaCreateBuffer(*m_pAllocator, &vertBufferInfo, &vertBufferAllocInfo, &m_vertBuffer.buffer, &m_vertBuffer.bufferAlloc, nullptr));

    void* pVertGpuAddr = nullptr;
    vmaMapMemory(*m_pAllocator, m_vertBuffer.bufferAlloc, &pVertGpuAddr);
    memcpy(pVertGpuAddr, vertices, sizeof(vertices));
    vmaUnmapMemory(*m_pAllocator, m_vertBuffer.bufferAlloc);

    VkBufferCreateInfo idxBufferInfo = {};
    {
        idxBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        idxBufferInfo.size = sizeof(indices);
        idxBufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                              VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        idxBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo idxBufferAllocInfo = {};
    {
        idxBufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        idxBufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
                                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    vmaCreateBuffer(*m_pAllocator, &idxBufferInfo, &idxBufferAllocInfo, &m_idxBuffer.buffer, &m_idxBuffer.bufferAlloc, nullptr);

    // Transform matrix buffer
    VkBufferCreateInfo transformMatrixBufferInfo = {};
    {
        transformMatrixBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        transformMatrixBufferInfo.size = sizeof(transformMatrix);
        transformMatrixBufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        transformMatrixBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo transformMatrixAllocInfo = {};
    {
        transformMatrixAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        transformMatrixAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
                                         VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    vmaCreateBuffer(*m_pAllocator, &transformMatrixBufferInfo, &transformMatrixAllocInfo, &m_transformMatBuffer.buffer, &m_transformMatBuffer.bufferAlloc, nullptr);

    void* pTransformMatrixGpuAddr = nullptr;
    vmaMapMemory(*m_pAllocator, m_transformMatBuffer.bufferAlloc, &pTransformMatrixGpuAddr);
    memcpy(pTransformMatrixGpuAddr, transformMatrix.matrix, sizeof(transformMatrix));
    vmaUnmapMemory(*m_pAllocator, m_transformMatBuffer.bufferAlloc);
}

// ================================================================================================================
void VulkanRTApp::InitRayTracingFuncPtrs()
{
    /*
    vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(m_device, "vkGetAccelerationStructureBuildSizesKHR"));
    vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(m_device, "vkCreateAccelerationStructureKHR"));
    vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(m_device, "vkDestroyAccelerationStructureKHR"));
    vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(m_device, "vkGetAccelerationStructureDeviceAddressKHR"));
    vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(m_device, "vkCmdBuildAccelerationStructuresKHR"));
    vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(m_device, "vkCreateRayTracingPipelinesKHR"));
    vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(m_device, "vkGetRayTracingShaderGroupHandlesKHR"));
    vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(m_device, "vkCmdTraceRaysKHR"));
    */
}

// ================================================================================================================
void VulkanRTApp::CreateAccelerationStructures()
{
    struct BottomLevelAccelerationStructure
    {
        VkAccelerationStructureKHR accStructure;
        VkBuffer accStructureBuffer;
        VmaAllocation accStructureBufferAlloc;
        uint64_t deviceAddress;
        VkBuffer scratchBuffer;
        VmaAllocation scratchBufferAlloc;
    };
    BottomLevelAccelerationStructure bottomLevelAccelerationStructure;

    VkAccelerationStructureGeometryKHR blasGeometry = {};
    {
        blasGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        blasGeometry.geometryType = VkGeometryTypeKHR::VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        blasGeometry.flags = VkGeometryFlagBitsKHR::VK_GEOMETRY_OPAQUE_BIT_KHR;
        {
            VkAccelerationStructureGeometryDataKHR accStructureGeoData = {};
            VkAccelerationStructureGeometryTrianglesDataKHR accStructureGeoTriData = {};
            {
                accStructureGeoTriData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
                // accStructureGeoTriData.vertexFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
                accStructureGeoTriData.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
                accStructureGeoTriData.vertexData = SharedLib::GetVkDeviceOrHostAddressConstKHR(m_device, m_vertBuffer.buffer);
                accStructureGeoTriData.vertexStride = sizeof(float) * 3;
                accStructureGeoTriData.maxVertex = 2;
                accStructureGeoTriData.indexType = VK_INDEX_TYPE_UINT32;
                accStructureGeoTriData.indexData = SharedLib::GetVkDeviceOrHostAddressConstKHR(m_device, m_idxBuffer.buffer);
                accStructureGeoTriData.transformData = transformMatBufferDeviceAddr;
            }
            accStructureGeoData.triangles = accStructureGeoTriData;
            blasGeometry.geometry = accStructureGeoData;
        }
    }
}