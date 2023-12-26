#pragma pack_matrix(row_major)

// We only render the gbuffer covered by the light volume. The rendered results will be added together.

struct VSOutput
{
    float4 Pos : SV_POSITION;
    nointerpolation float4 lightPos: POSITION0;
    nointerpolation float4 radiance: POSITION1;
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

float4x4 PosToModelMat(float3 pos, float radius)
{
    // NOTE: HLSL's matrices are column major.
    // But, it is filled column by column in this way. So it's good.
    // As for the UBO mat input, we still need to transpose the row-major matrix.
    float4x4 mat = { float4(radius, 0.0, 0.0, pos.x),
                     float4(0.0, radius, 0.0, pos.y),
                     float4(0.0, 0.0, radius, pos.z),
                     float4(0.0, 0.0, 0.0,    1.0) };
                     
    return mat;
}

[[vk::binding(0, 0)]] cbuffer UBO0 { VertUBO i_vertUbo; };
[[vk::binding(1, 0)]] StructuredBuffer<float3> lightsPosStorageData;
[[vk::binding(2, 0)]] StructuredBuffer<float> lightsVolumeRadiusData;
[[vk::binding(3, 0)]] StructuredBuffer<float3> lightsRadianceStorageData;

VSOutput main(
    uint instId : SV_InstanceID,
    VSInput i_vertInput)
{
    VSOutput output = (VSOutput)0;

    float4x4 modelMat = PosToModelMat(lightsPosStorageData[instId], lightsVolumeRadiusData[instId]);

    float4 worldPos = mul(modelMat, float4(i_vertInput.vPosition, 1.0));
    output.Pos = mul(i_vertUbo.vpMat, worldPos);
    output.lightPos = float4(lightsPosStorageData[instId], 1.0);
    output.radiance = float4(lightsRadianceStorageData[instId], 1.0);

    return output;
}