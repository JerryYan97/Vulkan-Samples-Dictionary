#pragma once
#include <vector>
#include <string>
#include <unordered_map>

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

    };

    class LightEntity : public Entity
    {

    };

    class Level
    {
    public:
        Level() {}
        ~Level() {}

        void Finalize();

        void AddEntity(Entity* entity);
        void RemoveEntity(Entity* entity);

    private:
        std::unordered_map<std::string, Entity*> m_mshEntities;
    };
}