Algorithm:

HLSL:
Sample command:

`dxc -spirv -T vs_6_1 -E main -fspv-extension=SPV_KHR_multiview "C:\JiaruiYan\Projects\OneFileVulkans\Samples\2-04_DumpQuad\DebugSample\hlsl\DumpQuadVert.hlsl" -Fo DumpQuadVert.spv`

`dxc -spirv -T ps_6_1 -E main -fspv-extension=SPV_KHR_multiview "C:\JiaruiYan\Projects\OneFileVulkans\Samples\2-04_DumpQuad\DebugSample\hlsl\DumpQuadFrag.hlsl" -Fo DumpQuadFrag.spv`

References:

Mipmap: https://vulkan-tutorial.com/Generating_Mipmaps

Vulkan HLSL: https://github.com/SaschaWillems/Vulkan/blob/master/shaders/hlsl/README.md

DXC doc: https://github.com/microsoft/DirectXShaderCompiler/blob/main/docs/SPIR-V.rst