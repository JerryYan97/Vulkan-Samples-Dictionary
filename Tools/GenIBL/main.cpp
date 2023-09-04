#include "vk_mem_alloc.h"
#include "args.hxx"

#include "GenIBL.h"
#include "../../SharedLibrary/Utils/VulkanDbgUtils.h"

#include <vulkan/vulkan.h>
#include <Windows.h>

#include "renderdoc_app.h"

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

    GenIBL app;
    app.AppInit();

    if (rdoc_api)
    {
        rdoc_api->EndFrameCapture(NULL, NULL);
    }
}
