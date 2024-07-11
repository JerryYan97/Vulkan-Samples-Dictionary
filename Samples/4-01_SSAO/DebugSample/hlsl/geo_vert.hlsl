#pragma pack_matrix(row_major)

struct VSOutput
{
    float4 Pos : SV_POSITION;
    float4 WorldPos : POSITION0;
    float4 Normal : NORMAL0;
    float4 Tangent : TANGENT0;
    float2 UV : TEXCOORD0;
    nointerpolation uint meshIdx : BLENDINDICES0;
};

struct VSInput
{
    float3 vPosition : POSITION;
    float3 vNormal : NORMAL;
    float4 vTangent : TANGENT;
    float2 vUv : TEXCOORD;
};

struct VertUBO
{
    float4x4 modelMat;
    float4x4 vpMat;
};

struct PushConstantBuffer
{
    uint meshIdx;
};

[[vk::binding(0, 0)]] cbuffer UBO0 { VertUBO i_vertUbo; };

[[vk::push_constant]] PushConstantBuffer i_pushConstantBuffer;

VSOutput main(
    VSInput i_vertInput)
{
    VSOutput output = (VSOutput)0;

    output.WorldPos = mul(i_vertUbo.modelMat, float4(i_vertInput.vPosition, 1.0));
    output.Normal.xyz = mul(i_vertUbo.modelMat, float4(i_vertInput.vNormal, 0.0));
    output.Pos = mul(i_vertUbo.vpMat, output.WorldPos);
    output.Tangent = mul(i_vertUbo.modelMat, float4(i_vertInput.vTangent.xyz, 0.0));
    output.UV = i_vertInput.vUv;
    output.meshIdx = i_pushConstantBuffer.meshIdx;

    return output;
}