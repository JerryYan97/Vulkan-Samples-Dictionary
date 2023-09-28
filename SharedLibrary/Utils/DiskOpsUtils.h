#pragma once
#include <string>
#include <vector>

namespace SharedLib
{
    float* ReadImg(const std::string& namePath, int& components, int& width, int& height);
    void SaveImg(const std::string& namePath, uint32_t width, uint32_t height, uint32_t components, float* pData);
    void ReadBinaryFile(const std::string& namePath, std::vector<char>& oData);

    // TODO: An interface to read obj/gltf.
    static void ReadModel() {};
}