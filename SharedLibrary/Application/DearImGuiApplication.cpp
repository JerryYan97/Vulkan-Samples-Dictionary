#include "DearImGuiApplication.h"
#include "../../../ThirdPartyLibs/DearImGUI/imgui.h"
#include "../../../ThirdPartyLibs/DearImGUI/backends/imgui_impl_glfw.h"
#include "../../../ThirdPartyLibs/DearImGUI/backends/imgui_impl_vulkan.h"

#include "VulkanDbgUtils.h"

namespace SharedLib
{
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
            guiRenderTargetAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            guiRenderTargetAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            guiRenderTargetAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            guiRenderTargetAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            guiRenderTargetAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
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