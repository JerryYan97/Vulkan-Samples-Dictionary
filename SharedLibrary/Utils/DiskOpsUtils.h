#pragma once
#include <string>

namespace SharedLib
{
    float* ReadImg(const std::string& namePath, int& components, int& width, int& height);
    void SaveImg(const std::string& namePath, uint32_t width, uint32_t height, uint32_t components, float* pData);
}