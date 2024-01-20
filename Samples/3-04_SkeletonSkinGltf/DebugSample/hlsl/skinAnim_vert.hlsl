#pragma pack_matrix(row_major)

// NOTE: This example only supports the base color and assume fix roughness and metallic, but we'll still use the IBL.

struct VSOutput
{
    float4 Pos : SV_POSITION;
    float4 WorldPos : POSITION0;
    float4 Normal : NORMAL0;
    float2 UV : TEXCOORD0;
};

struct VSInput
{
    float3 vPosition     : POSITION;
    float3 vNormal       : NORMAL;
    float2 vUv           : TEXCOORD;
    float  vBlendWeight0 : BLENDWEIGHT0;
    float  vBlendWeight1 : BLENDWEIGHT1;
    float  vBlendWeight2 : BLENDWEIGHT2;
    float  vBlendWeight3 : BLENDWEIGHT3;
    uint   vBlendIdx0    : BLENDINDICES0;
    uint   vBlendIdx1    : BLENDINDICES1;
    uint   vBlendIdx2    : BLENDINDICES2;
    uint   vBlendIdx3    : BLENDINDICES3;
};

struct VertPushConstant
{
    float4x4 vpMat;
};

[[vk::push_constant]] VertPushConstant i_vertPushConstant;

// JointMat = JointGlobalMat * JointInverseBindMat;
[[vk::binding(0, 0)]] StructuredBuffer<float4x4> i_jointsMats;

VSOutput main(
    VSInput i_vertInput)
{
    VSOutput output = (VSOutput)0;

    // Theoricitally, the scaling part of the skin matrix should be 1 after adding since joints don't have scaling and
    // the weight added together should be 1. Thus the normal matrix is just the skinMat itself.
    float4x4 skinMat = mul(i_vertInput.vBlendWeight0, i_jointsMats[i_vertInput.vBlendIdx0]) +
                       mul(i_vertInput.vBlendWeight1, i_jointsMats[i_vertInput.vBlendIdx1]) +
                       mul(i_vertInput.vBlendWeight2, i_jointsMats[i_vertInput.vBlendIdx2]) +
                       mul(i_vertInput.vBlendWeight3, i_jointsMats[i_vertInput.vBlendIdx3]);
    
    float4x4 mvpMat = i_vertPushConstant.vpMat * skinMat;

    output.Pos = mul(mvpMat, float4(i_vertInput.vPosition, 1.0));
    output.WorldPos = mul(skinMat, float4(i_vertInput.vPosition, 1.0));
    output.Normal = mul(skinMat, float4(i_vertInput.vNormal, 0.0));
    output.UV = i_vertInput.vUv;

    return output;
}