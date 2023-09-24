#include "vk_mem_alloc.h"
#include "args.hxx"

#include "GenIBL.h"
#include "../../SharedLibrary/Utils/CmdBufUtils.h"
#include "../../SharedLibrary/Utils/VulkanDbgUtils.h"
#include "../../SharedLibrary/Utils/StrPathUtils.h"
#include "../../SharedLibrary/Utils/AppUtils.h"
#include "../../SharedLibrary/Utils/DiskOpsUtils.h"

#include "renderdoc_app.h"
#include <Windows.h>
#include <cassert>
#include <filesystem>

// Adjustable Parameters:
// * The input HDRI color clamp.
// * The input HDRI cubemap mipmap level sampling.
// * The number of roughness levels for the prefilter environment map.
int main(
    int argc,
    char** argv)
{
    args::ArgumentParser parser("This tool takes a cubemap as input and output its image based lighting data.",
        "E.g. GenIBL.exe --srcPath ./img.hdr --dstPath ./iblOutput");
    args::HelpFlag help(parser, "help", "Display this help menu", { 'h', "help" });
    args::CompletionFlag completion(parser, { "complete" });

    args::ValueFlag<std::string> inputPath(parser, "", "The input cubemap image.", { 'i', "srcPath" });
    args::ValueFlag<std::string> outputPath(parser, "", "The output image based lighting data output folder.", { 'o', "dstPath" });

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

        VkImageSubresourceRange cubemapSubResRangeLayer6Level1{};
        {
            cubemapSubResRangeLayer6Level1.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            cubemapSubResRangeLayer6Level1.baseMipLevel = 0;
            cubemapSubResRangeLayer6Level1.levelCount = 1;
            cubemapSubResRangeLayer6Level1.baseArrayLayer = 0;
            cubemapSubResRangeLayer6Level1.layerCount = 6;
        }

        VkImageSubresourceRange cubemapSubResRangeLayer6Level10{};
        {
            cubemapSubResRangeLayer6Level10.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            cubemapSubResRangeLayer6Level10.baseMipLevel = 0;
            cubemapSubResRangeLayer6Level10.levelCount = 10;
            cubemapSubResRangeLayer6Level10.baseArrayLayer = 0;
            cubemapSubResRangeLayer6Level10.layerCount = 6;
        }

        VkClearValue clearColor = { {{1.0f, 0.0f, 0.0f, 1.0f}} };

        ImgInfo inputHdriInfo = app.GetInputHdriInfo();

        // Init resources used by the CubemapFormatTransApp.
        VkExtent3D cubemapExtent3D{};
        {
            cubemapExtent3D.width = inputHdriInfo.width;
            cubemapExtent3D.height = inputHdriInfo.width;
            cubemapExtent3D.depth = 1;
        }

        cubemapFormatTransApp.SetInputCubemapImg(app.GetDiffuseIrradianceCubemap(),
                                                 cubemapExtent3D,
                                                 cubemapSubResRangeLayer6Level1);

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

            SharedLib::SendImgDataToGpu(cmdBuffer,
                device,
                gfxQueue,
                inputHdriInfo.pData,
                3 * sizeof(float) * inputHdriInfo.width * inputHdriInfo.height,
                app.GetInputCubemap(),
                cubemapSubResRangeLayer6Level1,
                VK_IMAGE_LAYOUT_UNDEFINED,
                hdrBufToImgCopy,
                allocator);
        }

        // Blur the input cubemap of the diffuse irradiance map rendering -- Equivalent to generating mipmaps.
        {
            app.CmdGenInputCubemapMipMaps(cmdBuffer);
        }

        // Render the diffuse irradiance map
        {
            // Fill the command buffer
            VkCommandBufferBeginInfo beginInfo{};
            {
                beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            }
            VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo));

            // Transfer the layout of the input cubemap mipmaps from transfer dst to shader read.
            VkImageMemoryBarrier hdrDstToShaderBarrier{};
            {
                hdrDstToShaderBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                hdrDstToShaderBarrier.image = app.GetInputCubemap();
                hdrDstToShaderBarrier.subresourceRange = cubemapSubResRangeLayer6Level10;
                hdrDstToShaderBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                hdrDstToShaderBarrier.dstAccessMask = VK_ACCESS_NONE;
                hdrDstToShaderBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                hdrDstToShaderBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }

            vkCmdPipelineBarrier(
                cmdBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &hdrDstToShaderBarrier);

            // Transform the layout of the output cubemap from undefined to render target.
            VkImageMemoryBarrier cubemapRenderTargetTransBarrier{};
            {
                cubemapRenderTargetTransBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                cubemapRenderTargetTransBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                cubemapRenderTargetTransBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                cubemapRenderTargetTransBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                cubemapRenderTargetTransBarrier.image = app.GetDiffuseIrradianceCubemap();
                cubemapRenderTargetTransBarrier.subresourceRange = cubemapSubResRangeLayer6Level1;
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
            std::string outputCubemapPathName = outputDir + "/diffuse_irradiance_cubemap.hdr";
            cubemapFormatTransApp.DumpOutputCubemapToDisk(outputCubemapPathName);
        }

        // Rendering the prefilter environment map
        // TODO: Use the push constant instead of waiting for draw completes and ubo updates.
        {
            app.GenPrefilterEnvMap();
        }

        // Reformat the prefilter environment map and dump out
        {
            cubemapFormatTransApp.Destroy();
            std::string prefilterOutputDir = outputDir + "/prefilterEnvMaps";
            SharedLib::CleanOrCreateDir(prefilterOutputDir);

            for (uint32_t i = 0; i < RoughnessLevels; i++)
            {
                uint32_t divFactor = 1 << i;
                uint32_t currentMipDim = inputHdriInfo.width / divFactor;
                VkExtent3D currentMipExtent{};
                {
                    currentMipExtent.width = currentMipDim;
                    currentMipExtent.height = currentMipDim;
                    currentMipExtent.depth = 1;
                }

                VkImageSubresourceRange currentMipSubres{};
                {
                    currentMipSubres.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    currentMipSubres.baseMipLevel = i;
                    currentMipSubres.levelCount = 1;
                    currentMipSubres.baseArrayLayer = 0;
                    currentMipSubres.layerCount = 6;
                }

                cubemapFormatTransApp.SetInputCubemapImg(app.GetPrefilterEnvMap(), 
                                                         currentMipExtent,
                                                         currentMipSubres);
                cubemapFormatTransApp.Init();

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

                std::string currentMipName = "prefilterMip" + std::to_string(i) + ".hdr";
                std::string prefilterEnvMapPathName = prefilterOutputDir + "/" + currentMipName;

                cubemapFormatTransApp.DumpOutputCubemapToDisk(prefilterEnvMapPathName);

                cubemapFormatTransApp.Destroy();
            }
        }

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

        // Render the envBrdf map
        {
            // Fill the command buffer
            VkCommandBufferBeginInfo beginInfo{};
            {
                beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            }
            VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo));

            // Transform the layout of the output image to the color attachment
            VkImageSubresourceRange envBrdfMapSubresource{};
            {
                envBrdfMapSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                envBrdfMapSubresource.baseMipLevel = 0;
                envBrdfMapSubresource.levelCount = 1;
                envBrdfMapSubresource.baseArrayLayer = 0;
                envBrdfMapSubresource.layerCount = 1;
            }

            VkImageMemoryBarrier envBrdfRenderTargetTransBarrier{};
            {
                envBrdfRenderTargetTransBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                envBrdfRenderTargetTransBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                envBrdfRenderTargetTransBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                envBrdfRenderTargetTransBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                envBrdfRenderTargetTransBarrier.image = app.GetEnvBrdfOutputImg();
                envBrdfRenderTargetTransBarrier.subresourceRange = envBrdfMapSubresource;
            }

            vkCmdPipelineBarrier(cmdBuffer,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &envBrdfRenderTargetTransBarrier);

            VkRenderingAttachmentInfoKHR renderAttachmentInfo{};
            {
                renderAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
                renderAttachmentInfo.imageView = app.GetEnvBrdfOutputImgView();
                renderAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                renderAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                renderAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                renderAttachmentInfo.clearValue = clearColor;
            }

            VkRenderingInfoKHR renderInfo{};
            {
                renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
                renderInfo.renderArea.offset = { 0, 0 };
                renderInfo.renderArea.extent = { EnvBrdfMapDim, EnvBrdfMapDim };
                renderInfo.layerCount = 1;
                renderInfo.colorAttachmentCount = 1;
                renderInfo.pColorAttachments = &renderAttachmentInfo;
            }

            vkCmdBeginRendering(cmdBuffer, &renderInfo);

            float data[2] = { EnvBrdfMapDim, EnvBrdfMapDim };
            vkCmdPushConstants(cmdBuffer,
                               app.GetEnvBrdfPipelineLayout(),
                               VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(float) * 2, data);

            vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app.GetEnvBrdfPipeline());

            // Set the viewport
            VkViewport viewport{};
            {
                viewport.x = 0.f;
                viewport.y = 0.f;
                viewport.width = (float)EnvBrdfMapDim;
                viewport.height = (float)EnvBrdfMapDim;
                viewport.minDepth = 0.f;
                viewport.maxDepth = 1.f;
            }
            vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

            // Set the scissor
            VkRect2D scissor{};
            {
                scissor.offset = { 0, 0 };
                scissor.extent = { EnvBrdfMapDim, EnvBrdfMapDim };
            }
            vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

            vkCmdDraw(cmdBuffer, 6, 1, 0, 0);

            vkCmdEndRendering(cmdBuffer);

            // Submit all the works recorded before
            VK_CHECK(vkEndCommandBuffer(cmdBuffer));

            SharedLib::SubmitCmdBufferAndWait(device, gfxQueue, cmdBuffer);

            vkResetCommandBuffer(cmdBuffer, 0);
        }

        // Dump the envBrdf map
        {
            // Transfer the envBrdf map from color attachment layout to the copy dst and copy it to a buffer to dump.
            // Fill the command buffer
            VkCommandBufferBeginInfo beginInfo{};
            {
                beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            }
            VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo));

            // Transform the layout of the output image to the color attachment
            VkImageSubresourceRange envBrdfMapSubresource{};
            {
                envBrdfMapSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                envBrdfMapSubresource.baseMipLevel = 0;
                envBrdfMapSubresource.levelCount = 1;
                envBrdfMapSubresource.baseArrayLayer = 0;
                envBrdfMapSubresource.layerCount = 1;
            }

            VkImageSubresourceLayers envBrdfMapSubresLayers{};
            {
                envBrdfMapSubresLayers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                envBrdfMapSubresLayers.baseArrayLayer = 0;
                envBrdfMapSubresLayers.layerCount = 1;
                envBrdfMapSubresLayers.mipLevel = 0;
            }

            VkImageMemoryBarrier envBrdfRenderTargetTransBarrier{};
            {
                envBrdfRenderTargetTransBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                envBrdfRenderTargetTransBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                envBrdfRenderTargetTransBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                envBrdfRenderTargetTransBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                envBrdfRenderTargetTransBarrier.image = app.GetEnvBrdfOutputImg();
                envBrdfRenderTargetTransBarrier.subresourceRange = envBrdfMapSubresource;
            }

            vkCmdPipelineBarrier(cmdBuffer,
                VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &envBrdfRenderTargetTransBarrier);

            // Submit all the works recorded before
            VK_CHECK(vkEndCommandBuffer(cmdBuffer));

            SharedLib::SubmitCmdBufferAndWait(device, gfxQueue, cmdBuffer);

            vkResetCommandBuffer(cmdBuffer, 0);
            
            // Dump the buffer to the disk.
            uint32_t bufferDwordsCnt = EnvBrdfMapDim * EnvBrdfMapDim * 4;
            float* pEnvBrdfMapData = new float[bufferDwordsCnt];

            SharedLib::CopyImgToRam(cmdBuffer,
                                    device,
                                    gfxQueue,
                                    allocator,
                                    app.GetEnvBrdfOutputImg(),
                                    envBrdfMapSubresLayers,
                                    { EnvBrdfMapDim, EnvBrdfMapDim, 1},
                                    4, pEnvBrdfMapData);
            
            uint32_t outputDwordsCnt = EnvBrdfMapDim * EnvBrdfMapDim * 3;
            float* pEnvBrdfOutputData = new float[outputDwordsCnt];

            SharedLib::Img4EleTo3Ele(pEnvBrdfMapData, pEnvBrdfOutputData, EnvBrdfMapDim* EnvBrdfMapDim);

            std::string envBrdfMapPathName = outputDir + "/envBrdf.hdr";
            SharedLib::SaveImg(envBrdfMapPathName, EnvBrdfMapDim, EnvBrdfMapDim, 3, pEnvBrdfOutputData);

            delete[] pEnvBrdfMapData;
            delete[] pEnvBrdfOutputData;
        }

        // Copy and paste the input cubemap to the package
        {
            std::string backgroundCubemapOutputPathName = outputDir + "/background_cubemap.hdr";
            std::filesystem::copy_file(inputPathName, backgroundCubemapOutputPathName);
        }

        // End RenderDoc debug
        if (rdoc_api)
        {
            std::cout << "Frame capture ends." << std::endl;
            rdoc_api->EndFrameCapture(NULL, NULL);
        }
    }

    system("pause");
}
