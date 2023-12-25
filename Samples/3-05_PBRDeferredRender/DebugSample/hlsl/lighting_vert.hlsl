#pragma pack_matrix(row_major)

// We only render the gbuffer covered by the light volume. The rendered results will be added together.

struct VSOutput
{
    float4 Pos : SV_POSITION;
    nointerpolation uint instId : BLENDINDICES0;
};

struct VSInput
{
    float3 vPosition : POSITION;
    float3 vNormal : NORMAL;
};

struct VertUBO
{
    float4x4 vpMat;
};

float4x4 PosToModelMat(float3 pos)
{
    // NOTE: HLSL's matrices are column major.
    // But, it is filled column by column in this way. So it's good.
    // As for the UBO mat input, we still need to transpose the row-major matrix.
    float4x4 mat = { float4(1.0, 0.0, 0.0, pos.x),
                     float4(0.0, 1.0, 0.0, pos.y),
                     float4(0.0, 0.0, 1.0, pos.z),
                     float4(0.0, 0.0, 0.0, 1.0) };
                     
    return mat;
}

[[vk::binding(0, 0)]] cbuffer UBO0 { VertUBO i_vertUbo; };
[[vk::binding(1, 0)]] StructuredBuffer<float3> lightsPosStorageData;

VSOutput main(
    uint instId : SV_InstanceID,
    VSInput i_vertInput)
{
    VSOutput output = (VSOutput)0;

    float4x4 modelMat = PosToModelMat(lightsPosStorageData[instId]);

    float4 worldPos = mul(modelMat, float4(i_vertInput.vPosition, 1.0));
    output.Pos = mul(i_vertUbo.vpMat, worldPos);
    output.instId = instId;

    return output;
}