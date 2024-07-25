#pragma once
#include <string>
#include <vector>

namespace SharedLib
{
    class Level;
    class Entity;

    class AssetsLoaderManager
    {
    public:
        AssetsLoaderManager() {}
        ~AssetsLoaderManager() {}

        virtual void Load(const std::string& absPath, Level& oLevel) = 0;

    protected:
        std::vector<Entity*> m_entities;
    };

    class GltfLoaderManager : public AssetsLoaderManager
    {
    public:
        GltfLoaderManager() {}
        ~GltfLoaderManager() {}

        void Load(const std::string& absPath, Level& oLevel) override;
    };
}