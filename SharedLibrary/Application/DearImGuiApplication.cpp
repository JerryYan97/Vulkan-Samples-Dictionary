#include "DearImGuiApplication.h"
#include "../../../ThirdPartyLibs/DearImGUI/imgui.h"
#include "../../../ThirdPartyLibs/DearImGUI/backends/imgui_impl_glfw.h"
#include "../../../ThirdPartyLibs/DearImGUI/backends/imgui_impl_vulkan.h"

#include "VulkanDbgUtils.h"

namespace SharedLib
{
    // ================================================================================================================
    ImGuiApplication::ImGuiApplication()
    {
        
    }

    // ================================================================================================================
    ImGuiApplication::~ImGuiApplication()
    {
    }

    // ================================================================================================================
    void ImGuiApplication::InitImGui()
    {
        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();

        // Setup Platform/Renderer backends
        ImGui_ImplGlfw_InitForVulkan(m_pWindow, true);
        ImGui_ImplVulkan_InitInfo initInfo{};
        {
            initInfo.Instance = m_instance;
            initInfo.PhysicalDevice = m_physicalDevice;
            initInfo.Device = m_device;
            initInfo.QueueFamily = m_graphicsQueueFamilyIdx;
            initInfo.Queue = m_graphicsQueue;
            initInfo.DescriptorPool = m_descriptorPool;
            initInfo.Subpass = 0; // GUI render will use the first subpass.
            initInfo.MinImageCount = m_swapchainImgCnt;
            initInfo.ImageCount = m_swapchainImgCnt;
            initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
            initInfo.CheckVkResultFn = CheckVkResult;
        }
        ImGui_ImplVulkan_Init(&initInfo, guiRenderPass);
    }

    // ================================================================================================================
    void ImGuiApplication::FrameStart()
    {
        GlfwApplication::FrameStart();
    }

    // ================================================================================================================
    void ImGuiApplication::FrameEnd()
    {
        GlfwApplication::FrameEnd();
    }
}