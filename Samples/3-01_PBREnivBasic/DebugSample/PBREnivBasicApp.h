#pragma once
#include "../../3-00_SharedLibrary/Application.h"

VK_DEFINE_HANDLE(VmaAllocation);

namespace SharedLib
{
    class Camera;
}

class PBREnivBasicApp : public SharedLib::GlfwApplication
{
public:
    PBREnivBasicApp();
    ~PBREnivBasicApp();

    virtual void AppInit() override;

private:
    VkImage       m_hdrCubeMapImage;
    VkImageView   m_hdrCubeMapView;
    VkSampler     m_hdrSampler;
    VmaAllocation m_hdrCubeMapAlloc;

    SharedLib::Camera*           m_pCamera;
    std::vector<VkBuffer>        m_cameraParaBuffers;
    std::vector<VmaAllocation>   m_cameraParaBufferAllocs;
    std::vector<VkDescriptorSet> m_skyboxPipelineDescriptorSet0s;
};
