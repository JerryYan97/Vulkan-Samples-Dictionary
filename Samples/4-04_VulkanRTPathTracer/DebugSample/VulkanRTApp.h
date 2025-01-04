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
    SharedLib::Camera* m_pCamera;
};