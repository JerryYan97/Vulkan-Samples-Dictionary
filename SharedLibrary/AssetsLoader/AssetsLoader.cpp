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

                    // Load uv
                    int uvIdx = -1;
                    if (mesh.primitives[0].attributes.count("TEXCOORD_0") > 0)
                    {
                        uvIdx = mesh.primitives[0].attributes.at("TEXCOORD_0");
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

                    // Load the base color texture or create a default pure color texture.
                    // The baseColorFactor contains the red, green, blue, and alpha components of the main color of the material.
                    int materialIdx = mesh.primitives[0].material;

                    if (materialIdx != -1)
                    {
                        const auto& material = model.materials[materialIdx];
                        int baseColorTexIdx = material.pbrMetallicRoughness.baseColorTexture.index;
                        // A texture binding is defined by an index of a texture object and an optional index of texture coordinates.
                        // Its green channel contains roughness values and its blue channel contains metalness values.
                        int baseColorTexIdx = material.pbrMetallicRoughness.baseColorTexture.index;
                        int metallicRoughnessTexIdx = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
                        int occlusionTexIdx = material.occlusionTexture.index;
                        int normalTexIdx = material.normalTexture.index;
                        // material.emissiveTexture -- Let forget emissive. The renderer doesn't support emissive textures.

                        if (baseColorTexIdx == -1)
                        {
                            // A pure color model.
                            float pureColor[3] = { material.pbrMetallicRoughness.baseColorFactor[0],
                                                   material.pbrMetallicRoughness.baseColorFactor[1],
                                                   material.pbrMetallicRoughness.baseColorFactor[2] };

                            m_skeletalMesh.mesh.baseColorImg = CreateDummyPureColorImg(pureColor);
                        }
                        else
                        {
                            // A texture is defined by an image index, denoted by the source property and a sampler index (sampler).
                            // Assmue that all textures are 8 bits per channel. They are all xxx / 255. They all have 4 components.
                            const auto& baseColorTex = model.textures[baseColorTexIdx];
                            int baseColorTexImgIdx = baseColorTex.source;

                            // This model has a base color texture.
                            const auto& baseColorImg = model.images[baseColorTexImgIdx];

                            VkImageSubresourceRange tex2dSubResRange{};
                            {
                                tex2dSubResRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                                tex2dSubResRange.baseMipLevel = 0;
                                tex2dSubResRange.levelCount = 1;
                                tex2dSubResRange.baseArrayLayer = 0;
                                tex2dSubResRange.layerCount = 1;
                            }

                            VkSamplerCreateInfo samplerInfo{};
                            {
                                samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
                                samplerInfo.magFilter = VK_FILTER_LINEAR;
                                samplerInfo.minFilter = VK_FILTER_LINEAR;
                                samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
                                samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                                samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                                samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                                samplerInfo.minLod = -1000;
                                samplerInfo.maxLod = 1000;
                                samplerInfo.maxAnisotropy = 1.0f;
                            }

                            SharedLib::GpuImgCreateInfo gpuImgCreateInfo{};
                            {
                                gpuImgCreateInfo.allocFlags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
                                gpuImgCreateInfo.hasSampler = true;
                                gpuImgCreateInfo.imgSubresRange = tex2dSubResRange;
                                gpuImgCreateInfo.imgUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
                                gpuImgCreateInfo.imgViewType = VK_IMAGE_VIEW_TYPE_2D;
                                gpuImgCreateInfo.samplerInfo = samplerInfo;
                                gpuImgCreateInfo.imgExtent = VkExtent3D{ (uint32_t)baseColorImg.width, (uint32_t)baseColorImg.height, 1 };
                                gpuImgCreateInfo.imgFormat = VK_FORMAT_R8G8B8A8_SRGB;
                            }

                            m_skeletalMesh.mesh.baseColorImg = CreateGpuImage(gpuImgCreateInfo);

                            VkBufferImageCopy baseColorBufToImgCopy{};
                            {
                                VkExtent3D extent{};
                                {
                                    extent.width = baseColorImg.width;
                                    extent.height = baseColorImg.height;
                                    extent.depth = 1;
                                }

                                baseColorBufToImgCopy.bufferRowLength = extent.width;
                                baseColorBufToImgCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                                baseColorBufToImgCopy.imageSubresource.mipLevel = 0;
                                baseColorBufToImgCopy.imageSubresource.baseArrayLayer = 0;
                                baseColorBufToImgCopy.imageSubresource.layerCount = 1;
                                baseColorBufToImgCopy.imageExtent = extent;
                            }


                            SharedLib::RAIICommandBuffer raiiCmdBuffer(m_gfxCmdPool, m_device);

                            SharedLib::SendImgDataToGpu(raiiCmdBuffer.m_cmdBuffer,
                                m_device,
                                m_graphicsQueue,
                                (void*)baseColorImg.image.data(),
                                baseColorImg.image.size() * sizeof(unsigned char),
                                m_skeletalMesh.mesh.baseColorImg.image,
                                tex2dSubResRange,
                                VK_IMAGE_LAYOUT_UNDEFINED,
                                baseColorBufToImgCopy,
                                *m_pAllocator);
                        }
                    }
                    else
                    {
                        float white[3] = { 1.f, 1.f, 1.f };
                        m_skeletalMesh.mesh.baseColorImg = CreateDummyPureColorImg(white);
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