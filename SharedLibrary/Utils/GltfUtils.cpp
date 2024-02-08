#include "GltfUtils.h"
#include "MathUtils.h"
#include <vector>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/ext/quaternion_common.hpp>
#include <glm/ext/quaternion_float.hpp>

namespace SharedLib
{
    // ================================================================================================================
    uint32_t GetAComponentEleBytesCnt(
        int componentType)
    {
        uint32_t aComponentEleBytesCnt = 0;
        switch (componentType)
        {
        case TINYGLTF_COMPONENT_TYPE_DOUBLE:
            aComponentEleBytesCnt = sizeof(double);
            break;
        case TINYGLTF_COMPONENT_TYPE_FLOAT:
            aComponentEleBytesCnt = sizeof(float);
            break;
        case TINYGLTF_COMPONENT_TYPE_INT:
            aComponentEleBytesCnt = sizeof(int);
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            aComponentEleBytesCnt = sizeof(unsigned short);
            break;
        case TINYGLTF_COMPONENT_TYPE_BYTE:
            aComponentEleBytesCnt = 1;
            break;
        default:
            assert(false, "Invalid component type.");
        }
        return aComponentEleBytesCnt;
    }

    // ================================================================================================================
    uint32_t GetComponentEleCnt(
        int accessorType)
    {
        uint32_t componentEleCnt = 0;
        switch (accessorType)
        {
        case TINYGLTF_TYPE_SCALAR:
            componentEleCnt = 1;
            break;
        case TINYGLTF_TYPE_VEC2:
            componentEleCnt = 2;
            break;
        case TINYGLTF_TYPE_VEC3:
            componentEleCnt = 3;
            break;
        case TINYGLTF_TYPE_VEC4:
            componentEleCnt = 4;
            break;
        case TINYGLTF_TYPE_MAT4:
            componentEleCnt = 16;
            break;
        default:
            assert(false, "Invalid accessor type.");
        }

        return componentEleCnt;
    }

    // ================================================================================================================
    uint32_t GetAccessorDataBytes(
        const tinygltf::Accessor& accessor)
    {
        uint32_t aComponentEleBytesCnt = GetAComponentEleBytesCnt(accessor.componentType);
        uint32_t componentEleCnt = GetComponentEleCnt(accessor.type);
        uint32_t componentCnt = accessor.count;

        return componentCnt * componentEleCnt * aComponentEleBytesCnt;
    }

    // ================================================================================================================
    void ReadOutAccessorData(
        void*                              pDst,
        const tinygltf::Accessor&          accessor,
        std::vector<tinygltf::BufferView>& bufferViews,
        std::vector<tinygltf::Buffer>&     buffers)
    {
        auto&    bufferView = bufferViews[accessor.bufferView];
        auto&    buffer     = buffers[bufferView.buffer];
        uint32_t bytesCnt   = GetAccessorDataBytes(accessor);

        int accessorByteOffset = accessor.byteOffset;
        int componentCnt       = accessor.count;

        uint32_t aComponentEleBytesCnt = GetAComponentEleBytesCnt(accessor.componentType);
        uint32_t componentEleCnt       = GetComponentEleCnt(accessor.type);
        uint32_t componentBytesCnt     = aComponentEleBytesCnt * componentEleCnt;

        int bufferOffset = accessorByteOffset + bufferView.byteOffset;
        
        const unsigned char* pBufferData = buffers[bufferView.buffer].data.data();

        if (bufferView.byteStride == 0)
        {
            memcpy(pDst, &pBufferData[bufferOffset], bytesCnt);
        }
        else
        {
            std::vector<uint8_t> tmpData(bytesCnt, 0);
            for(uint32_t i = 0; i < componentCnt; i++)
            {
                int srcByteOffset = bufferOffset + i * bufferView.byteStride;
                int dstByteOffset = i * componentBytesCnt;
                memcpy(&tmpData[dstByteOffset], pBufferData + srcByteOffset, componentBytesCnt);
            }
            memcpy(pDst, tmpData.data(), tmpData.size());
        }
    }

