#include "GltfUtils.h"
#include <vector>

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
}