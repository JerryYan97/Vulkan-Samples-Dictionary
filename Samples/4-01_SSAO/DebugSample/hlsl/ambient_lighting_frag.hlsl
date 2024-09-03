#pragma pack_matrix(row_major)

[[vk::binding(1, 0)]] Texture2D i_baseColorTexture;
[[vk::binding(1, 0)]] SamplerState i_baseColorSamplerState;

float4 main(
    float2 i_uv : TEXCOORD0) : SV_TARGET0
{
    float4 output = (float4)0;

    output = i_baseColorTexture.Sample(i_baseColorSamplerState, i_uv);

    return output;
}