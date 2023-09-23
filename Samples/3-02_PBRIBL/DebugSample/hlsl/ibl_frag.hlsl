#include <GGXModel.hlsl>

const static float3 F0 = float3(1.0, 1.0, 1.0);
const static float3 Albedo = float3(1.0, 1.0, 1.0); 

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

    float3 diffuseIrradiance = i_diffuseCubeMapTexture.Sample(i_diffuseCubemapSamplerState, N).xyz;

    float3 prefilterEnv = i_prefilterEnvCubeMapTexture.SampleLevel(i_prefilterEnvCubeMapSamplerState,
                                                                   R, roughness * i_sceneInfo.maxMipLevel).xyz;

    float2 envBrdf = i_envBrdfTexture.Sample(i_envBrdfSamplerState, float2(NoV, roughness)).xy;

    float3 Ks = fresnelSchlickRoughness(NoV, F0, roughness);
    float3 Kd = float3(1.0, 1.0, 1.0) - Ks;
    Kd *= (1.0 - metalic);

    float3 diffuse = Kd * diffuseIrradiance * Albedo;
    float3 specular = prefilterEnv * (F0 * envBrdf.x + envBrdf.y);

    return float4(diffuse + specular, 1.0);
}