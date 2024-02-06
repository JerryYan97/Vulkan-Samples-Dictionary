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
    float4 vBlendWeights : BLENDWEIGHT;
    uint4  vJointIndices : BLENDINDICES;
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
    float4x4 skinMat = mul(i_vertInput.vBlendWeights.x, i_jointsMats[i_vertInput.vJointIndices.x]) +
                       mul(i_vertInput.vBlendWeights.y, i_jointsMats[i_vertInput.vJointIndices.y]) +
                       mul(i_vertInput.vBlendWeights.z, i_jointsMats[i_vertInput.vJointIndices.z]) +
                       mul(i_vertInput.vBlendWeights.w, i_jointsMats[i_vertInput.vJointIndices.w]);
    
    float4x4 mvpMat = mul(i_vertPushConstant.vpMat, skinMat);

    output.Pos = mul(mvpMat, float4(i_vertInput.vPosition, 1.0));
    output.WorldPos = mul(skinMat, float4(i_vertInput.vPosition, 1.0));
    output.Normal = mul(skinMat, float4(i_vertInput.vNormal, 0.0));

    // output.Pos = mul(i_vertPushConstant.vpMat, float4(i_vertInput.vPosition, 1.0));
    // output.WorldPos = float4(i_vertInput.vPosition, 1.0);
    // output.Normal = float4(i_vertInput.vNormal, 0.0);

    output.UV = i_vertInput.vUv;

    return output;
}