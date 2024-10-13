#include "DearImGuiApplication.h"
#include "../../../SharedLibrary/Utils/VulkanDbgUtils.h"
#include "../../../ThirdPartyLibs/DearImGUI/imgui.h"
#include "../../../ThirdPartyLibs/DearImGUI/backends/imgui_impl_glfw.h"
#include "../../../ThirdPartyLibs/DearImGUI/backends/imgui_impl_vulkan.h"

#include "VulkanDbgUtils.h"

namespace SharedLib
{
    uint32_t CommandGenerator::m_commandTypeUIDCounter = 0;

    // ================================================================================================================
    ImGuiInputHandler::ImGuiInputHandler()
    {
    }

    // ================================================================================================================
    ImGuiInputHandler::~ImGuiInputHandler()
    {
        for (auto& itr : m_commandGenerators)
        {
            delete itr;
        }

        m_commandGenerators.clear();
    }

    // ================================================================================================================
    void ImGuiInputHandler::AddOrUpdateCommandGenerator(
        CommandGenerator*  pCommandGenerator)
    {
        if (pCommandGenerator == nullptr)
        {
            return;
        }

        for (auto& itr : m_commandGenerators)
        {
            if (itr->GetKeyCombination() == pCommandGenerator->GetKeyCombination())
            {
                m_commandGenerators.erase(itr);
                delete itr;
                return;
            }
        }

        m_commandGenerators.insert(pCommandGenerator);
    }

    // ================================================================================================================
    std::unordered_set<InputEnum> ImGuiInputHandler::GenerateInputEnum(
        const std::vector<ImGuiInput>& inputs)
    {
        std::unordered_set<InputEnum> inputEnums;
        for (const auto& input : inputs)
        {
            inputEnums.insert(input.GetInputEnum());
        }
        return inputEnums;
    }

    // ================================================================================================================
    std::vector<CustomizedCommand> ImGuiInputHandler::HandleInput()
    {
        ImGuiIO& io = ImGui::GetIO();

        std::vector<CustomizedCommand> commands;
        std::vector<ImGuiInput>        frameInputs;

        // Common ImGUI input parameters:
        // io.MousePos, io.MouseDelta, ImGui::IsMouseDown(i), ImGui::IsKeyDown(ImGuiKey key), ...etc.
        // Cache the current frame inputs
        if(ImGui::IsKeyDown(ImGuiKey_W))
        {
            frameInputs.push_back(ImGuiInput(InputEnum::PRESS_W));
        }
        
        if (ImGui::IsKeyDown(ImGuiKey_A))
        {
            frameInputs.push_back(ImGuiInput(InputEnum::PRESS_A));
        }

        if (ImGui::IsKeyDown(ImGuiKey_S))
        {
            frameInputs.push_back(ImGuiInput(InputEnum::PRESS_S));
        }

        if (ImGui::IsKeyDown(ImGuiKey_D))
        {
            frameInputs.push_back(ImGuiInput(InputEnum::PRESS_D));
        }
        
        if(abs(io.MouseDelta.x) > 0 || abs(io.MouseDelta.y))
        {
            ImGuiInput mouseMove(InputEnum::MOUSE_MOVE);
            mouseMove.AddFloat(io.MouseDelta.x);
            mouseMove.AddFloat(io.MouseDelta.y);
            mouseMove.AddFloat(io.MousePos.x);
            mouseMove.AddFloat(io.MousePos.y);

            frameInputs.push_back(mouseMove);
        }

        // Middle mouse button
        if (ImGui::IsMouseDown(2))
        {
            frameInputs.push_back(ImGuiInput(InputEnum::PRESS_MOUSE_MIDDLE_BUTTON));
        }

        std::unordered_set<InputEnum> inputEnums = GenerateInputEnum(frameInputs);

        // Generate the commands
        for (const auto& itr : m_commandGenerators)
        {
            if (itr->CheckKeyCombination(inputEnums))
            {
                commands.push_back(itr->GenerateCommand(frameInputs));
            }
        }

        return commands;
    }

    // ================================================================================================================
    ImGuiInput CommandGenerator::FindQualifiedInput(
        InputEnum                                iEnum,
        const std::vector<SharedLib::ImGuiInput> inputs)
    {
        for (const auto& itr : inputs)
        {
            if (itr.GetInputEnum() == iEnum)
            {
                return itr;
            }
        }
        return ImGuiInput(InputEnum::INVALID);
    }

    // ================================================================================================================
    ImGuiApplication::ImGuiApplication()
        : GlfwApplication(),
          m_descriptorPool(VK_NULL_HANDLE),
          m_guiRenderPass(VK_NULL_HANDLE)
    {}

