#include <GGXModel.hlsl>

struct SceneInfoUbo
{
    float maxMipLevel;
};

TextureCube i_diffuseCubeMapTexture : register(t1);
SamplerState i_diffuseCubemapSamplerState : register(s1);

TextureCube i_prefilterEnvCubeMapTexture : register(t2);
SamplerState i_prefilterEnvCubeMapSamplerState : register(s2);

Texture2D    i_envBrdfTexture : register(t3);
SamplerState i_envBrdfSamplerState : register(s3);

[[vk::push_constant]] SceneInfoUbo i_sceneInfo;

float4 main(
    float4 i_pixelWorldPos    : POSITION0,
    float4 i_pixelWorldNormal : NORMAL0,
    float2 i_params           : TEXCOORD0) : SV_Target
{
    float3 V = normalize(-i_pixelWorldPos.xyz);
    float3 N = normalize(i_pixelWorldNormal.xyz);
    float NoV = saturate(dot(N, V));
    float3 R = 2 * NoV * N - V;

    float metalic = i_params[0];
    float roughness = i_params[1];

    float3 diffuse = i_diffuseCubeMapTexture.Sample(i_diffuseCubemapSamplerState, R).xyz;

    float3 prefilterEnv = i_prefilterEnvCubeMapTexture.SampleLevel(i_prefilterEnvCubeMapSamplerState,
                                                                   R, roughness * i_sceneInfo.maxMipLevel).xyz;

    float2 envBrdf = i_envBrdfTexture.Sample(i_envBrdfSamplerState, float2(NoV, roughness)).xy;

    return float4(envBrdf, 0.0, 1.0);
}