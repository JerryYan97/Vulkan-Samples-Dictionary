#include "GenIBL.h"
#include "vk_mem_alloc.h"

// ================================================================================================================
void GenIBL::InitDiffuseIrradiancePipeline()
{

}

// ================================================================================================================
void GenIBL::InitDiffuseIrradiancePipelineLayout()
{

}

// ================================================================================================================
void GenIBL::InitDiffuseIrradianceShaderModules()
{

}

// ================================================================================================================
void GenIBL::DestroyDiffuseIrradiancePipelineResourses()
{
    vkDestroyShaderModule(m_device, m_diffuseIrradianceVsShaderModule, nullptr);
    vkDestroyShaderModule(m_device, m_diffuseIrradiancePsShaderModule, nullptr);

    // Destroy the pipeline layout
    vkDestroyPipelineLayout(m_device, m_diffuseIrradiancePipelineLayout, nullptr);

    vmaDestroyImage(*m_pAllocator, m_diffuseIrradianceCubemap, m_diffuseIrradianceCubemapAlloc);
    vkDestroyImageView(m_device, m_diffuseIrradianceCubemapImageView, nullptr);
}

// ================================================================================================================
void GenIBL::InitDiffuseIrradianceOutputObjects()
{

}