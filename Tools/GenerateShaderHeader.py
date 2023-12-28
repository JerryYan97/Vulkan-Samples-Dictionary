import os
import argparse
import shutil
import sys
import subprocess
from pathlib import Path

def GenerateShaderFormatedArray(hexStr, arrayName):
    shaderArrayStr = "    constexpr uint8_t " + arrayName + "[] = {\n"
    for idx in range(len(hexStr) // 2):
        eleStr = ""
        if idx % 16 == 0:
            eleStr += "       "
        eleStr += " 0x" + hexStr[2*idx : 2*idx + 2] + ","
        if idx % 16 == 15:
            eleStr += "\n"
        shaderArrayStr += eleStr
    shaderArrayStr = shaderArrayStr[:-1]
    shaderArrayStr += "};\n"
    return shaderArrayStr


def GeneratePreShaderArrayStr():
    preShadersStr = "// ATTENTION: This file is generated from HLSL shaders and the GenerateShaderHeader.py. Don't edit it manually!\n"
    preShadersStr += "#pragma once\n\n"
    preShadersStr += "namespace SharedLib\n"
    preShadersStr += "{\n"
    return preShadersStr


def GenerateHeader(shaderFoldersPathsNameList, shadersPath):
    generateHeaderHandle = open(shadersPath + "\\" + "g_prebuiltShaders.h", "w")

    generateHeaderHandle.write(GeneratePreShaderArrayStr())

    for shaderFolderPathName in shaderFoldersPathsNameList:
        fileGenerator = os.walk(shaderFolderPathName)
        filenames = next(fileGenerator)
        for fileName in filenames[2]:
            if ".spv" in fileName:
                with open(shaderFolderPathName + "\\" + fileName, mode='rb') as file: # b is important -> binary
                    fileContent = file.read()
                    hexStr = fileContent.hex()
                    arrayStr = GenerateShaderFormatedArray(hexStr, fileName.rsplit(".")[0] + "Script")
                    generateHeaderHandle.write(arrayStr)
                    generateHeaderHandle.write("\n")
    
    generateHeaderHandle.write("}")
    generateHeaderHandle.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Compile the target .hlsl shaders')
    parser.add_argument('--src', type=str, help='path to the target hlsl shader', default="")
    parser.add_argument('--dstDir', type=str, help='path to the output spv folder', default="")
    args = parser.parse_args()