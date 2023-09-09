#include "SphericalToCubemap.h"
#include "args.hxx"
#include "../../SharedLibrary/Utils/CmdBufUtils.h"
#include "../../SharedLibrary/Utils/VulkanDbgUtils.h"
#include "../../SharedLibrary/Utils/AppUtils.h"

#include "renderdoc_app.h"
#include <Windows.h>
#include <cassert>

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

// TODO: Some CmdBuffer recording can be packaged.
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

    std::string inputHdrPathName = {};
    bool isDefault = true;
    std::string inputHdrFolderPath = {};
    if (inputPath)
    {
        inputHdrPathName = inputPath.Get();
        std::cout << "Read File From: " << inputHdrPathName << std::endl;
        isDefault = false;

        size_t found = inputHdrPathName.find_last_of("/\\");
        if (found != std::string::npos)
        {
            inputHdrFolderPath = inputHdrPathName.substr(0, found);
        }
        else
        {
            std::cout << "Cannot find the input file." << std::endl;
            exit(1);
        }
    }
    else
    {
        inputHdrPathName = SOURCE_PATH;
        inputHdrPathName += "/data/little_paris_eiffel_tower_4k.hdr";
        std::cout << "Read default file from: " << inputHdrPathName << std::endl;
    }

    // RenderDoc debug starts
    RENDERDOC_API_1_6_0* rdoc_api = NULL;
    if (HMODULE mod = GetModuleHandleA("renderdoc.dll"))
    {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
        int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0, (void**)&rdoc_api);
        assert(ret == 1);
    }

    if (rdoc_api)
    {
        rdoc_api->StartFrameCapture(NULL, NULL);
    }

    SphericalToCubemap app;
    app.ReadInHdri(inputHdrPathName);
    app.AppInit();

    SharedLib::CubemapFormatTransApp cubemapFormatTransApp;
    cubemapFormatTransApp.SetInputCubemapImg(app.GetOutputCubemapImg(), app.GetOutputCubemapExtent());

    if (CheckImgValAbove1(app.GetInputHdriData(), app.GetInputHdriWidth(), app.GetInputHdriHeight()))
    {
        std::cout << "The image has elements that are larger than 1.f." << std::endl;
    }
    else
    {
        std::cout << "The image doesn't have elements that are larger than 1.f." << std::endl;
    }

    // Common data used in the CmdBuffer filling process.
    VkCommandBuffer cmdBuffer = app.GetGfxCmdBuffer(0);
    VkQueue gfxQueue = app.GetGfxQueue();
    VkDevice device = app.GetVkDevice();
    VmaAllocator allocator = *app.GetVmaAllocator();
    VkDescriptorSet pipelineDescriptorSet = app.GetDescriptorSet();

    SharedLib::VulkanInfos formatTransVkInfo{};
    {
        formatTransVkInfo.device = device;
        formatTransVkInfo.pAllocator = app.GetVmaAllocator();
        formatTransVkInfo.descriptorPool = app.GetDescriptorPool();
        formatTransVkInfo.gfxCmdPool = app.GetGfxCmdPool();
        formatTransVkInfo.gfxQueue = gfxQueue;
    }
    cubemapFormatTransApp.GetVkInfos(formatTransVkInfo);
    cubemapFormatTransApp.Init();

    VkImageSubresourceRange cubemapSubResRange{};
    {
        cubemapSubResRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        cubemapSubResRange.baseMipLevel = 0;
        cubemapSubResRange.levelCount = 1;
        cubemapSubResRange.baseArrayLayer = 0;
        cubemapSubResRange.layerCount = 6;
    }

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

        SharedLib::SendImgDataToGpu(cmdBuffer, 
                                       device,
                                       gfxQueue,
                                       app.GetInputHdriData(),
                                       3 * sizeof(float) * app.GetInputHdriWidth() * app.GetInputHdriHeight(),
                                       app.GetHdriImg(),
                                       copySubRange,
                                       hdrBufToImgCopy,
                                       allocator);
    }

    // Draw the Front, Back, Top, Bottom, Right, Left faces to the cubemap.
    {
        // Fill the command buffer
        VkCommandBufferBeginInfo beginInfo{};
        {
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        }
        VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo));

        // Transform the layout of the output cubemap from undefined to render target.
        VkImageMemoryBarrier cubemapRenderTargetTransBarrier{};
        {
            cubemapRenderTargetTransBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            cubemapRenderTargetTransBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            cubemapRenderTargetTransBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            cubemapRenderTargetTransBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            cubemapRenderTargetTransBarrier.image = app.GetOutputCubemapImg();
            cubemapRenderTargetTransBarrier.subresourceRange = cubemapSubResRange;
        }

        vkCmdPipelineBarrier(cmdBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &cubemapRenderTargetTransBarrier);

        VkClearValue clearColor = { {{1.0f, 0.0f, 0.0f, 1.0f}} };

        VkRenderingAttachmentInfoKHR renderAttachmentInfo{};
        {
            renderAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
            renderAttachmentInfo.imageView = app.GetOutputCubemapImgView();
            renderAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
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
            renderInfo.viewMask = 0x3F;
            renderInfo.pColorAttachments = &renderAttachmentInfo;
        }

        vkCmdBeginRendering(cmdBuffer, &renderInfo);

        // Bind the graphics pipeline
        vkCmdBindDescriptorSets(cmdBuffer, 
                                VK_PIPELINE_BIND_POINT_GRAPHICS, 
                                app.GetPipelineLayout(),
                                0, 1, &pipelineDescriptorSet,
                                0, NULL);

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
        
        // Submit all the works recorded before
        VK_CHECK(vkEndCommandBuffer(cmdBuffer));

        SharedLib::SubmitCmdBufferAndWait(device, gfxQueue, cmdBuffer);

        vkResetCommandBuffer(cmdBuffer, 0);
    }

    // Convert output 6 faces images to the Vulkan's cubemap's format
    // From Front, Back, Top, ... To X+, X-, Y+,...
    {
        // Fill the command buffer
        VkCommandBufferBeginInfo beginInfo{};
        {
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        }
        VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo));

        cubemapFormatTransApp.CmdConvertCubemapFormat(cmdBuffer);

        // Submit all the works recorded before
        VK_CHECK(vkEndCommandBuffer(cmdBuffer));

        SharedLib::SubmitCmdBufferAndWait(device, gfxQueue, cmdBuffer);

        vkResetCommandBuffer(cmdBuffer, 0);
    }

    // Save the vulkan format cubemap to the disk
    {
        std::string outputCubemapPathName = {};
        if (isDefault)
        {
            outputCubemapPathName = SOURCE_PATH;
            outputCubemapPathName += "/data/output_cubemap.hdr";
        }
        else
        {
            outputCubemapPathName = inputHdrFolderPath + "/output_cubemap.hdr";
        }

        cubemapFormatTransApp.DumpOutputCubemapToDisk(outputCubemapPathName);
    }

    if (rdoc_api)
    {
        rdoc_api->EndFrameCapture(NULL, NULL);
    }

    cubemapFormatTransApp.Destroy();
    system("pause");
}