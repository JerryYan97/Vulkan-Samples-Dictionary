import os
import argparse
import shutil
import sys
import subprocess
from pathlib import Path


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
    
    return srcFileNamePath[divSignIdx : hlslPostFixIdx]


def SelectProfile(srcFileName):
    if(srcFileName.find('_vert') != -1):
        return 'vs_6_1'
    elif(srcFileName.find('_frag') != -1):
        return 'ps_6_1'
    else:
        sys.exit('Unrecogonized shader type.')


def SelectDxc():
    pathEnvStr = os.environ['PATH']
    pathEnvStrList = pathEnvStr.rsplit(';')
    foundVulkanSDK = False
    dxcCmdStr = ''
    for path in pathEnvStrList:
        if 'VulkanSDK' in path:
            foundVulkanSDK = True
            dxcCmdStr = path

    if foundVulkanSDK is False:
        sys.exit('Cannot find the Vulkan SDK in the PATH environment variable.')

    return dxcCmdStr + '\\dxc.exe'


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Compile the target .hlsl shaders')
    parser.add_argument('--src', type=str, help='path to the target hlsl shader', default="")
    parser.add_argument('--dstDir', type=str, help='path to the output spv folder', default="")
    args = parser.parse_args()

    if args.src.find('.hlsl') == -1:
        sys.exit('The input is not a hlsl file')

    srcName = ShaderName(args.src)
    dstPathName = args.dstDir + '/' + srcName + '.spv'
    profile = SelectProfile(srcName)
    dxcCmdStr = SelectDxc()

    subprocess.check_output([
        dxcCmdStr,
        '-spirv',
        '-T', profile,
        '-E', 'main',
        '-I', os.path.realpath(os.path.dirname(__file__)),
        '-fspv-extension=SPV_KHR_ray_tracing',
        '-fspv-extension=SPV_KHR_multiview',
        '-fspv-extension=SPV_KHR_shader_draw_parameters',
        '-fspv-extension=SPV_EXT_descriptor_indexing',
        args.src,
        '-Fo', dstPathName
    ])
    