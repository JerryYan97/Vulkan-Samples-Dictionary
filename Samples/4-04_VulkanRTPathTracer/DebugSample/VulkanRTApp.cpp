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

// ================================================================================================================
VulkanRTApp::VulkanRTApp() :
    ImGuiApplication(),
    m_pCamera(nullptr)
{}

// ================================================================================================================
VulkanRTApp::~VulkanRTApp()
{
    
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
    InitDevice(deviceExtensions, deviceQueueInfos, nullptr);
    InitKHRFuncPtrs();
    InitVmaAllocator();
    InitGraphicsQueue();
    InitPresentQueue();

    InitSwapchain();
    InitGfxCommandPool();
    InitGfxCommandBuffers(m_swapchainImgCnt);
    SwapchainColorImgsLayoutTrans(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    SwapchainDepthImgsLayoutTrans(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    InitImGui();
    
    InitSwapchainSyncObjects();
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