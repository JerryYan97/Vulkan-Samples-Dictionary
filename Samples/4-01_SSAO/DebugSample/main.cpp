#include "vk_mem_alloc.h"

#include "SSAOApp.h"
#include "../../../SharedLibrary/Utils/VulkanDbgUtils.h"

#include <vulkan/vulkan.h>

int main()
{
    SSAOApp app;
    app.AppInit();

    app.GpuWaitForIdle();

    // Main Loop
    while (!app.WindowShouldClose())
    {
        VkDevice device = app.GetVkDevice();
        VkExtent2D swapchainImageExtent = app.GetSwapchainImageExtent();

        app.FrameStart();

        // Get next available image from the swapchain. Wait and reset current frame fence in case of the frame is not finished.
        if (app.WaitNextImgIdxOrNewSwapchain() == false)
        {
            continue;
        }

        // We use the acquired swapchain image index as the frame index, so GetCurrentXXX() functions need to be called after WaitNextImgIdxOrNewSwapchain().
        VkFence inFlightFence = app.GetCurrentFrameFence();
        VkCommandBuffer currentCmdBuffer = app.GetCurrentFrameGfxCmdBuffer();

        app.UpdateCameraAndGpuBuffer();

        // Fill the command buffer
        VkCommandBufferBeginInfo beginInfo{};
        {
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        }
        VK_CHECK(vkBeginCommandBuffer(currentCmdBuffer, &beginInfo));
        
        // Transfer GBuffer layout from UNDEFINED/Shader Resource to Color Attachment.
        app.CmdSSAOFrameStartLayoutTrans(currentCmdBuffer);
        
        /*
        app.CmdGBufferLayoutTrans(currentCmdBuffer,
                                  VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                  VK_ACCESS_NONE,
                                  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        */

        app.CmdGeoPass(currentCmdBuffer);
        
        app.CmdGBufferLayoutTrans(currentCmdBuffer,
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                  VK_ACCESS_SHADER_READ_BIT,
                                  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        app.CmdSSAOAppMultiTypeRendering(currentCmdBuffer);

        // Theoritically, we should call ImGuiFrame() before the scene rendering passes to improve performance (Not opaque GUI)
        // but it requires depth test so we call it after the scene rendering passes.
        // In addition, the ImGUI Renderpass transfers the swapchain color target layout from COLOR_ATTACHMENT_OPTIMAL
        // to PRESENT_SRC_KHR, so we don't need to manually transfer the layout.
        app.ImGuiFrame(currentCmdBuffer);

        // app.CmdSwapchainColorImgToPresent(currentCmdBuffer);
        
        VK_CHECK(vkEndCommandBuffer(currentCmdBuffer));

        app.GfxCmdBufferFrameSubmitAndPresent();

        app.FrameEnd();
    }
}
