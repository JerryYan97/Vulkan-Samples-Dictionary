struct VSOutput
{
    float4 Pos : SV_POSITION;
    float4 WorldPos : POSITION0;
    float4 Normal : NORMAL0;
    nointerpolation float2 Params : TEXCOORD0; // [Metalic, roughness].
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

cbuffer UBO0 : register(b0) { VertUBO i_vertUbo; }

VSOutput main(
    VSInput i_vertInput)
{
    VSOutput output = (VSOutput)0;

    return output;
}