#pragma once
#include "../../../SharedLibrary/Application/DearImGuiApplication.h"
#include "../../../SharedLibrary/Pipeline/Pipeline.h"
#include <array>

VK_DEFINE_HANDLE(VmaAllocation);

namespace SharedLib
{
    class Camera;
    class GltfLoaderManager;
    class Level;
}

class VulkanRTApp : public SharedLib::ImGuiApplication
{
public:
    VulkanRTApp();
    ~VulkanRTApp();

    virtual void AppInit() override;
    virtual void ImGuiFrame(VkCommandBuffer cmdBuffer) override;

private:
    void InitVertIdxTransMatBuffers();
    void InitRayTracingFuncPtrs();

    void CreateAccelerationStructures();

    SharedLib::Camera* m_pCamera;

    SharedLib::GpuBuffer m_vertBuffer;
    SharedLib::GpuBuffer m_idxBuffer;
    SharedLib::GpuBuffer m_transformMatBuffer;

    struct BottomLevelAccelerationStructure
    {
        VkAccelerationStructureKHR accStructure;
        VkBuffer accStructureBuffer;
        VmaAllocation accStructureBufferAlloc;
        uint64_t deviceAddress;
        VkBuffer scratchBuffer;
        VmaAllocation scratchBufferAlloc;
    };
    BottomLevelAccelerationStructure m_bottomLevelAccelerationStructure;
};