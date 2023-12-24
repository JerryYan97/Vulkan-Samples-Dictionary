#include "PBRDeferredApp.h"
#include <glfw3.h>
#include <cstdlib>
#include "../../../SharedLibrary/Utils/VulkanDbgUtils.h"
#include "../../../SharedLibrary/Camera/Camera.h"
#include "../../../SharedLibrary/Event/Event.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include "vk_mem_alloc.h"

// ================================================================================================================
PBRDeferredApp::PBRDeferredApp() :
    GlfwApplication(),
    m_geoPassVsShaderModule(VK_NULL_HANDLE),
    m_geoPassPsShaderModule(VK_NULL_HANDLE),
    m_geoPassPipelineDesSetLayout(VK_NULL_HANDLE),
    m_geoPassPipelineLayout(VK_NULL_HANDLE),
    m_geoPassPipeline(),
    // m_lightPosBuffer(VK_NULL_HANDLE),
    // m_lightPosBufferAlloc(VK_NULL_HANDLE),
    m_idxBuffer(),
    m_vertBuffer(),
    m_offsetStorageBuffer(),
    m_albedoStorageBuffer(),
    m_metallicRoughnessStorageBuffer(),
    m_lightPosStorageBuffer(),
    m_lightRadianceStorageBuffer(),
    m_vertBufferByteCnt(0),
    m_idxBufferByteCnt(0) //,
    // m_lightPosUboDesBufferInfo()
{
    m_pCamera = new SharedLib::Camera();
}

// ================================================================================================================
PBRDeferredApp::~PBRDeferredApp()
{
    vkDeviceWaitIdle(m_device);
    delete m_pCamera;

    DestroySphereVertexIndexBuffers();

    DestroyVpUboObjects();
    DestroyGBuffer();
    DestroyGeoPassSSBOs();

    // DestroyFragUboObjects();

    // Destroy shader modules
    vkDestroyShaderModule(m_device, m_geoPassVsShaderModule, nullptr);
    vkDestroyShaderModule(m_device, m_geoPassPsShaderModule, nullptr);

    // Destroy the pipeline layout
    vkDestroyPipelineLayout(m_device, m_geoPassPipelineLayout, nullptr);

    // Destroy the descriptor set layout
    vkDestroyDescriptorSetLayout(m_device, m_geoPassPipelineDesSetLayout, nullptr);
}

// ================================================================================================================
void PBRDeferredApp::DestroyVpUboObjects()
{
    for (auto buffer : m_vpUboBuffers)
    {
        vmaDestroyBuffer(*m_pAllocator, buffer.buffer, buffer.bufferAlloc);
    }    
}

