#pragma pack_matrix(row_major)

[[vk::binding(0, 0)]] Texture2D i_albedoTexture;
[[vk::binding(0, 0)]] SamplerState i_albedoSamplerState;

float4 main(
    float2 i_uv : TEXCOORD0) : SV_TARGET0
{
    float4 output = (float4)0;

    output = i_albedoTexture.Sample(i_albedoSamplerState, i_uv);

    return output;
}