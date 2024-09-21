#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include "../Application/Application.h"

namespace SharedLib
{
    class Entity
    {
    public:
        Entity() {}
        ~Entity() {}

        virtual void InitGpuRsrc(VkDevice device, VmaAllocator* pAllocator, VkCommandBuffer cmdBuffer, VkQueue queue) {}
        virtual void Finialize() = 0;

        float m_position[3];

    private:
        // virtual void Update() = 0;
        // virtual void Render() = 0;
    };

    class MeshPrimitive
    {
    public:
        MeshPrimitive() {}
        ~MeshPrimitive() {}
        
        // float m_position[3];
        std::vector<float>    m_vertData;
        
        std::vector<float>    m_posData;
        std::vector<float>    m_normalData;
        std::vector<float>    m_tangentData;
        std::vector<float>    m_texCoordData;

        std::vector<uint16_t> m_idxDataUint16;

        ImgInfo m_baseColorTex;         // TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE (5121), 4 components.
        ImgInfo m_metallicRoughnessTex; // R32G32_SFLOAT
        ImgInfo m_normalTex;            // R32G32B32_SFLOAT
        ImgInfo m_occlusionTex;         // R32_SFLOAT
        ImgInfo m_emissiveTex;          // Currently don't support.

        void InitGpuRsrc(VkDevice device, VmaAllocator* pAllocator, VkCommandBuffer cmdBuffer, VkQueue queue);
        void FinializeGpuRsrc(VkDevice device, VmaAllocator* pAllocator);

        VkBuffer* GetVertBuffer() { return &m_vertBuffer.buffer; }
        VkBuffer  GetIndexBuffer() { return m_indexBuffer.buffer; }

        VkDescriptorImageInfo* GetBaseColorImgDescInfo() { return &m_baseColorGpuImg.imageDescInfo; }
        VkDescriptorImageInfo* GetMetallicRoughnessImgDescInfo() { return &m_metallicRoughnessGpuImg.imageDescInfo; }
        VkDescriptorImageInfo* GetNormalImgDescInfo() { return &m_normalGpuImg.imageDescInfo; }
        VkDescriptorImageInfo* GetOcclusionImgDescInfo() { return &m_occlusionGpuImg.imageDescInfo; }
        VkDescriptorImageInfo* GetEmissiveImgDescInfo() { return &m_emissiveGpuImg.imageDescInfo; }

    protected:
        GpuBuffer m_vertBuffer;
        GpuBuffer m_indexBuffer;

        GpuImg m_baseColorGpuImg;
        GpuImg m_metallicRoughnessGpuImg;
        GpuImg m_normalGpuImg;
        GpuImg m_occlusionGpuImg;
        GpuImg m_emissiveGpuImg;
    };

    class MeshEntity : public Entity
    {
    public:
        MeshEntity() {}
        ~MeshEntity() {}

        void Finialize() override {}

        virtual void InitGpuRsrc(VkDevice device, VmaAllocator* pAllocator, VkCommandBuffer cmdBuffer, VkQueue queue) override;
        virtual void FinializeGpuRsrc(VkDevice device, VmaAllocator* pAllocator);

        std::vector<MeshPrimitive> m_meshPrimitives;
    protected:
        
    };

    class SkeletalMeshEntity : public MeshEntity
    {
    public:
        SkeletalMeshEntity() {}
        ~SkeletalMeshEntity() {}
    };

    enum LightType
    {
        POINT_LIGHT,
        DIRECTIONAL_LIGHT,
        MESH_LIGHT
    };

    class LightEntity : public Entity
    {
    public:
        LightEntity() {}
        ~LightEntity() {}

    private:
        LightType m_lightType;
    };

    class Level
    {
    public:
        Level() {}
        ~Level() {}

        void Finalize() {}

        bool AddMshEntity(const std::string& name, MeshEntity* entity);
        bool AddLightEntity(const std::string& name, LightEntity* entity);

        std::unordered_map<std::string, MeshEntity*>  m_meshEntities;
        std::unordered_map<std::string, LightEntity*> m_lightEntities;
    };
}