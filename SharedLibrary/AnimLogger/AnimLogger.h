#pragma once
#include <vulkan/vulkan.h>
#include <chrono>
#include <string>
#include "../VMA/vk_mem_alloc.h"

// NOTE: The anim logger may need more time investment, which maybe not worth my time...
// I can just use video editor to do the hacks...
// The speed of dumping image significantly affects the original app run, which means I may need multithreed
// technique to overcome this problem and spend more time one relevant research... Currently, I can workaround it with
// video cut hacks...
// So, I may don't want to use it anymore...
namespace SharedLib
{
    struct AnimLoggerInitInfo
    {
        uint32_t      logFps;
        float         logDuration; // In the unit of second.
        VmaAllocator* pAllocator;
        std::string   dumpDir; // It has to be a valid directory.
    };

    // Dumped img always has three elements -- RGB.
    // Assume 8 bits per element to save memory.
    struct DumpImgInfo
    {
        uint32_t      width;
        uint32_t      height;
        VkImage       stgImg;
        VmaAllocation stgImgAlloc;
    };

    class AnimLogger
    {
    public:
        AnimLogger();
        ~AnimLogger();

        void Init(AnimLoggerInitInfo initInfo);

        // Copy the color render target data to a staging VkImage. Should be called between the rendering and present
        // layout trans.
        void CmdCopyRenderTargetOut(VkCommandBuffer cmdBuffer, VkImage srcImg, VkExtent2D srcImgExtent);

        // Dump out the render target data stored in the buffer. Should be called after the queue finishes and the
        // present happens. In the previous interface, we record the command of copying out the render target data to
        // a buffer. Thus, we need to wait until the queue finishes all its works which includes our buffer copy. And
        // since we copy the image data out to a buffer, we don't need to care about the layout of the render target.
        void DumpRenderTargetData(VkDevice device, VkQueue submitQueue, VkFence queueExeFence, VkCommandBuffer cmdBuffer);

    protected:

    private:
        uint32_t m_logFps;
        float    m_logDurationRemain; // In the unit of second.
        float    m_logDurationStart;  // In the unit of second.

        bool                                  m_isFirstTimeRecord;
        std::chrono::steady_clock::time_point m_lastTime;
        uint32_t                              m_dumpedImgCnt;

        DumpImgInfo m_dumpImgInfo;
        bool m_isTransDstLayout;

        VmaAllocator* m_pAllocator;

        std::string m_dumpDir;
    };
}