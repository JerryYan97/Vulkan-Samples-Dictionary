#include "SphericalToCubemap.h"
#include "args.hxx"
#include "../../SharedLibrary/Utils/CmdBufUtils.h"

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

    // Just get a command buffer from 
    // VkCommandBuffer cmdBuffer = app.GetCurrentFrameGfxCmdBuffer();

    // Send hdri data to its gpu objects through a staging buffer.
    {
        // SharedLib::CmdSendImgDataToGpu(cmdBuffer, );
    }
}