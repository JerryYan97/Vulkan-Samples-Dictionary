#include "vk_mem_alloc.h"
#include "args.hxx"

#include "GenIBL.h"
#include "../../SharedLibrary/Utils/CmdBufUtils.h"
#include "../../SharedLibrary/Utils/VulkanDbgUtils.h"
#include "../../SharedLibrary/Utils/StrPathUtils.h"
#include "../../SharedLibrary/Utils/AppUtils.h"

#include "renderdoc_app.h"
#include <Windows.h>
#include <cassert>

int main(
    int argc,
    char** argv)
{
    args::ArgumentParser parser("This tool takes a cubemap as input and output its image based lighting data.",
        "E.g. GenIBL.exe --srcPath ./img.hdr --dstPath ./iblOutput");
    args::HelpFlag help(parser, "help", "Display this help menu", { 'h', "help" });
    args::CompletionFlag completion(parser, { "complete" });

    args::ValueFlag<std::string> inputPath(parser, "", "The input cubemap image.", { 'i', "srcPath" });
    args::ValueFlag<std::string> outputPath(parser, "", "The output image based lighting data output folder.", { 'i', "dstPath" });

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

    // Create or clean folder, path manipulation -- Make sure that they are absolute paths.
    std::string inputPathName;
    std::string outputDir;
    {
        if (inputPath)
        {
            if (SharedLib::IsFile(inputPath.Get()) == false)
            {
                std::cerr << "The input is not a file!" << std::endl;
                return 1;
            }

            bool isValid = SharedLib::GetAbsolutePathName(inputPath.Get(), inputPathName);
            std::cout << "Read File From: " << inputPathName << std::endl;
            if (isValid == false)
            {
                std::cerr << "Invalid input Path!" << std::endl;
                return 1;
            }
        }
        else
        {
            std::cerr << "Cannot find the input Path!" << std::endl;
            return 1;
        }

        if (outputPath)
        {
            bool isValid = SharedLib::GetAbsolutePathName(outputPath.Get(), outputDir);
            if (isValid == false)
            {
                std::cerr << "Invalid output Path!" << std::endl;
                return 1;
            }
        }
        else
        {
            std::cerr << "Cannot find the output directory!" << std::endl;
            return 1;
        }
    }

    // Start application
    {
        // RenderDoc debug starts
        RENDERDOC_API_1_6_0* rdoc_api = NULL;
        {
            if (HMODULE mod = GetModuleHandleA("renderdoc.dll"))
            {
                pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
                int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0, (void**)&rdoc_api);
                assert(ret == 1);
            }

            if (rdoc_api)
            {
                std::cout << "Frame capture starts." << std::endl;
                rdoc_api->StartFrameCapture(NULL, NULL);
            }
        }

        GenIBL app;
        app.ReadInCubemap(inputPathName);
        app.AppInit();

        SharedLib::CubemapFormatTransApp cubemapFormatTransApp{};

        // Common data used in the CmdBuffer filling process.
        VkCommandBuffer cmdBuffer = app.GetGfxCmdBuffer(0);
        VkQueue         gfxQueue = app.GetGfxQueue();
        VkDevice        device = app.GetVkDevice();
        VmaAllocator    allocator = *app.GetVmaAllocator();
        VkDescriptorSet pipelineDescriptorSet = app.GetDiffIrrPreFilterEnvMapDesSet();

        VkImageSubresourceRange cubemapSubResRange{};
        {
            cubemapSubResRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            cubemapSubResRange.baseMipLevel = 0;
            cubemapSubResRange.levelCount = 1;
            cubemapSubResRange.baseArrayLayer = 0;
            cubemapSubResRange.layerCount = 6;
        }

        VkClearValue clearColor = { {{1.0f, 0.0f, 0.0f, 1.0f}} };

        ImgInfo inputHdriInfo = app.GetInputHdriInfo();

        // Init resources used by the CubemapFormatTransApp.
        VkExtent3D cubemapExtent3D{};
        {
            cubemapExtent3D.width = inputHdriInfo.width;
            cubemapExtent3D.height = inputHdriInfo.height;
            cubemapExtent3D.depth = 1;
        }
        cubemapFormatTransApp.SetInputCubemapImg(app.GetDiffuseIrradianceCubemap(), cubemapExtent3D);
        SharedLib::VulkanInfos formatTransVkInfo{};
        {
            formatTransVkInfo.device = device;
            formatTransVkInfo.pAllocator = app.GetVmaAllocator();
            formatTransVkInfo.descriptorPool = app.GetDescriptorPool();
        }
        cubemapFormatTransApp.GetVkInfos(formatTransVkInfo);
        cubemapFormatTransApp.Init();

        // Send input hdri buffer data to its gpu cubemap image.
        {
            VkBufferImageCopy hdrBufToImgCopy{};
            {
                VkExtent3D extent{};
                {
                    extent.width = inputHdriInfo.width;
                    extent.height = inputHdriInfo.width;
                    extent.depth = 1;
                }

                hdrBufToImgCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                hdrBufToImgCopy.imageSubresource.mipLevel = 0;
                hdrBufToImgCopy.imageSubresource.baseArrayLayer = 0;
                hdrBufToImgCopy.imageSubresource.layerCount = 6;

                hdrBufToImgCopy.imageExtent = extent;
            }

            SharedLib::CmdSendImgDataToGpu(cmdBuffer,
                device,
                gfxQueue,
                inputHdriInfo.pData,
                3 * sizeof(float) * inputHdriInfo.width * inputHdriInfo.height,
                app.GetInputCubemap(),
                cubemapSubResRange,
                hdrBufToImgCopy,
                allocator);
        }

        // Render the diffuse irradiance map
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
                cubemapRenderTargetTransBarrier.image = app.GetDiffuseIrradianceCubemap();
                cubemapRenderTargetTransBarrier.subresourceRange = cubemapSubResRange;
            }

            vkCmdPipelineBarrier(cmdBuffer,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &cubemapRenderTargetTransBarrier);

            VkRenderingAttachmentInfoKHR renderAttachmentInfo{};
            {
                renderAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
                renderAttachmentInfo.imageView = app.GetDiffuseIrradianceCubemapView();
                renderAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                renderAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                renderAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                renderAttachmentInfo.clearValue = clearColor;
            }

            VkExtent2D colorRenderTargetExtent{};
            {
                colorRenderTargetExtent.width = inputHdriInfo.width;
                colorRenderTargetExtent.height = inputHdriInfo.width;
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
                app.GetDiffuseIrradiancePipelineLayout(),
                0, 1, &pipelineDescriptorSet,
                0, NULL);

            vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app.GetDiffuseIrradiancePipeline());

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

            // Transfer diffuse irradiance map's layout to copy source
            VkImageMemoryBarrier cubemapColorAttToSrcBarrier{};
            {
                cubemapColorAttToSrcBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                cubemapColorAttToSrcBarrier.image = app.GetDiffuseIrradianceCubemap();
                cubemapColorAttToSrcBarrier.subresourceRange = cubemapSubResRange;
                cubemapColorAttToSrcBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                cubemapColorAttToSrcBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                cubemapColorAttToSrcBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                cubemapColorAttToSrcBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            }

            vkCmdPipelineBarrier(
                cmdBuffer,
                VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &cubemapColorAttToSrcBarrier);

            // Submit all the works recorded before
            VK_CHECK(vkEndCommandBuffer(cmdBuffer));

            SharedLib::SubmitCmdBufferAndWait(device, gfxQueue, cmdBuffer);

            vkResetCommandBuffer(cmdBuffer, 0);
        }

        // Reformat the diffuse irradiance map since it's a cubemap
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

        // Save the vulkan format diffuse irradiance cubemap to the disk
        {
            // Copy the rendered images to a buffer.
            VkBuffer stagingBuffer;
            VmaAllocation stagingBufferAlloc;

            VmaAllocationCreateInfo stagingBufAllocInfo{};
            {
                stagingBufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
                stagingBufAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            }

            VkBufferCreateInfo stgBufInfo{};
            {
                stgBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                stgBufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
                stgBufInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                stgBufInfo.size = 4 * sizeof(float) * inputHdriInfo.width * inputHdriInfo.height * 6; // RGBA and 6 images blocks.
            }

            VK_CHECK(vmaCreateBuffer(allocator, &stgBufInfo, &stagingBufAllocInfo, &stagingBuffer, &stagingBufferAlloc, nullptr));

            SharedLib::CmdCopyCubemapToBuffer(cmdBuffer, device, gfxQueue, cubemapFormatTransApp.GetOutputCubemap(), inputHdriInfo.width, stagingBuffer);

            // Copy the buffer data to RAM and save that on the disk.
            float* pImgData = new float[4 * inputHdriInfo.width * inputHdriInfo.height * 6];

            void* pBufferMapped;
            vmaMapMemory(allocator, stagingBufferAlloc, &pBufferMapped);
            memcpy(pImgData, pBufferMapped, 4 * sizeof(float) * inputHdriInfo.width * inputHdriInfo.height * 6);
            vmaUnmapMemory(allocator, stagingBufferAlloc);

            // Convert data from 4 elements to 3 elements data
            float* pImgData3Ele = new float[3 * inputHdriInfo.width * inputHdriInfo.height * 6];
            for (uint32_t i = 0; i < inputHdriInfo.width * inputHdriInfo.height * 6; i++)
            {
                uint32_t ele4Idx0 = i * 4;
                uint32_t ele4Idx1 = i * 4 + 1;
                uint32_t ele4Idx2 = i * 4 + 2;

                uint32_t ele3Idx0 = i * 3;
                uint32_t ele3Idx1 = i * 3 + 1;
                uint32_t ele3Idx2 = i * 3 + 2;

                pImgData3Ele[ele3Idx0] = pImgData[ele4Idx0];
                pImgData3Ele[ele3Idx1] = pImgData[ele4Idx1];
                pImgData3Ele[ele3Idx2] = pImgData[ele4Idx2];
            }

            std::string outputCubemapPathName = outputDir + "/diffuse_irradiance_cubemap.hdr";

            app.SaveCubemap(outputCubemapPathName,
                app.GetOutputCubemapExtent().width,
                app.GetOutputCubemapExtent().height * 6,
                3,
                pImgData3Ele);

            // Cleanup resources
            vkResetCommandBuffer(cmdBuffer, 0);
            vmaDestroyBuffer(allocator, stagingBuffer, stagingBufferAlloc);
            delete pImgData;
            delete pImgData3Ele;
        }

        // End RenderDoc debug
        if (rdoc_api)
        {
            std::cout << "Frame capture ends." << std::endl;
            rdoc_api->EndFrameCapture(NULL, NULL);
        }

        cubemapFormatTransApp.Destroy();
    }

    system("pause");
}
