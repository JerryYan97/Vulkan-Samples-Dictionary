import os
import argparse
import shutil
import sys
import subprocess
from pathlib import Path


# Duplicate the HLSLCompile.py
def ShaderName(srcFileNamePath):
    hlslPostFixIdx = srcFileNamePath.find('.hlsl')
    if hlslPostFixIdx == -1:
        sys.exit('The src does not have .hlsl post fix.')

    # Find '/' or '\\'
    divSignIdx = srcFileNamePath.rfind('/')
    if divSignIdx == -1:
        divSignIdx = srcFileNamePath.rfind('\\')
        if divSignIdx == -1:
            sys.exit('The src does not have directory div sign.')
    
    return srcFileNamePath[divSignIdx + 1 : hlslPostFixIdx]


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


def GenerateHeader(shaderPath, shaderName):
    generateHeaderHandle = open(shaderPath + "\\" + "g_" + shaderName + ".h", "w")
    generateHeaderHandle.write(GeneratePreShaderArrayStr())

    with open(shaderPath + "\\" + shaderName + ".spv", mode='rb') as file: # b is important -> binary
        fileContent = file.read()
        hexStr = fileContent.hex()
        arrayStr = GenerateShaderFormatedArray(hexStr, shaderName + "Script")
        generateHeaderHandle.write(arrayStr)
        generateHeaderHandle.write("\n")        
    
    generateHeaderHandle.write("}")
    generateHeaderHandle.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Compile the target .hlsl shaders')
    parser.add_argument('--src', type=str, help='path to the target hlsl shader', default="", required=True)
    parser.add_argument('--dstDir', type=str, help='path to the output header folder', default="", required=True)
    args = parser.parse_args()

    curScriptDir = os.path.realpath(os.path.dirname(__file__))
    compileHlslScriptPathName = curScriptDir + "/../SharedLibrary/HLSL/HLSLCompile.py"

    subprocess.check_output([
        'python',
        compileHlslScriptPathName,
        '--src', args.src,
        '--dstDir', args.dstDir
    ])

    GenerateHeader(args.dstDir, ShaderName(args.src))