    // ================================================================================================================
    // TODO: We maybe able to abstract the local transformation code out so that we can reuse the code.
    void GetNodeAndChildrenModelMats(const std::vector<tinygltf::Node>& nodes,
                                     float                              parentChainedTransformationMat[16],
                                     uint32_t                           curNodeIdx,
                                     std::vector<float>&                matsVec)
    {
        const tinygltf::Node& node = nodes[curNodeIdx];
        int matStartOffset = curNodeIdx * 16;
        float localTransformMat[16] = {};

        if (node.matrix.size() != 0)
        {
            // This node's local transformation is represented by a matrix.
            // The TinyGltf Mat's ele are double, but we want float, so we cannot directly memcpy.
            for (int eleIdx = 0; eleIdx < 16; eleIdx++)
            {
                localTransformMat[eleIdx] = node.matrix[eleIdx];
            }

            SharedLib::MatTranspose(localTransformMat, 4);
        }
        else
        {
            // This node's local transformation is represented by TRS params.
            // Set joint translation from the gltf
            float localTranslation[3];
            if (node.translation.size() != 0)
            {
                localTranslation[0] = node.translation[0];
                localTranslation[1] = node.translation[1];
                localTranslation[2] = node.translation[2];
            }
            else
            {
                localTranslation[0] = 0.f;
                localTranslation[1] = 0.f;
                localTranslation[2] = 0.f;
            }

            // Set joint rotation from the gltf
            // GLM's quat's scalar element is the first element but the GLTF's quat scalar element is the last element.
            // localRotationQuat is a GLM's quat. node.rotation is a GLTF's quat.
            glm::quat localRotationQuat;
            if (node.rotation.size() != 0)
            {
                localRotationQuat = glm::quat(node.rotation[3],
                                              node.rotation[0],
                                              node.rotation[1],
                                              node.rotation[2]);
            }
            else
            {
                localRotationQuat = glm::quat(1.f, 0.f, 0.f, 0.f);
            }

            // Set joint scaling from the gltf
            float localScale[3];
            if (node.scale.size() != 0)
            {
                localScale[0] = node.scale[0];
                localScale[1] = node.scale[1];
                localScale[2] = node.scale[2];
            }
            else
            {
                localScale[0] = 1.f;
                localScale[1] = 1.f;
                localScale[2] = 1.f;
            }

            glm::mat4 rotationMatrix = glm::toMat4(localRotationQuat);
            glm::vec4 rotRow0 = glm::row(rotationMatrix, 0);
            glm::vec4 rotRow1 = glm::row(rotationMatrix, 1);
            glm::vec4 rotRow2 = glm::row(rotationMatrix, 2);

            float localTranslationMat[16] = {
                1.f, 0.f, 0.f, localTranslation[0],
                0.f, 1.f, 0.f, localTranslation[1],
                0.f, 0.f, 1.f, localTranslation[2],
                0.f, 0.f, 0.f, 1.f
            };

            float localRotationMat[16] = {
                rotRow0[0], rotRow0[1], rotRow0[2], 0.f,
                rotRow1[0], rotRow1[1], rotRow1[2], 0.f,
                rotRow2[0], rotRow2[1], rotRow2[2], 0.f,
                0.f,        0.f,        0.f,        1.f
            };

            float localScaleMat[16] = {
                localScale[0], 0.f,                  0.f,                  0.f,
                0.f,           localScale[1],        0.f,                  0.f,
                0.f,           0.f,                  localScale[2],        0.f,
                0.f,           0.f,                  0.f,                  1.f
            };

            float localRSMat[16] = {};
            MatrixMul4x4(localRotationMat, localScaleMat, localRSMat);
            MatrixMul4x4(localTranslationMat, localRSMat, localTransformMat);
        }

        float nodeModelMat[16];
        MatrixMul4x4(parentChainedTransformationMat, localTransformMat, nodeModelMat);
        memcpy(&matsVec[matStartOffset], nodeModelMat, sizeof(nodeModelMat));

        for (int childNodeId : node.children)
        {
            GetNodeAndChildrenModelMats(nodes, nodeModelMat, childNodeId, matsVec);
        }
    }

    // ================================================================================================================
    void GetNodesModelMats(const tinygltf::Model& model,
                           std::vector<float>&    matsVec)
    {
        matsVec.resize(model.nodes.size() * 16);
        for (auto rootNodeId : model.scenes[0].nodes)
        {
            float identityMat[16] = {
                1.f, 0.f, 0.f, 0.f,
                0.f, 1.f, 0.f, 0.f,
                0.f, 0.f, 1.f, 0.f,
                0.f, 0.f, 0.f, 1.f
            };

            GetNodeAndChildrenModelMats(model.nodes, identityMat, rootNodeId, matsVec);
        }
    }

    // ================================================================================================================
    int GetArmatureNodeIdx(const tinygltf::Model& model)
    {
        for (int i = 0; i < model.nodes.size(); i++)
        {
            for (auto childIdx : model.nodes[i].children)
            {
                if (childIdx == model.skins[0].joints[0])
                {
                    return i;
                }
            }
        }
        return -1;
    }
}