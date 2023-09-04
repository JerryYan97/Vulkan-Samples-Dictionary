// #define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include "args.hxx"

#include "GenIBL.h"
#include "../../SharedLibrary/Utils/VulkanDbgUtils.h"

#include <vulkan/vulkan.h>

#include "renderdoc_app.h"

int main()
{
    GenIBL app;
    app.AppInit();

}
