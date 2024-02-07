#pragma once
#include <cstdint>
#include "tiny_gltf.h"

namespace SharedLib
{
    uint32_t GetAccessorDataBytes(const tinygltf::Accessor& accessor);

    void ReadOutAccessorData(void*                              pDst,
                             const tinygltf::Accessor&          accessor,
                             std::vector<tinygltf::BufferView>& bufferViews,
                             std::vector<tinygltf::Buffer>&     buffers);
}
