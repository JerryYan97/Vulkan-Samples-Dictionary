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

        ImgInfo baseColorTex;         // R8G8B8_SRGB
        ImgInfo metallicRoughnessTex; // R32G32_SFLOAT
        ImgInfo normalTex;            // R32G32B32_SFLOAT
        ImgInfo occlusionTex;         // R32_SFLOAT
        ImgInfo emissiveTex;          // Currently don't support.

    protected:
        GpuBuffer vertBuffer;
        GpuBuffer indexBuffer;

        GpuImg baseColorGpuImg;
        GpuImg metallicRoughnessGpuImg;
        GpuImg normalGpuImg;
        GpuImg occlusionGpuImg;
        GpuImg emissiveGpuImg;
    };

    class MeshEntity : public Entity
    {
    public:
        MeshEntity() {}
        ~MeshEntity() {}

        void Finialize() override {}

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

    private:
        std::unordered_map<std::string, MeshEntity*>  m_meshEntities;
        std::unordered_map<std::string, LightEntity*> m_lightEntities;
    };
}