    // ================================================================================================================
    ImGuiApplication::~ImGuiApplication()
    {
        for (auto framebuffer : m_imGuiFramebuffers)
        {
            vkDestroyFramebuffer(m_device, framebuffer, nullptr);
        }

        // Destroy the render pass
        vkDestroyRenderPass(m_device, m_guiRenderPass, nullptr);

        // Destroy the descriptor pool
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    // ================================================================================================================
    void ImGuiApplication::InitImGui()
    {
        // Create descriptor pool and render pass for ImGui
        // Create the render pass -- We will use dynamic rendering for the scene rendering
        // Specify the GUI attachment: We will need to present everything in GUI. So, the finalLayout would be presentable.
        VkAttachmentDescription guiRenderTargetAttachment{};
        {
            guiRenderTargetAttachment.format = m_choisenSurfaceFormat.format;
            guiRenderTargetAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            // guiRenderTargetAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Note that we don't clear the ImGUI render target since we put DearImGUI at end of a render pass and it's transparent.
            guiRenderTargetAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            guiRenderTargetAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            guiRenderTargetAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            guiRenderTargetAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            guiRenderTargetAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            guiRenderTargetAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        }

        // Specify the color reference, which specifies the attachment layout during the subpass
        VkAttachmentReference guiAttachmentRef{};
        {
            guiAttachmentRef.attachment = 0;
            guiAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }

        // Specity the subpass executed for the GUI
        VkSubpassDescription guiSubpass{};
        {
            guiSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            guiSubpass.colorAttachmentCount = 1;
            guiSubpass.pColorAttachments = &guiAttachmentRef;
        }

        // Specify the dependency between the scene subpass (0) and the gui subpass (1).
        // The gui subpass' rendering output should wait for the scene subpass' rendering output.
        VkSubpassDependency guiSubpassesDependency{};
        {
            guiSubpassesDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            guiSubpassesDependency.dstSubpass = 0;
            guiSubpassesDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            guiSubpassesDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            guiSubpassesDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            guiSubpassesDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        }

        // Create the render pass
        VkRenderPassCreateInfo guiRenderPassInfo{};
        {
            guiRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            guiRenderPassInfo.attachmentCount = 1;
            guiRenderPassInfo.pAttachments = &guiRenderTargetAttachment;
            guiRenderPassInfo.subpassCount = 1;
            guiRenderPassInfo.pSubpasses = &guiSubpass;
            guiRenderPassInfo.dependencyCount = 1;
            guiRenderPassInfo.pDependencies = &guiSubpassesDependency;
        }
        VK_CHECK(vkCreateRenderPass(m_device, &guiRenderPassInfo, nullptr, &m_guiRenderPass));

        // Create the descriptor pool
        VkDescriptorPoolSize poolSizes[] =
        {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };
        VkDescriptorPoolCreateInfo pool_info{};
        {
            pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            pool_info.maxSets = 1000 * sizeof(poolSizes) / sizeof(VkDescriptorPoolSize);
            pool_info.poolSizeCount = (uint32_t)(sizeof(poolSizes) / sizeof(VkDescriptorPoolSize));
            pool_info.pPoolSizes = poolSizes;
        }
        VK_CHECK(vkCreateDescriptorPool(m_device, &pool_info, nullptr, &m_descriptorPool));

        // Create Framebuffer for ImGui
        m_imGuiFramebuffers.resize(m_swapchainColorImageViews.size());
        for (int i = 0; i < m_swapchainColorImageViews.size(); i++)
        {
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = m_guiRenderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = &m_swapchainColorImageViews[i];
            framebufferInfo.width = m_swapchainImageExtent.width;
            framebufferInfo.height = m_swapchainImageExtent.height;
            framebufferInfo.layers = 1;
            VK_CHECK(vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_imGuiFramebuffers[i]));
        }

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
        ImGui_ImplVulkan_Init(&initInfo, m_guiRenderPass);

        // Upload Fonts
        {
            // Use any command queue
            VK_CHECK(vkResetCommandPool(m_device, m_gfxCmdPool, 0));

            VkCommandBufferAllocateInfo fontUploadCmdBufAllocInfo{};
            {
                fontUploadCmdBufAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                fontUploadCmdBufAllocInfo.commandPool = m_gfxCmdPool;
                fontUploadCmdBufAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                fontUploadCmdBufAllocInfo.commandBufferCount = 1;
            }
            VkCommandBuffer fontUploadCmdBuf;
            VK_CHECK(vkAllocateCommandBuffers(m_device, &fontUploadCmdBufAllocInfo, &fontUploadCmdBuf));

            VkCommandBufferBeginInfo begin_info{};
            {
                begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            }
            VK_CHECK(vkBeginCommandBuffer(fontUploadCmdBuf, &begin_info));

            ImGui_ImplVulkan_CreateFontsTexture(fontUploadCmdBuf);

            VkSubmitInfo end_info = {};
            {
                end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                end_info.commandBufferCount = 1;
                end_info.pCommandBuffers = &fontUploadCmdBuf;
            }

            VK_CHECK(vkEndCommandBuffer(fontUploadCmdBuf));

            VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &end_info, VK_NULL_HANDLE));

            VK_CHECK(vkDeviceWaitIdle(m_device));
            ImGui_ImplVulkan_DestroyFontUploadObjects();
        }
    }

    // ================================================================================================================
    void ImGuiApplication::FrameStart()
    {
        GlfwApplication::FrameStart(); // Contains the GlfwPollEvents() call.
    }

    // ================================================================================================================
    void ImGuiApplication::FrameEnd()
    {
        GlfwApplication::FrameEnd();
    }
}