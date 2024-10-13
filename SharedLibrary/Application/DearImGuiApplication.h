#pragma once
#include "GlfwApplication.h"
#include <vector>
#include "unordered_map"
#include <unordered_set>
#include <vector>

namespace SharedLib
{
    enum InputEnum
    {
        INVALID,
        PRESS_W,
        PRESS_S,
        PRESS_A,
        PRESS_D,
        PRESS_MOUSE_MIDDLE_BUTTON,
        MOUSE_MOVE
    };

    class ImGuiInput
    {
    public:
        ImGuiInput(InputEnum iInputEnum) :
            m_inputEnum(iInputEnum)
        {}

        ~ImGuiInput() {}

        void AddInt(uint32_t iInt) { m_payloadInts.push_back(iInt); }
        void AddFloat(float iFloat) { m_payloadFloats.push_back(iFloat); }

        std::vector<uint32_t> GetInts() { return m_payloadInts; }
        std::vector<float>    GetFloats() { return m_payloadFloats; }
        InputEnum             GetInputEnum() const { return m_inputEnum; }

    protected:
        InputEnum             m_inputEnum;
        std::vector<uint32_t> m_payloadInts;
        std::vector<float>    m_payloadFloats;
    };

    // Assmue that a command doesn't have any behavior. It's like a blob of binary data and different components can interpret it differently.
    struct CustomizedCommand
    {
        uint32_t              m_commandTypeUID;
        std::vector<uint32_t> m_payloadInts;
        std::vector<float>    m_payloadFloats;
    };

    // If we want to change the key combination of a command, we should reuse the CommandGenerator since each command
    // generator has a unique command type UID.
    class CommandGenerator
    {
    public:
        CommandGenerator() { m_cmdGenCmdTypeUID = m_commandTypeUIDCounter++; }
        ~CommandGenerator() {}

        friend class ImGuiInputHandler; // ImGuiInputHandler clears the command type UID counter when it's created.

        virtual CustomizedCommand GenerateCommand(const std::vector<ImGuiInput> inputs) = 0;
        bool CheckKeyCombination(const std::unordered_set<InputEnum>& curInputStates) { return curInputStates == m_keycombination; }
        std::unordered_set<InputEnum> GetKeyCombination() { return m_keycombination; }
        void SetKeyCombination(const std::unordered_set<InputEnum>& keyCombination) { m_keycombination = keyCombination; }

        bool CheckCmdTypeUID(uint32_t iCmdTypeUID) { return iCmdTypeUID == m_cmdGenCmdTypeUID; }
        uint32_t GetCmdTypeUID() { return m_cmdGenCmdTypeUID; }

    protected:
        ImGuiInput FindQualifiedInput(InputEnum iEnum, const std::vector<SharedLib::ImGuiInput> inputs);

        std::unordered_set<InputEnum> m_keycombination;
        static uint32_t               m_commandTypeUIDCounter;
        uint32_t                      m_cmdGenCmdTypeUID; 
    };

    // Input Handler is not responsible for the life cycle of the command generators, because we design for game engine and
    // the in different game levels, the set of command generators may be different.
    class ImGuiInputHandler
    {
    public:
        ImGuiInputHandler();
        ~ImGuiInputHandler();

        void AddOrUpdateCommandGenerator(CommandGenerator* pCommandGenerator);
        void RemoveCommandGenerator(CommandGenerator* pCommandGenerator){ m_commandGenerators.erase(pCommandGenerator); }

        std::vector<CustomizedCommand> HandleInput();

    protected:
        std::unordered_set<InputEnum> GenerateInputEnum(const std::vector<ImGuiInput>& inputs);

    private:
        std::unordered_set<CommandGenerator*> m_commandGenerators;
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

        virtual void ImGuiFrame(VkCommandBuffer cmdBuffer) = 0;

    protected:
        void InitImGui(); // Note that we need to use a command buffer to upload the font texture, so we need to call this after the command buffer is created.

        ImGuiInputHandler m_inputHandler;
        VkDescriptorPool  m_descriptorPool; // It's only used for ImGui.
        VkRenderPass      m_guiRenderPass;  // It's only used for ImGui.

        std::vector<VkFramebuffer> m_imGuiFramebuffers; // They are only used for ImGui.

    private:
    };
}
