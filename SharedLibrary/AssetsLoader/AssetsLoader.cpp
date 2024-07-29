#include "AssetsLoader.h"
#include "../Scene/Level.h"
#include "../Utils/StrPathUtils.h"
#include "../Utils/GltfUtils.h"
#include "../Utils/MathUtils.h"

#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"

namespace SharedLib
{
    // ================================================================================================================
    AssetsLoaderManager::~AssetsLoaderManager()
    {
        for (auto entity : m_entities)
        {
            entity->Finialize();
            delete entity;
        }
    }

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

                // NOTE: (1): TinyGltf loader has already loaded the binary buffer data and the images data.
                //       (2): The gltf may has multiple buffers. The buffer idx should come from the buffer view.
                //       (3): Be aware of the byte stride: https://github.com/KhronosGroup/glTF-Tutorials/blob/main/gltfTutorial/gltfTutorial_005_BuffersBufferViewsAccessors.md#data-interleaving
                //       (4): Be aware of the base color factor: https://github.com/KhronosGroup/glTF-Tutorials/blob/main/gltfTutorial/gltfTutorial_011_SimpleMaterial.md#material-definition
                // This example only supports gltf that only has one mesh and one skin.
                assert(model.meshes.size() == 1, "This SharedLib Gltf Loader currently only supports one mesh.");
                assert(model.skins.size() == 0, "This SharedLib Gltf Loader currently doesn't support the skinning."); // TODO: Support skinning and animation.

                // Load mesh and relevant info
                // TODO: We should support multiple meshes in the future.
                const auto& mesh = model.meshes[0];
                MeshEntity* pMeshEntity = new MeshEntity();
                m_entities.push_back(pMeshEntity);

                // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#reference-mesh-primitive
                // Meshes are defined as arrays of primitives. Primitives correspond to the data required for GPU draw calls.
                // Primitives specify one or more attributes, corresponding to the vertex attributes used in the draw calls.
                // The specification defines the following attribute semantics: POSITION, NORMAL, TANGENT, TEXCOORD_n, COLOR_n, JOINTS_n, and WEIGHTS_n.
                
                for (uint32_t i = 0; i < mesh.primitives.size(); i++)
                {
                    const auto& primitive = mesh.primitives[i];
                    MeshPrimitive meshPrimitive;
                    
                    // Load pos
                    int posIdx = mesh.primitives[0].attributes.at("POSITION");
                    const auto& posAccessor = model.accessors[posIdx];

                    assert(posAccessor.componentType == TINYGLTF_PARAMETER_TYPE_FLOAT, "The pos accessor data type should be float.");
                    assert(posAccessor.type          == TINYGLTF_TYPE_VEC3, "The pos accessor type should be vec3.");

                    const auto& posBufferView = model.bufferViews[posAccessor.bufferView];
                    // Assmue the data and element type of the position is float3
                    meshPrimitive.m_posData.resize(3 * posAccessor.count);
                    SharedLib::ReadOutAccessorData(meshPrimitive.m_posData.data(), posAccessor, model.bufferViews, model.buffers);

                    // Load indices
                    int indicesIdx = mesh.primitives[0].indices;
                    const auto& idxAccessor = model.accessors[indicesIdx];

                    assert(idxAccessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT, "The idx accessor data type should be uint16.");
                    assert(idxAccessor.type == TINYGLTF_TYPE_SCALAR, "The idx accessor type should be scalar.");

                    meshPrimitive.m_idxDataUint16.resize(idxAccessor.count);
                    SharedLib::ReadOutAccessorData(meshPrimitive.m_idxDataUint16.data(), idxAccessor, model.bufferViews, model.buffers);

                    // Load normal
                    int normalIdx = -1;
                    if (mesh.primitives[0].attributes.count("NORMAL") > 0)
                    {
                        normalIdx = mesh.primitives[0].attributes.at("NORMAL");
                        const auto& normalAccessor = model.accessors[normalIdx];

                        assert(normalAccessor.componentType == TINYGLTF_PARAMETER_TYPE_FLOAT, "The normal accessor data type should be float.");
                        assert(normalAccessor.type == TINYGLTF_TYPE_VEC3, "The normal accessor type should be vec3.");

                        meshPrimitive.m_normalData.resize(3 * normalAccessor.count);
                        SharedLib::ReadOutAccessorData(meshPrimitive.m_normalData.data(), normalAccessor, model.bufferViews, model.buffers);
                    }
                    else
                    {
                        // If we don't have any normal geo data, then we will just apply the first triangle's normal to all the other
                        // triangles/vertices.
                        uint16_t idx0 = meshPrimitive.m_idxDataUint16[0];
                        float vertPos0[3] = { meshPrimitive.m_posData[3 * idx0], meshPrimitive.m_posData[3 * idx0 + 1], meshPrimitive.m_posData[3 * idx0 + 2] };

                        uint16_t idx1 = meshPrimitive.m_idxDataUint16[1];
                        float vertPos1[3] = { meshPrimitive.m_posData[3 * idx1], meshPrimitive.m_posData[3 * idx1 + 1], meshPrimitive.m_posData[3 * idx1 + 2] };

                        uint16_t idx2 = meshPrimitive.m_idxDataUint16[2];
                        float vertPos2[3] = { meshPrimitive.m_posData[3 * idx2], meshPrimitive.m_posData[3 * idx2 + 1], meshPrimitive.m_posData[3 * idx2 + 2] };

                        float v1[3] = { vertPos1[0] - vertPos0[0], vertPos1[1] - vertPos0[1], vertPos1[2] - vertPos0[2] };
                        float v2[3] = { vertPos2[0] - vertPos0[0], vertPos2[1] - vertPos0[1], vertPos2[2] - vertPos0[2] };

                        float autoGenNormal[3] = { 0.f };
                        SharedLib::CrossProductVec3(v1, v2, autoGenNormal);
                        SharedLib::NormalizeVec(autoGenNormal, 3);

                        meshPrimitive.m_normalData.resize(3 * posAccessor.count);
                        for (uint32_t i = 0; i < posAccessor.count; i++)
                        {
                            uint32_t normalStartingIdx = i * 3;
                            meshPrimitive.m_normalData[normalStartingIdx] = autoGenNormal[0];
                            meshPrimitive.m_normalData[normalStartingIdx + 1] = autoGenNormal[1];
                            meshPrimitive.m_normalData[normalStartingIdx + 2] = autoGenNormal[2];
                        }
                    }
                }
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