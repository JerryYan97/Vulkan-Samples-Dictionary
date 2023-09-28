#include "DiskOpsUtils.h"
#include <iostream>
#include <fstream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STBI_MSC_SECURE_CRT
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace SharedLib
{
    // ================================================================================================================
    float* ReadImg(
        const std::string& namePath,
        int& components,
        int& width,
        int& height)
    {
        return stbi_loadf(namePath.c_str(), &width, &height, &components, 0);
    }

    // ================================================================================================================
    void SaveImg(
        const std::string& namePath,
        uint32_t width,
        uint32_t height,
        uint32_t components,
        float* pData)
    {
        int res = stbi_write_hdr(namePath.c_str(), width, height, components, pData);
        if (res > 0)
        {
            std::cout << namePath << ": saves successfully." << std::endl;
        }
        else
        {
            std::cout << "Img fails to save." << std::endl;
        }
    }

    // ================================================================================================================
    void ReadBinaryFile(
        const std::string& namePath,
        std::vector<char>& oData)
    {
        std::ifstream ifd(namePath, std::ios::binary | std::ios::ate);

        // Set the file read pointer to the end of the file to 
        int size = ifd.tellg();

        // Set the file read pointer back to the start
        ifd.seekg(0, std::ios::beg);
        
        oData.resize(size); // << resize not reserve
        ifd.read(oData.data(), size);
    }
}