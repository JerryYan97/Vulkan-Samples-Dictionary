#pragma once
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

VK_DEFINE_HANDLE(VmaAllocator)

namespace SharedLib
{
    class Level;
    class Entity;

    // AssetsLoaderManager is responsible for loading assets from disk, populating the Level object and manage the
    // entities' release.
    class AssetsLoaderManager
    {
    public:
        AssetsLoaderManager() {}
        ~AssetsLoaderManager();

        virtual void Load(const std::string& absPath, Level& oLevel) = 0;
        void InitEntitesGpuRsrc(VkDevice device, VmaAllocator* pAllocator);
        void FinializeEntities(VkDevice device, VmaAllocator* pAllocator);

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