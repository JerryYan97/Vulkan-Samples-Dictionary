#include "AssetsLoader.h"
#include "../Scene/Level.h"
#include "../Scene/Entity.h"
#include "../Utils/StrPathUtils.h"

#define TINYGLTF_IMPLEMENTATION
#include "../../ThirdPartyLibs/TinyGltf/tiny_gltf.h"

namespace SharedLib
{
    // ================================================================================================================
    void GltfLoaderManager::Load(const std::string& absPath,
                                 Level&             oLevel)
    {
        std::string filePostfix;
        if (GetFilePostfix(absPath, filePostfix))
        {
            if (strcmp(filePostfix.c_str(), "gltf") == 0)
            {
                tinygltf::Model model;
                tinygltf::TinyGLTF loader;
                std::string err;
                std::string warn;

                bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, absPath);
                if (!warn.empty()) {
                    printf("Warn: %s\n", warn.c_str());
                }

                if (!err.empty()) {
                    printf("Err: %s\n", err.c_str());
                }

                if (!ret) {
                    printf("Failed to parse glTF\n");
                    exit(1);
                }

                // NOTE: TinyGltf loader has already loaded the binary buffer data and the images data.
                const auto& binaryBuffer = model.buffers[0].data;
                const unsigned char* pBufferData = binaryBuffer.data();
            }
            else if(strcmp(filePostfix.c_str(), "glb") == 0)
            {
                //bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, argv[1]); // for binary glTF(.glb)
            }
            else
            {
                ASSERT(false, "Unsupported GLTF file format.");
            }
        }
        else
        {
            ASSERT(false, "Cannot find a postfix.");
        }
    }
}