#pragma once
#include "GlfwApplication.h"

namespace SharedLib
{
    // Vulkan application with a swapchain, glfwWindow and customizable DearImGui interface.
    // - Hide swapchain/glfw/ImGui operations.
    // - There is a func that holds the ImGui code.
    // - ImGUI based input handling.
    class ImGuiApplication : public GlfwApplication
    {
    public:
        ImGuiApplication();
        ~ImGuiApplication();

        virtual void AppInit() override { /* Unimplemented */ };

        virtual void FrameStart() override;
        virtual void FrameEnd() override;

        virtual void ImGuiFrame() = 0;

    protected:
        void InitImGui();


    private:
        VkDescriptorPool m_descriptorPool; // It's only used for ImGui.
        VkRenderPass     m_guiRenderPass;  // It's only used for ImGui.
    };
}
