#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include "GpuRsrc.h"

namespace SharedLib
{
    class Entity
    {
    public:
        Entity() {}
        ~Entity() {}

        virtual void Finialize() = 0;

    private:
        float m_position[3];

        virtual void Update() = 0;
        virtual void Render() = 0;
    };

    class MeshEntity : public Entity
    {
    public:
        MeshEntity() {}
        ~MeshEntity() {}

    private:
        ImgInfo baseColorTex;
        ImgInfo metallicRoughnessTex;
        ImgInfo normalTex;
        ImgInfo occlusionTex;
        ImgInfo emissiveTex;

        GpuBuffer vertBuffer;
        GpuBuffer indexBuffer;

        GpuImage baseColorGpuImg;
        GpuImage metallicRoughnessGpuImg;
        GpuImage normalGpuImg;
        GpuImage occlusionGpuImg;
        GpuImage emissiveGpuImg;
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