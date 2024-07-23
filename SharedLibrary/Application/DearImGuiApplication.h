#pragma once
#include "GlfwApplication.h"

namespace SharedLib
{
    class ImGuiInputCommand
    {
    public:
        ImGuiInputCommand() {}
        ~ImGuiInputCommand() {}

        virtual void Execute(class ImGuiApplication* pImGuiApp) {}
    protected:

    private:
    };

    class ImGuiInputHandler
    {
    public:
        ImGuiInputHandler() {}
        ~ImGuiInputHandler() {}

        ImGuiInputCommand* HandleInput() {}
    protected:

    private:
        ImGuiInputCommand* m_pPressW;
        ImGuiInputCommand* m_pPressS;
        ImGuiInputCommand* m_pPressA;
        ImGuiInputCommand* m_pPressD;
    };


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

        ImGuiInputHandler inputHandler;

    private:
        VkDescriptorPool m_descriptorPool; // It's only used for ImGui.
        VkRenderPass     m_guiRenderPass;  // It's only used for ImGui.

        std::vector<VkFramebuffer> m_imGuiFramebuffers; // They are only used for ImGui.
    };
}
