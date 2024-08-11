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
        
    }

    // ================================================================================================================
    void AssetsLoaderManager::InitEntitesGpuRsrc(VkDevice      device,
                                                 VmaAllocator* pAllocator)
    {
        for(auto entity : m_entities)
        {
            entity->InitGpuRsrc(device, pAllocator);
        }
    }

    // ================================================================================================================
    void AssetsLoaderManager::FinializeEntities(VkDevice      device,
                                                VmaAllocator* pAllocator)
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
                // Any node MAY contain one mesh, defined in its mesh property. The mesh MAY be skinned using information provided in a referenced skin object.
                // TODO: We should support multiple meshes in the future.
                const auto& mesh = model.meshes[0];
                MeshEntity* pMeshEntity = new MeshEntity();
                pMeshEntity->m_meshPrimitives.resize(mesh.primitives.size());
                m_entities.push_back(pMeshEntity);
                oLevel.AddMshEntity("MeshEntity", pMeshEntity);

                // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#reference-mesh-primitive
                // Meshes are defined as arrays of primitives. Primitives correspond to the data required for GPU draw calls.
                // Primitives specify one or more attributes, corresponding to the vertex attributes used in the draw calls.
                // The specification defines the following attribute semantics: POSITION, NORMAL, TANGENT, TEXCOORD_n, COLOR_n, JOINTS_n, and WEIGHTS_n.
                
                for (uint32_t i = 0; i < mesh.primitives.size(); i++)
                {
                    const auto& primitive = mesh.primitives[i];
                    MeshPrimitive& meshPrimitive = pMeshEntity->m_meshPrimitives[i];
                    
                    // Load pos
                    int posIdx = mesh.primitives[i].attributes.at("POSITION");
                    const auto& posAccessor = model.accessors[posIdx];

                    assert(posAccessor.componentType == TINYGLTF_PARAMETER_TYPE_FLOAT, "The pos accessor data type should be float.");
                    assert(posAccessor.type          == TINYGLTF_TYPE_VEC3, "The pos accessor type should be vec3.");

                    const auto& posBufferView = model.bufferViews[posAccessor.bufferView];
                    // Assmue the data and element type of the position is float3
                    meshPrimitive.m_posData.resize(3 * posAccessor.count);
                    SharedLib::ReadOutAccessorData(meshPrimitive.m_posData.data(), posAccessor, model.bufferViews, model.buffers);

                    // Load indices
                    int indicesIdx = mesh.primitives[i].indices;
                    const auto& idxAccessor = model.accessors[indicesIdx];

                    assert(idxAccessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT, "The idx accessor data type should be uint16.");
                    assert(idxAccessor.type == TINYGLTF_TYPE_SCALAR, "The idx accessor type should be scalar.");

                    meshPrimitive.m_idxDataUint16.resize(idxAccessor.count);
                    SharedLib::ReadOutAccessorData(meshPrimitive.m_idxDataUint16.data(), idxAccessor, model.bufferViews, model.buffers);

                    // Load normal
                    int normalIdx = -1;
                    if (mesh.primitives[i].attributes.count("NORMAL") > 0)
                    {
                        normalIdx = mesh.primitives[i].attributes.at("NORMAL");
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

                    // Load uv
                    int uvIdx = -1;
                    if (mesh.primitives[i].attributes.count("TEXCOORD_0") > 0)
                    {
                        uvIdx = mesh.primitives[i].attributes.at("TEXCOORD_0");
                        const auto& uvAccessor = model.accessors[uvIdx];

                        assert(uvAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, "The uv accessor data type should be float.");
                        assert(uvAccessor.type == TINYGLTF_TYPE_VEC2, "The uv accessor type should be vec2.");

                        meshPrimitive.m_texCoordData.resize(2 * uvAccessor.count);
                        SharedLib::ReadOutAccessorData(meshPrimitive.m_texCoordData.data(), uvAccessor, model.bufferViews, model.buffers);
                    }
                    else
                    {
                        assert(false, "The loaded mesh doesn't have uv data.");
                        meshPrimitive.m_texCoordData = std::vector<float>(posAccessor.count * 2, 0.f);
                    }

                    // Load tangent
                    int tangentIdx = -1;
                    if (mesh.primitives[i].attributes.count("TANGENT"))
                    {
                        tangentIdx = mesh.primitives[i].attributes.at("TANGENT");
                        const auto& tangentAccessor = model.accessors[tangentIdx];

                        assert(tangentAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, "The tangent accessor data type should be float.");
                        assert(tangentAccessor.type == TINYGLTF_TYPE_VEC4, "The tangent accessor type should be vec4.");
                        assert(tangentAccessor.count == posAccessor.count, "The tangent data count should be the same as the pos data count.");

                        meshPrimitive.m_tangentData.resize(4 * tangentAccessor.count);
                        SharedLib::ReadOutAccessorData(meshPrimitive.m_tangentData.data(), tangentAccessor, model.bufferViews, model.buffers);
                    }
                    else
                    {
                        // assert(false, "The loaded mesh doesn't have tangent data.");
                        meshPrimitive.m_tangentData = std::vector<float>(posAccessor.count * 4, 0.f);
                    }

                    // Load the base color texture or create a default pure color texture.
                    // The baseColorFactor contains the red, green, blue, and alpha components of the main color of the material.
                    int materialIdx = mesh.primitives[i].material;

                    // All the textures are 4 components R8G8B8A8 textures.
                    auto& pfnSetDefaultBaseColor = [](MeshPrimitive& meshPrimitive) {
                        meshPrimitive.m_baseColorTex.pixHeight = 1;
                        meshPrimitive.m_baseColorTex.pixWidth = 1;
                        meshPrimitive.m_baseColorTex.componentCnt = 4;
                        meshPrimitive.m_baseColorTex.dataVec = std::vector<uint8_t>(4, 255);
                        };

                    auto& pfnSetDefaultMetallicRoughness = [](MeshPrimitive& meshPrimitive) {
                        float defaultMetallicRoughness[4] = { 0.f, 1.f, 0.f, 0.f };
                        meshPrimitive.m_metallicRoughnessTex.pixHeight = 1;
                        meshPrimitive.m_metallicRoughnessTex.pixWidth = 1;
                        meshPrimitive.m_metallicRoughnessTex.componentCnt = 4;
                        meshPrimitive.m_metallicRoughnessTex.dataVec = std::vector<uint8_t>(sizeof(defaultMetallicRoughness), 0);
                        memcpy(meshPrimitive.m_metallicRoughnessTex.dataVec.data(), defaultMetallicRoughness, sizeof(defaultMetallicRoughness));
                        };

                    auto& pfnSetDefaultOcclusion = [](MeshPrimitive& meshPrimitive) {
                        float defaultOcclusion[4] = { 1.f, 0.f, 0.f, 0.f };
                        meshPrimitive.m_occlusionTex.pixHeight = 1;
                        meshPrimitive.m_occlusionTex.pixWidth = 1;
                        meshPrimitive.m_occlusionTex.componentCnt = 4;
                        meshPrimitive.m_occlusionTex.dataVec = std::vector<uint8_t>(sizeof(defaultOcclusion), 0);
                        memcpy(meshPrimitive.m_occlusionTex.dataVec.data(), &defaultOcclusion, sizeof(defaultOcclusion));
                        };

                    auto& pfnSetDefaultNormal = [](MeshPrimitive& meshPrimitive) {
                        float defaultNormal[3] = { 0.f, 0.f, 1.f };
                        meshPrimitive.m_normalTex.pixHeight = 1;
                        meshPrimitive.m_normalTex.pixWidth = 1;
                        meshPrimitive.m_normalTex.componentCnt = 3;
                        meshPrimitive.m_normalTex.dataVec = std::vector<uint8_t>(sizeof(defaultNormal), 0);
                        memcpy(meshPrimitive.m_normalTex.dataVec.data(), defaultNormal, sizeof(defaultNormal));
                        };


                    if (materialIdx != -1)
                    {
                        const auto& material = model.materials[materialIdx];
                        // A texture binding is defined by an index of a texture object and an optional index of texture coordinates.
                        // Its green channel contains roughness values and its blue channel contains metalness values.
                        int baseColorTexIdx = material.pbrMetallicRoughness.baseColorTexture.index;
                        int metallicRoughnessTexIdx = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
                        int occlusionTexIdx = material.occlusionTexture.index;
                        int normalTexIdx = material.normalTexture.index;
                        // material.emissiveTexture -- Let forget emissive. The renderer doesn't support emissive textures.

                        if (baseColorTexIdx == -1)
                        {
                            pfnSetDefaultBaseColor(meshPrimitive);
                        }
                        else
                        {
                            // A texture is defined by an image index, denoted by the source property and a sampler index (sampler).
                            // Assmue that all textures are 8 bits per channel. They are all xxx / 255. They all have 4 components.
                            const auto& baseColorTex = model.textures[baseColorTexIdx];
                            int baseColorTexImgIdx = baseColorTex.source;

                            // This model has a base color texture.
                            const auto& baseColorImg = model.images[baseColorTexImgIdx];

                            meshPrimitive.m_baseColorTex.pixWidth     = baseColorImg.width;
                            meshPrimitive.m_baseColorTex.pixHeight    = baseColorImg.height;
                            meshPrimitive.m_baseColorTex.componentCnt = baseColorImg.component;
                            meshPrimitive.m_baseColorTex.dataVec      = baseColorImg.image;

                            assert(baseColorImg.component == 4, "All textures should have 4 components.");
                            assert(baseColorImg.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE, "All textures' each component should be a byte.");
                        }

                        // The textures for metalness and roughness properties are packed together in a single texture called metallicRoughnessTexture.Its green
                        // channel contains roughness values and its blue channel contains metalness values.This texture MUST be encoded with linear transfer function
                        // and MAY use more than 8 bits per channel.
                        if (metallicRoughnessTexIdx == -1)
                        {
                            pfnSetDefaultMetallicRoughness(meshPrimitive);
                        }
                        else
                        {
                            const auto& metallicRoughnessTex = model.textures[metallicRoughnessTexIdx];
                            int metallicRoughnessTexImgIdx = metallicRoughnessTex.source;

                            const auto& metallicRoughnessImg = model.images[metallicRoughnessTexImgIdx];

                            meshPrimitive.m_metallicRoughnessTex.pixWidth = metallicRoughnessImg.width;
                            meshPrimitive.m_metallicRoughnessTex.pixHeight = metallicRoughnessImg.height;
                            meshPrimitive.m_metallicRoughnessTex.componentCnt = metallicRoughnessImg.component;
                            meshPrimitive.m_metallicRoughnessTex.dataVec = metallicRoughnessImg.image;

                            assert(metallicRoughnessImg.component == 4, "All textures should have 4 components.");
                            assert(metallicRoughnessImg.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE, "All textures' each component should be a byte.");
                        }

                        if (normalTexIdx == -1)
                        {
                            pfnSetDefaultNormal(meshPrimitive);
                        }
                        else
                        {
                            const auto& normalTex = model.textures[normalTexIdx];
                            int normalTexImgIdx = normalTex.source;

                            const auto& normalImg = model.images[normalTexImgIdx];

                            meshPrimitive.m_normalTex.pixWidth = normalImg.width;
                            meshPrimitive.m_normalTex.pixHeight = normalImg.height;
                            meshPrimitive.m_normalTex.componentCnt = normalImg.component;
                            meshPrimitive.m_normalTex.dataVec = normalImg.image;

                            assert(normalImg.component == 4, "All textures should have 4 components.");
                            assert(normalImg.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE, "All textures' each component should be a byte.");
                        }

                        // The occlusion texture; it indicates areas that receive less indirect lighting from ambient sources.
                        // Direct lighting is not affected.The red channel of the texture encodes the occlusion value,
                        // where 0.0 means fully - occluded area(no indirect lighting) and 1.0 means not occluded area(full indirect lighting).
                        if (occlusionTexIdx == -1)
                        {
                            pfnSetDefaultOcclusion(meshPrimitive);
                        }
                        else
                        {
                            const auto& occlusionTex = model.textures[occlusionTexIdx];
                            int occlusionTexImgIdx = occlusionTex.source;

                            const auto& occlusionImg = model.images[occlusionTexImgIdx];

                            meshPrimitive.m_occlusionTex.pixWidth = occlusionImg.width;
                            meshPrimitive.m_occlusionTex.pixHeight = occlusionImg.height;
                            meshPrimitive.m_occlusionTex.componentCnt = occlusionImg.component;
                            meshPrimitive.m_occlusionTex.dataVec = occlusionImg.image;

                            assert(occlusionImg.component == 4, "All textures should have 4 components.");
                            assert(occlusionImg.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE, "All textures' each component should be a byte.");
                        }
                    }
                    else
                    {
                        // No material, then we will create a pure white model.
                        pfnSetDefaultBaseColor(meshPrimitive);
                        pfnSetDefaultMetallicRoughness(meshPrimitive);
                        pfnSetDefaultOcclusion(meshPrimitive);
                        pfnSetDefaultNormal(meshPrimitive);
                    }
                }
            }
            else if(strcmp(filePostfix.c_str(), "glb") == 0)
            {
                //bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, argv[1]); // for binary glTF(.glb)
                ASSERT(false, "Currently don't support the glb file format.");
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