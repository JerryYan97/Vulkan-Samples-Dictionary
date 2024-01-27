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
};

struct VertUBO
{
    float4x4 vpMat;
};

static const float3 g_sphereWorldPos[14] = {
    float3(15.0,  2.5, -8.0),
    float3(15.0,  2.5, -8.0 + 1.0 * (16.0 / 6.0)),
    float3(15.0,  2.5, -8.0 + 2.0 * (16.0 / 6.0)),
    float3(15.0,  2.5, -8.0 + 3.0 * (16.0 / 6.0)),
    float3(15.0,  2.5, -8.0 + 4.0 * (16.0 / 6.0)),
    float3(15.0,  2.5, -8.0 + 5.0 * (16.0 / 6.0)),
    float3(15.0,  2.5, -8.0 + 6.0 * (16.0 / 6.0)),
    float3(15.0,  -2.5, -8.0),
    float3(15.0,  -2.5, -8.0 + 1.0 * (16.0 / 6.0)),
    float3(15.0,  -2.5, -8.0 + 2.0 * (16.0 / 6.0)),
    float3(15.0,  -2.5, -8.0 + 3.0 * (16.0 / 6.0)),
    float3(15.0,  -2.5, -8.0 + 4.0 * (16.0 / 6.0)),
    float3(15.0,  -2.5, -8.0 + 5.0 * (16.0 / 6.0)),
    float3(15.0,  -2.5, -8.0 + 6.0 * (16.0 / 6.0))
};

[[vk::binding(0, 0)]] cbuffer UBO0 { VertUBO i_vertUbo; }

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

VSOutput main(
    uint instId : SV_InstanceID,
    VSInput i_vertInput)
{
    VSOutput output = (VSOutput)0;

    float4x4 modelMat = PosToModelMat(g_sphereWorldPos[instId]);
    float roughnessOffset = 1.0 / 7.0;
    int instIdRemap = instId % 7;

    output.Params.x = 1.0;
    if(instId >= 7)
    {
        output.Params.x = 0.0;
    }

    output.Params.y = min(instIdRemap * roughnessOffset + 0.05, 1.0);

    float4 worldPos = mul(modelMat, float4(i_vertInput.vPosition, 1.0));
    float4 worldNormal = mul(modelMat, float4(i_vertInput.vNormal, 0.0));

    output.WorldPos = worldPos;
    output.Normal.xyz = normalize(worldNormal.xyz);
    output.Pos = mul(i_vertUbo.vpMat, worldPos);

    return output;
}