// ================================================================================================================
void PBRDeferredApp::InitVpUboObjects()
{
    float defaultPos[] = {-12.f, 0.f, 0.f};
    m_pCamera->SetPos(defaultPos);

    // The alignment of a vec3 is 4 floats and the element alignment of a struct is the largest element alignment,
    // which is also the 4 float. Therefore, we need 32 floats as the buffer to store the VP's parameters.
    VkBufferCreateInfo bufferInfo{};
    {
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = 16 * sizeof(float);
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo bufferAllocInfo{};
    {
        bufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        bufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
                                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    m_vpUboBuffers.resize(m_swapchainImgCnt);

    float vpMat[16] = {};
    float tmpViewMat[16] = {};
    float tmpPersMat[16] = {};
    m_pCamera->GenViewPerspectiveMatrices(tmpViewMat, tmpPersMat, vpMat);

    for (uint32_t i = 0; i < m_swapchainImgCnt; i++)
    {
        vmaCreateBuffer(*m_pAllocator,
                        &bufferInfo,
                        &bufferAllocInfo,
                        &m_vpUboBuffers[i].buffer,
                        &m_vpUboBuffers[i].bufferAlloc,
                        nullptr);

        CopyRamDataToGpuBuffer(vpMat,
                               m_vpUboBuffers[i].buffer,
                               m_vpUboBuffers[i].bufferAlloc,
                               16 * sizeof(float));

        // NOTE: For the push descriptors, the dstSet is ignored.
        //       This app doesn't have other resources so a fixed descriptor set is enough.
        {
            m_vpUboBuffers[i].bufferDescInfo.buffer = m_vpUboBuffers[i].buffer;
            m_vpUboBuffers[i].bufferDescInfo.offset = 0;
            m_vpUboBuffers[i].bufferDescInfo.range = sizeof(float) * 16;
        }
    }
}

// ================================================================================================================
// NOTE: A vert = pos + normal + uv.
void PBRDeferredApp::ReadInSphereData()
{
    std::string inputfile = SOURCE_PATH;
    inputfile += "/../data/uvNormalSphere.obj";
    
    tinyobj::ObjReaderConfig readerConfig;
    tinyobj::ObjReader sphereObjReader;

    sphereObjReader.ParseFromFile(inputfile, readerConfig);

    auto& shapes = sphereObjReader.GetShapes();
    auto& attrib = sphereObjReader.GetAttrib();

    // We assume that this test only has one shape
    assert(shapes.size() == 1, "This application only accepts one shape!");

    for (uint32_t s = 0; s < shapes.size(); s++)
    {
        // Loop over faces(polygon)
        uint32_t index_offset = 0;
        uint32_t idxBufIdx = 0;
        for (uint32_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
        {
            uint32_t fv = shapes[s].mesh.num_face_vertices[f];

            // Loop over vertices in the face.
            for (uint32_t v = 0; v < fv; v++)
            {
                // Access to vertex
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

                float vx = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
                float vy = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
                float vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

                // Transfer the vertex buffer's vertex index to the element index -- 6 * vertex index + xxx;
                m_vertData.push_back(vx);
                m_vertData.push_back(vy);
                m_vertData.push_back(vz);

                // Check if `normal_index` is zero or positive. negative = no normal data
                assert(idx.normal_index >= 0, "The model doesn't have normal information but it is necessary.");
                float nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
                float ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
                float nz = attrib.normals[3 * size_t(idx.normal_index) + 2];

                m_vertData.push_back(nx);
                m_vertData.push_back(ny);
                m_vertData.push_back(nz);

                m_idxData.push_back(idxBufIdx);
                idxBufIdx++;
            }
            index_offset += fv;
        }
    }

    m_vertBufferByteCnt = m_vertData.size() * sizeof(float);
    m_idxBufferByteCnt = m_idxData.size() * sizeof(uint32_t);
}

// ================================================================================================================
void PBRDeferredApp::InitSphereVertexIndexBuffers()
{
    // Create sphere data GPU buffers
    VkBufferCreateInfo vertBufferInfo{};
    {
        vertBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        vertBufferInfo.size = m_vertBufferByteCnt;
        vertBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        vertBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo vertBufferAllocInfo{};
    {
        vertBufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        vertBufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
                                    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    vmaCreateBuffer(*m_pAllocator,
        &vertBufferInfo,
        &vertBufferAllocInfo,
        &m_vertBuffer.buffer,
        &m_vertBuffer.bufferAlloc,
        nullptr);

    VkBufferCreateInfo idxBufferInfo{};
    {
        idxBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        idxBufferInfo.size = m_idxBufferByteCnt;
        idxBufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        idxBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo idxBufferAllocInfo{};
    {
        idxBufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        idxBufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
                                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    vmaCreateBuffer(*m_pAllocator,
        &idxBufferInfo,
        &idxBufferAllocInfo,
        &m_idxBuffer.buffer,
        &m_idxBuffer.bufferAlloc,
        nullptr);

    // Send sphere data to the GPU buffers
    CopyRamDataToGpuBuffer(m_vertData.data(), m_vertBuffer.buffer, m_vertBuffer.bufferAlloc, m_vertBufferByteCnt);
    CopyRamDataToGpuBuffer(m_idxData.data(), m_idxBuffer.buffer, m_idxBuffer.bufferAlloc, m_idxBufferByteCnt);
}

// ================================================================================================================
void PBRDeferredApp::DestroySphereVertexIndexBuffers()
{
    vmaDestroyBuffer(*m_pAllocator, m_vertBuffer.buffer, m_vertBuffer.bufferAlloc);
    vmaDestroyBuffer(*m_pAllocator, m_idxBuffer.buffer, m_idxBuffer.bufferAlloc);
}

// ================================================================================================================
/*
void PBRDeferredApp::InitFragUboObjects()
{
    // The alignment of a vec3 is 4 floats and the element alignment of a struct is the largest element alignment,
    // which is also the 4 float. Therefore, we need 16 floats as the buffer to store the Camera's parameters.
    VkBufferCreateInfo bufferInfo{};
    {
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = 20 * sizeof(float);
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo bufferAllocInfo{};
    {
        bufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        bufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
                                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    vmaCreateBuffer(
        *m_pAllocator,
        &bufferInfo,
        &bufferAllocInfo,
        &m_lightPosBuffer,
        &m_lightPosBufferAlloc,
        nullptr);

    float cameraPos[3] = {};
    m_pCamera->GetPos(cameraPos);

    // Copy light and camera data to ubo buffer
    // The last element of each lines is a padding float
    float lightPos[20] = {
        10.f,  3.f, -8.f, 0.f,
        10.f,  3.f,  8.f, 0.f,
        10.f, -3.f, -8.f, 0.f,
        10.f, -3.f,  8.f, 0.f,
        cameraPos[0], cameraPos[1], cameraPos[2], 0.f
    };

    CopyRamDataToGpuBuffer(lightPos, m_lightPosBuffer, m_lightPosBufferAlloc, sizeof(lightPos));

    // NOTE: For the push descriptors, the dstSet is ignored.
    //       This app doesn't have other resources so a fixed descriptor set is enough.
    {
        m_lightPosUboDesBufferInfo.buffer = m_lightPosBuffer;
        m_lightPosUboDesBufferInfo.offset = 0;
        m_lightPosUboDesBufferInfo.range = sizeof(float) * 16;
    }

    VkWriteDescriptorSet writeLightsDesSet{};
    {
        writeLightsDesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeLightsDesSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeLightsDesSet.dstBinding = 1;
        writeLightsDesSet.descriptorCount = 1;
        writeLightsDesSet.pBufferInfo = &m_lightPosUboDesBufferInfo;
    }

    m_writeDescriptorSet0.push_back(writeLightsDesSet);
}
*/

// ================================================================================================================
/*
void PBRDeferredApp::DestroyFragUboObjects()
{
    vmaDestroyBuffer(*m_pAllocator, m_lightPosBuffer, m_lightPosBufferAlloc);
}
*/

// ================================================================================================================
void PBRDeferredApp::CmdGBufferToRenderTarget(
    VkCommandBuffer cmdBuffer,
    VkImageLayout   oldLayout)
{
    VkImageSubresourceRange colorOneMipOneLevelSubResRange{};
    {
        colorOneMipOneLevelSubResRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorOneMipOneLevelSubResRange.baseMipLevel = 0;
        colorOneMipOneLevelSubResRange.levelCount = 1;
        colorOneMipOneLevelSubResRange.baseArrayLayer = 0;
        colorOneMipOneLevelSubResRange.layerCount = 1;
    }

    std::vector<VkImageMemoryBarrier> gBufferToRenderTargetBarriers;

    // Transform the layout of the GBuffer textures from undefined to render target.
    VkImageMemoryBarrier gBufferRenderTargetTransBarrierTemplate{};
    {
        gBufferRenderTargetTransBarrierTemplate.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        gBufferRenderTargetTransBarrierTemplate.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        gBufferRenderTargetTransBarrierTemplate.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        gBufferRenderTargetTransBarrierTemplate.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        // swapchainRenderTargetTransBarrier.image = GetSwapchainColorImage(m_currentFrame);
        gBufferRenderTargetTransBarrierTemplate.subresourceRange = colorOneMipOneLevelSubResRange;
    }

    gBufferRenderTargetTransBarrierTemplate.image = m_worldPosTextures[m_currentFrame].image;
    gBufferToRenderTargetBarriers.push_back(gBufferRenderTargetTransBarrierTemplate);

    gBufferRenderTargetTransBarrierTemplate.image = m_normalTextures[m_currentFrame].image;
    gBufferToRenderTargetBarriers.push_back(gBufferRenderTargetTransBarrierTemplate);

    gBufferRenderTargetTransBarrierTemplate.image = m_albedoTextures[m_currentFrame].image;
    gBufferToRenderTargetBarriers.push_back(gBufferRenderTargetTransBarrierTemplate);

    gBufferRenderTargetTransBarrierTemplate.image = m_metallicRoughnessTextures[m_currentFrame].image;
    gBufferToRenderTargetBarriers.push_back(gBufferRenderTargetTransBarrierTemplate);

    vkCmdPipelineBarrier(
        cmdBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        0, nullptr,
        0, nullptr,
        gBufferToRenderTargetBarriers.size(),
        gBufferToRenderTargetBarriers.data());
}

// ================================================================================================================
void PBRDeferredApp::CmdGBufferToShaderInput(
    VkCommandBuffer cmdBuffer,
    VkImageLayout oldLayout)
{
    // TODO: Finish when you are implementing the lighting pass.
}

// ================================================================================================================
std::vector<VkRenderingAttachmentInfoKHR> PBRDeferredApp::GetGBufferAttachments()
{
    VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };

    std::vector<VkRenderingAttachmentInfoKHR> attachmentsInfos;

    VkRenderingAttachmentInfoKHR attachmentInfo{};
    {
        attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
        // colorAttachmentInfo.imageView = app.GetSwapchainColorImageView(imageIndex);
        attachmentInfo.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
        attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachmentInfo.clearValue = clearColor;
    }

    attachmentInfo.imageView = m_worldPosTextures[m_currentFrame].imageView;
    attachmentsInfos.push_back(attachmentInfo);

    attachmentInfo.imageView = m_normalTextures[m_currentFrame].imageView;
    attachmentsInfos.push_back(attachmentInfo);

    attachmentInfo.imageView = m_albedoTextures[m_currentFrame].imageView;
    attachmentsInfos.push_back(attachmentInfo);

    attachmentInfo.imageView = m_metallicRoughnessTextures[m_currentFrame].imageView;
    attachmentsInfos.push_back(attachmentInfo);

    return attachmentsInfos;
}

// ================================================================================================================
std::vector<VkWriteDescriptorSet> PBRDeferredApp::GetGeoPassWriteDescriptorSets()
{
    std::vector<VkWriteDescriptorSet> geoPassWriteDescSet;

    VkWriteDescriptorSet writeVpUboDesc{};
    {
        writeVpUboDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeVpUboDesc.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeVpUboDesc.descriptorCount = 1;
        writeVpUboDesc.dstBinding = 0;        
        writeVpUboDesc.pBufferInfo = &m_vpUboBuffers[m_currentFrame].bufferDescInfo;
    }
    geoPassWriteDescSet.push_back(writeVpUboDesc);

    VkWriteDescriptorSet writeOffsetSSBODesc{};
    {
        writeOffsetSSBODesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeOffsetSSBODesc.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writeOffsetSSBODesc.descriptorCount = 1;
        writeOffsetSSBODesc.dstBinding = 1;
        writeOffsetSSBODesc.pBufferInfo = &m_offsetStorageBuffer.bufferDescInfo;
    }
    geoPassWriteDescSet.push_back(writeOffsetSSBODesc);

    VkWriteDescriptorSet writeAlbedoSSBODesc{};
    {
        writeAlbedoSSBODesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeAlbedoSSBODesc.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writeAlbedoSSBODesc.descriptorCount = 1;
        writeAlbedoSSBODesc.dstBinding = 2;
        writeAlbedoSSBODesc.pBufferInfo = &m_albedoStorageBuffer.bufferDescInfo;
    }
    geoPassWriteDescSet.push_back(writeAlbedoSSBODesc);

    VkWriteDescriptorSet writeMetallicRoughnessSSBODesc{};
    {
        writeMetallicRoughnessSSBODesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeMetallicRoughnessSSBODesc.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writeMetallicRoughnessSSBODesc.descriptorCount = 1;
        writeMetallicRoughnessSSBODesc.dstBinding = 3;
        writeMetallicRoughnessSSBODesc.pBufferInfo = &m_metallicRoughnessStorageBuffer.bufferDescInfo;
    }
    geoPassWriteDescSet.push_back(writeMetallicRoughnessSSBODesc);

    return geoPassWriteDescSet;
}

// ================================================================================================================
void PBRDeferredApp::InitGeoPassPipelineLayout()
{
    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    {
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_geoPassPipelineDesSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
    }
    
    VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_geoPassPipelineLayout));
}

// ================================================================================================================
void PBRDeferredApp::InitGeoPassShaderModules()
{
    // Create Shader Modules.
    m_geoPassVsShaderModule = CreateShaderModule("/hlsl/geo_vert.spv");
    m_geoPassPsShaderModule = CreateShaderModule("/hlsl/geo_frag.spv");
}

// ================================================================================================================
void PBRDeferredApp::InitGeoPassPipelineDescriptorSetLayout()
{
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    // Create pipeline binding and descriptor objects for the camera parameters
    VkDescriptorSetLayoutBinding cameraUboBinding{};
    {
        cameraUboBinding.binding = 0;
        cameraUboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        cameraUboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        cameraUboBinding.descriptorCount = 1;
    }
    bindings.push_back(cameraUboBinding);

    // Binding for the spheres' offsets
    VkDescriptorSetLayoutBinding offsetsSSBOBinding{};
    {
        offsetsSSBOBinding.binding = 1;
        offsetsSSBOBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        offsetsSSBOBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        offsetsSSBOBinding.descriptorCount = 1;
    }
    bindings.push_back(offsetsSSBOBinding);

    // Binding for the spheres' albedos
    VkDescriptorSetLayoutBinding albedoSSBOBinding{};
    {
        albedoSSBOBinding.binding = 2;
        albedoSSBOBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        albedoSSBOBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        albedoSSBOBinding.descriptorCount = 1;
    }
    bindings.push_back(albedoSSBOBinding);

    // Binding for the spheres' metallic roughness material parameters
    VkDescriptorSetLayoutBinding metallicRoughnessSSBOBinding{};
    {
        metallicRoughnessSSBOBinding.binding = 3;
        metallicRoughnessSSBOBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        metallicRoughnessSSBOBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        metallicRoughnessSSBOBinding.descriptorCount = 1;
    }
    bindings.push_back(metallicRoughnessSSBOBinding);

    // Create pipeline's descriptors layout
    // The Vulkan spec states: The VkDescriptorSetLayoutBinding::binding members of the elements of the pBindings array 
    // must each have different values 
    // (https://vulkan.lunarg.com/doc/view/1.3.236.0/windows/1.3-extensions/vkspec.html#VUID-VkDescriptorSetLayoutCreateInfo-binding-00279)
    VkDescriptorSetLayoutCreateInfo pipelineDesSetLayoutInfo{};
    {
        pipelineDesSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        // Setting this flag tells the descriptor set layouts that no actual descriptor sets are allocated but instead pushed at command buffer creation time
        pipelineDesSetLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
        pipelineDesSetLayoutInfo.bindingCount = bindings.size();
        pipelineDesSetLayoutInfo.pBindings = bindings.data();
    }

    VK_CHECK(vkCreateDescriptorSetLayout(m_device,
                                         &pipelineDesSetLayoutInfo,
                                         nullptr,
                                         &m_geoPassPipelineDesSetLayout));
}

// ================================================================================================================
VkPipelineVertexInputStateCreateInfo PBRDeferredApp::CreateGeoPassPipelineVertexInputInfo()
{
    // Specifying all kinds of pipeline states
    // Vertex input state
    VkVertexInputBindingDescription* pVertBindingDesc = new VkVertexInputBindingDescription();
    memset(pVertBindingDesc, 0, sizeof(VkVertexInputBindingDescription));
    {
        pVertBindingDesc->binding = 0;
        pVertBindingDesc->stride = 6 * sizeof(float);
        pVertBindingDesc->inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    }
    m_heapMemPtrVec.push_back(pVertBindingDesc);

    VkVertexInputAttributeDescription* pVertAttrDescs = new VkVertexInputAttributeDescription[2];
    memset(pVertAttrDescs, 0, sizeof(VkVertexInputAttributeDescription) * 2);
    {
        // Position
        pVertAttrDescs[0].location = 0;
        pVertAttrDescs[0].binding = 0;
        pVertAttrDescs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        pVertAttrDescs[0].offset = 0;
        // Normal
        pVertAttrDescs[1].location = 1;
        pVertAttrDescs[1].binding = 0;
        pVertAttrDescs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        pVertAttrDescs[1].offset = 3 * sizeof(float);
    }
    m_heapArrayMemPtrVec.push_back(pVertAttrDescs);

    VkPipelineVertexInputStateCreateInfo vertInputInfo{};
    {
        vertInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertInputInfo.pNext = nullptr;
        vertInputInfo.vertexBindingDescriptionCount = 1;
        vertInputInfo.pVertexBindingDescriptions = pVertBindingDesc;
        vertInputInfo.vertexAttributeDescriptionCount = 2;
        vertInputInfo.pVertexAttributeDescriptions = pVertAttrDescs;
    }

    return vertInputInfo;
}

// ================================================================================================================
VkPipelineDepthStencilStateCreateInfo PBRDeferredApp::CreateGeoPassDepthStencilStateInfo()
{
    VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};
    {
        depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencilInfo.depthTestEnable = VK_TRUE;
        depthStencilInfo.depthWriteEnable = VK_TRUE;
        depthStencilInfo.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL; // Reverse depth for higher precision. 
        depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
        depthStencilInfo.stencilTestEnable = VK_FALSE;
    }

    return depthStencilInfo;
}

// ================================================================================================================
std::vector<VkPipelineColorBlendAttachmentState> PBRDeferredApp::CreateGeoPassPipelineColorBlendAttachmentStates()
{
    std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates;

    VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
    colorBlendAttachmentState.colorWriteMask = 0xf;
    colorBlendAttachmentState.blendEnable = VK_FALSE;

    colorBlendAttachmentStates.push_back(colorBlendAttachmentState);
    colorBlendAttachmentStates.push_back(colorBlendAttachmentState);
    colorBlendAttachmentStates.push_back(colorBlendAttachmentState);
    colorBlendAttachmentStates.push_back(colorBlendAttachmentState);

    return colorBlendAttachmentStates;
}

// ================================================================================================================
void PBRDeferredApp::InitGeoPassPipeline()
{
    VkPipelineRenderingCreateInfoKHR pipelineRenderCreateInfo{};
    {
        pipelineRenderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        pipelineRenderCreateInfo.colorAttachmentCount = m_gBufferFormats.size();
        pipelineRenderCreateInfo.pColorAttachmentFormats = m_gBufferFormats.data();
        pipelineRenderCreateInfo.depthAttachmentFormat = VK_FORMAT_D16_UNORM;
    }

    m_geoPassPipeline.SetPNext(&pipelineRenderCreateInfo);
    m_geoPassPipeline.SetPipelineLayout(m_geoPassPipelineLayout);

    VkPipelineVertexInputStateCreateInfo vertInputInfo = CreateGeoPassPipelineVertexInputInfo();
    m_geoPassPipeline.SetVertexInputInfo(&vertInputInfo);

    VkPipelineDepthStencilStateCreateInfo depthStencilInfo = CreateGeoPassDepthStencilStateInfo();
    m_geoPassPipeline.SetDepthStencilStateInfo(&depthStencilInfo);

    std::vector<VkPipelineColorBlendAttachmentState> colorAttachmentsBlendInfos = CreateGeoPassPipelineColorBlendAttachmentStates();
    m_geoPassPipeline.SetPipelineColorBlendInfo(colorAttachmentsBlendInfos);

    VkPipelineShaderStageCreateInfo shaderStgsInfo[2] = {};
    shaderStgsInfo[0] = CreateDefaultShaderStgCreateInfo(m_geoPassVsShaderModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStgsInfo[1] = CreateDefaultShaderStgCreateInfo(m_geoPassPsShaderModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    m_geoPassPipeline.SetShaderStageInfo(shaderStgsInfo, 2);

    m_geoPassPipeline.CreatePipeline(m_device);
}

// ================================================================================================================
void PBRDeferredApp::InitOffsetSSBO()
{
    std::vector<float> offsets;
    constexpr float XBase = -4.5f;
    constexpr float ZBase = -4.5f;
    constexpr float YBase =  0.f;

    for (uint32_t row = 0; row < 4; row++)
    {
        for (uint32_t col = 0; col < 4; col++)
        {
            float xOffset = XBase + row * 3.f;
            float zOffset = ZBase + col * 3.f;
            offsets.push_back(xOffset);
            offsets.push_back(YBase);
            offsets.push_back(zOffset);
            offsets.push_back(1.f);
        }
    }

    const uint32_t bufferBytesCnt = offsets.size() * sizeof(float);
    VkBufferCreateInfo bufferInfo{};
    {
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferBytesCnt;
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo bufferAllocInfo{};
    {
        bufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        bufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
                                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    vmaCreateBuffer(
        *m_pAllocator,
        &bufferInfo,
        &bufferAllocInfo,
        &m_offsetStorageBuffer.buffer,
        &m_offsetStorageBuffer.bufferAlloc,
        nullptr);

    CopyRamDataToGpuBuffer(offsets.data(),
                           m_offsetStorageBuffer.buffer,
                           m_offsetStorageBuffer.bufferAlloc,
                           bufferBytesCnt);

    VkDescriptorBufferInfo offsetSSBODescInfo{};
    {
        offsetSSBODescInfo.buffer = m_offsetStorageBuffer.buffer;
        offsetSSBODescInfo.offset = 0;
        offsetSSBODescInfo.range = bufferBytesCnt;
    }
    m_offsetStorageBuffer.bufferDescInfo = offsetSSBODescInfo;
}

// ================================================================================================================
void PBRDeferredApp::DestroyGeoPassSSBOs()
{
    vmaDestroyBuffer(*m_pAllocator, m_offsetStorageBuffer.buffer, m_offsetStorageBuffer.bufferAlloc);
    vmaDestroyBuffer(*m_pAllocator, m_albedoStorageBuffer.buffer, m_albedoStorageBuffer.bufferAlloc);
    vmaDestroyBuffer(*m_pAllocator,
                     m_metallicRoughnessStorageBuffer.buffer,
                     m_metallicRoughnessStorageBuffer.bufferAlloc);
}

// ================================================================================================================
void PBRDeferredApp::InitAlbedoSSBO()
{
    // Just all white
    std::vector<float> albedos;
    for (uint32_t i = 0; i < SphereCounts; i++)
    {
        albedos.push_back(1.f);
        albedos.push_back(1.f);
        albedos.push_back(1.f);
        albedos.push_back(1.f);
    }

    const uint32_t bufferBytesCnt = albedos.size() * sizeof(float);
    VkBufferCreateInfo bufferInfo{};
    {
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferBytesCnt;
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo bufferAllocInfo{};
    {
        bufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        bufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    vmaCreateBuffer(
        *m_pAllocator,
        &bufferInfo,
        &bufferAllocInfo,
        &m_albedoStorageBuffer.buffer,
        &m_albedoStorageBuffer.bufferAlloc,
        nullptr);

    CopyRamDataToGpuBuffer(albedos.data(),
        m_albedoStorageBuffer.buffer,
        m_albedoStorageBuffer.bufferAlloc,
        bufferBytesCnt);

    VkDescriptorBufferInfo albedoSSBODescInfo{};
    {
        albedoSSBODescInfo.buffer = m_albedoStorageBuffer.buffer;
        albedoSSBODescInfo.offset = 0;
        albedoSSBODescInfo.range = bufferBytesCnt;
    }
    m_albedoStorageBuffer.bufferDescInfo = albedoSSBODescInfo;
}

// ================================================================================================================
void PBRDeferredApp::InitMetallicRoughnessSSBO()
{
    std::vector<float> metallicRoughness;
    for (uint32_t i = 0; i < SphereCounts; i++)
    {
        float metallic = ((float)rand()) / ((float)RAND_MAX);
        float roughness = ((float)rand()) / ((float)RAND_MAX);

        metallicRoughness.push_back(metallic);
        metallicRoughness.push_back(roughness);
    }

    const uint32_t bufferBytesCnt = metallicRoughness.size() * sizeof(float);
    VkBufferCreateInfo bufferInfo{};
    {
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferBytesCnt;
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo bufferAllocInfo{};
    {
        bufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        bufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
                                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    vmaCreateBuffer(
        *m_pAllocator,
        &bufferInfo,
        &bufferAllocInfo,
        &m_metallicRoughnessStorageBuffer.buffer,
        &m_metallicRoughnessStorageBuffer.bufferAlloc,
        nullptr);

    CopyRamDataToGpuBuffer(metallicRoughness.data(),
        m_metallicRoughnessStorageBuffer.buffer,
        m_metallicRoughnessStorageBuffer.bufferAlloc,
        bufferBytesCnt);

    VkDescriptorBufferInfo metallicRoughnessSSBODescInfo{};
    {
        metallicRoughnessSSBODescInfo.buffer = m_metallicRoughnessStorageBuffer.buffer;
        metallicRoughnessSSBODescInfo.offset = 0;
        metallicRoughnessSSBODescInfo.range = bufferBytesCnt;
    }
    m_metallicRoughnessStorageBuffer.bufferDescInfo = metallicRoughnessSSBODescInfo;
}

// ================================================================================================================
void PBRDeferredApp::InitGBuffer()
{
    // It looks like AMD doesn't support the R32G32B32_SFLOAT image format.
    const VkFormat PosNormalAlbedoImgFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
    const VkFormat MetallicRoughnessImgFormat = VK_FORMAT_R32G32_SFLOAT;

    VkImageCreateInfo posNormalAlbedoImageInfo{};
    {
        posNormalAlbedoImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        posNormalAlbedoImageInfo.imageType = VK_IMAGE_TYPE_2D;
        posNormalAlbedoImageInfo.format = PosNormalAlbedoImgFormat;
        posNormalAlbedoImageInfo.extent.width = m_swapchainImageExtent.width;
        posNormalAlbedoImageInfo.extent.height = m_swapchainImageExtent.height;
        posNormalAlbedoImageInfo.extent.depth = 1;
        posNormalAlbedoImageInfo.mipLevels = 1;
        posNormalAlbedoImageInfo.arrayLayers = 1;
        posNormalAlbedoImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        posNormalAlbedoImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        posNormalAlbedoImageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        posNormalAlbedoImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        posNormalAlbedoImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    }

    VkImageCreateInfo metallicRoughnessImageInfo = posNormalAlbedoImageInfo;
    metallicRoughnessImageInfo.format = MetallicRoughnessImgFormat;

    VmaAllocationCreateInfo gBufferImgAllocInfo{};
    {
        gBufferImgAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        gBufferImgAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    VkImageSubresourceRange oneMipOneLayerSubRsrcRange{};
    {
        oneMipOneLayerSubRsrcRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        oneMipOneLayerSubRsrcRange.baseMipLevel = 0;
        oneMipOneLayerSubRsrcRange.levelCount = 1;
        oneMipOneLayerSubRsrcRange.baseArrayLayer = 0;
        oneMipOneLayerSubRsrcRange.layerCount = 1;
    }

    VkImageViewCreateInfo posNormalAlbedoImageViewInfo{};
    {
        posNormalAlbedoImageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        // posNormalAlbedoImageViewInfo.image = colorImage;
        posNormalAlbedoImageViewInfo.format = PosNormalAlbedoImgFormat;
        posNormalAlbedoImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        posNormalAlbedoImageViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        posNormalAlbedoImageViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        posNormalAlbedoImageViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        posNormalAlbedoImageViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        posNormalAlbedoImageViewInfo.subresourceRange = oneMipOneLayerSubRsrcRange;
    }

    VkImageViewCreateInfo metallicRoughnessImageViewInfo = posNormalAlbedoImageViewInfo;
    metallicRoughnessImageViewInfo.format = MetallicRoughnessImgFormat;

    m_gBufferFormats.push_back(PosNormalAlbedoImgFormat);
    m_gBufferFormats.push_back(PosNormalAlbedoImgFormat);
    m_gBufferFormats.push_back(PosNormalAlbedoImgFormat);
    m_gBufferFormats.push_back(MetallicRoughnessImgFormat);

    m_worldPosTextures.resize(m_swapchainImgCnt);
    m_normalTextures.resize(m_swapchainImgCnt);
    m_albedoTextures.resize(m_swapchainImgCnt);
    m_metallicRoughnessTextures.resize(m_swapchainImgCnt);

    for (uint32_t i = 0; i < m_swapchainImgCnt; i++)
    {
        vmaCreateImage(*m_pAllocator,
                       &posNormalAlbedoImageInfo,
                       &gBufferImgAllocInfo,
                       &m_worldPosTextures[i].image,
                       &m_worldPosTextures[i].imageAllocation, nullptr);

        vmaCreateImage(*m_pAllocator,
                       &posNormalAlbedoImageInfo,
                       &gBufferImgAllocInfo,
                       &m_normalTextures[i].image,
                       &m_normalTextures[i].imageAllocation, nullptr);

        vmaCreateImage(*m_pAllocator,
                       &posNormalAlbedoImageInfo,
                       &gBufferImgAllocInfo,
                       &m_albedoTextures[i].image,
                       &m_albedoTextures[i].imageAllocation, nullptr);

        vmaCreateImage(*m_pAllocator,
                       &metallicRoughnessImageInfo,
                       &gBufferImgAllocInfo,
                       &m_metallicRoughnessTextures[i].image,
                       &m_metallicRoughnessTextures[i].imageAllocation, nullptr);
        
        posNormalAlbedoImageViewInfo.image = m_worldPosTextures[i].image;
        VK_CHECK(vkCreateImageView(m_device, &posNormalAlbedoImageViewInfo, nullptr, &m_worldPosTextures[i].imageView));

        posNormalAlbedoImageViewInfo.image = m_normalTextures[i].image;
        VK_CHECK(vkCreateImageView(m_device, &posNormalAlbedoImageViewInfo, nullptr, &m_normalTextures[i].imageView));

        posNormalAlbedoImageViewInfo.image = m_albedoTextures[i].image;
        VK_CHECK(vkCreateImageView(m_device, &posNormalAlbedoImageViewInfo, nullptr, &m_albedoTextures[i].imageView));

        metallicRoughnessImageViewInfo.image = m_metallicRoughnessTextures[i].image;
        VK_CHECK(vkCreateImageView(m_device, &metallicRoughnessImageViewInfo, nullptr, &m_metallicRoughnessTextures[i].imageView));

        // TODO: Create samplers
    }
}

void PBRDeferredApp::DestroyGBuffer()
{
    for (uint32_t i = 0; i < m_swapchainImgCnt; i++)
    {
        vmaDestroyImage(*m_pAllocator, m_worldPosTextures[i].image, m_worldPosTextures[i].imageAllocation);
        vmaDestroyImage(*m_pAllocator, m_normalTextures[i].image, m_normalTextures[i].imageAllocation);
        vmaDestroyImage(*m_pAllocator, m_albedoTextures[i].image, m_albedoTextures[i].imageAllocation);
        vmaDestroyImage(*m_pAllocator, m_metallicRoughnessTextures[i].image, m_metallicRoughnessTextures[i].imageAllocation);

        vkDestroyImageView(m_device, m_worldPosTextures[i].imageView, nullptr);
        vkDestroyImageView(m_device, m_normalTextures[i].imageView, nullptr);
        vkDestroyImageView(m_device, m_albedoTextures[i].imageView, nullptr);
        vkDestroyImageView(m_device, m_metallicRoughnessTextures[i].imageView, nullptr);

        // TODO: Destroy samplers
    }
}

// ================================================================================================================
void PBRDeferredApp::AppInit()
{
    glfwInit();
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> instExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    InitInstance(instExtensions, glfwExtensionCount);

    // Init glfw window.
    InitGlfwWindowAndCallbacks();

    // Create vulkan surface from the glfw window.
    VK_CHECK(glfwCreateWindowSurface(m_instance, m_pWindow, nullptr, &m_surface));

    InitPhysicalDevice();
    InitGfxQueueFamilyIdx();
    InitPresentQueueFamilyIdx();

    // Queue family index should be unique in vk1.2:
    // https://vulkan.lunarg.com/doc/view/1.2.198.0/windows/1.2-extensions/vkspec.html#VUID-VkDeviceCreateInfo-queueFamilyIndex-02802
    std::vector<VkDeviceQueueCreateInfo> deviceQueueInfos = CreateDeviceQueueInfos({ m_graphicsQueueFamilyIdx,
                                                                                     m_presentQueueFamilyIdx });
    // Dummy device extensions vector. Swapchain, dynamic rendering and push descriptors are enabled by default.
    // We have tools that don't need the swapchain extension and the swapchain extension requires surface instance extensions.
    const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    InitDevice(deviceExtensions, deviceQueueInfos, nullptr);
    InitKHRFuncPtrs();
    InitVmaAllocator();
    InitGraphicsQueue();
    InitPresentQueue();

    InitGfxCommandPool();
    InitGfxCommandBuffers(SharedLib::MAX_FRAMES_IN_FLIGHT);

    InitSwapchain();
    
    // Create the graphics pipeline
    ReadInSphereData();
    InitSphereVertexIndexBuffers();

    InitVpUboObjects();
    InitOffsetSSBO();
    InitAlbedoSSBO();
    InitMetallicRoughnessSSBO();
    InitGBuffer();

    InitGeoPassShaderModules();
    InitGeoPassPipelineDescriptorSetLayout();
    InitGeoPassPipelineLayout();
    InitGeoPassPipeline();

    // InitFragUboObjects();
    InitSwapchainSyncObjects();
}