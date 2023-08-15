#include "SphericalToCubemap.h"
#include "args.hxx"
#include "../../SharedLibrary/Utils/CmdBufUtils.h"
#include "../../SharedLibrary/Utils/VulkanDbgUtils.h"

bool CheckImgValAbove1(
    float* pData,
    uint32_t width,
    uint32_t height)
{
    for (uint32_t i = 0; i < width * height * 3; i++)
    {
        if (pData[i] > 1.f)
        {
            return true;
        }
    }
    return false;
}

int main(
    int argc, 
    char** argv)
{
    args::ArgumentParser parser("This tool takes an equirectangular image as input and output the cubemap. If the input is ./img.hdr. The output will be ./img_cubemap.hdr", 
                                "E.g. SphericalToCubemap.exe --srcPath ./img.hdr");
    args::HelpFlag help(parser, "help", "Display this help menu", { 'h', "help" });
    args::CompletionFlag completion(parser, { "complete" });

    args::ValueFlag<std::string> inputPath(parser, "", "The input equirectangular image path.", { 'i', "srcPath"});

    try
    {
        parser.ParseCLI(argc, argv);
    }
    catch (const args::Completion& e)
    {
        std::cout << e.what();
        return 0;
    }
    catch (const args::Help&)
    {
        std::cout << parser;
        return 0;
    }
    catch (const args::ParseError& e)
    {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
        return 1;
    }

    SphericalToCubemap app;
    app.AppInit();

    app.ReadInHdri("C:\\JiaruiYan\\Projects\\OneFileVulkans\\Tools\\SphericalToCubemap\\data\\little_paris_eiffel_tower_4k.hdr");
    app.CreateHdriGpuObjects();

    if (CheckImgValAbove1(app.GetInputHdriData(), app.GetInputHdriWidth(), app.GetInputHdriHeight()))
    {
        std::cout << "The image has elements that are larger than 1.f." << std::endl;
    }
    else
    {
        std::cout << "The image doesn't have elements that are larger than 1.f." << std::endl;
    }

    // Just get a command buffer from 
    VkCommandBuffer cmdBuffer = app.GetGfxCmdBuffer(0);
    VkQueue gfxQueue = app.GetGfxQueue();
    VkDevice device = app.GetVkDevice();
    VmaAllocator allocator = *app.GetVmaAllocator();

    // Send hdri data to its gpu objects through a staging buffer.
    {
        VkImageSubresourceRange copySubRange{};
        {
            copySubRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copySubRange.baseMipLevel = 0;
            copySubRange.levelCount = 1;
            copySubRange.baseArrayLayer = 0;
            copySubRange.layerCount = 1;
        }

        VkBufferImageCopy hdrBufToImgCopy{};
        {
            VkExtent3D extent{};
            {
                extent.width = app.GetInputHdriWidth();
                extent.height = app.GetInputHdriHeight();
                extent.depth = 1;
            }

            hdrBufToImgCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            hdrBufToImgCopy.imageSubresource.mipLevel = 0;
            hdrBufToImgCopy.imageSubresource.baseArrayLayer = 0;
            hdrBufToImgCopy.imageSubresource.layerCount = 1;

            hdrBufToImgCopy.imageExtent = extent;
        }

        SharedLib::CmdSendImgDataToGpu(cmdBuffer, 
                                       device,
                                       gfxQueue,
                                       app.GetInputHdriData(),
                                       3 * sizeof(float) * app.GetInputHdriWidth() * app.GetInputHdriHeight(),
                                       app.GetHdriImg(),
                                       copySubRange,
                                       hdrBufToImgCopy,
                                       allocator);
    }

    // Draw the 6 cubemap faces
    {
        // Fill the command buffer
        VkCommandBufferBeginInfo beginInfo{};
        {
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        }
        VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo));

        VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };

        VkRenderingAttachmentInfoKHR renderAttachmentInfo{};
        {
            renderAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
            renderAttachmentInfo.imageView = app.GetOutputCubemapImgView();
            renderAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
            renderAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            renderAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            renderAttachmentInfo.clearValue = clearColor;
        }

        VkExtent2D colorRenderTargetExtent{};
        {
            colorRenderTargetExtent.width = app.GetOutputCubemapExtent().width;
            colorRenderTargetExtent.height = app.GetOutputCubemapExtent().height;
        }

        VkRenderingInfoKHR renderInfo{};
        {
            renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
            renderInfo.renderArea.offset = { 0, 0 };
            renderInfo.renderArea.extent = colorRenderTargetExtent;
            renderInfo.layerCount = 6;
            renderInfo.colorAttachmentCount = 1;
            renderInfo.pColorAttachments = &renderAttachmentInfo;
        }

        vkCmdBeginRendering(cmdBuffer, &renderInfo);

        // Bind the graphics pipeline
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app.GetPipeline());

        // Set the viewport
        VkViewport viewport{};
        {
            viewport.x = 0.f;
            viewport.y = 0.f;
            viewport.width = (float)colorRenderTargetExtent.width;
            viewport.height = (float)colorRenderTargetExtent.height;
            viewport.minDepth = 0.f;
            viewport.maxDepth = 1.f;
        }
        vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

        // Set the scissor
        VkRect2D scissor{};
        {
            scissor.offset = { 0, 0 };
            scissor.extent = colorRenderTargetExtent;
            vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
        }

        vkCmdDraw(cmdBuffer, 6, 1, 0, 0);

        vkCmdEndRendering(cmdBuffer);

        VK_CHECK(vkEndCommandBuffer(cmdBuffer));
        
        VkFence submitFence;
        VkFenceCreateInfo fenceInfo{};
        {
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        }
        VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &submitFence));
        VK_CHECK(vkResetFences(device, 1, &submitFence));

        VkSubmitInfo submitInfo{};
        {
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &cmdBuffer;
        }
        VK_CHECK(vkQueueSubmit(gfxQueue, 1, &submitInfo, submitFence));
        vkWaitForFences(device, 1, &submitFence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(device, submitFence, nullptr);
    }
    
    // Copy cubemap images out
    {

    }
}