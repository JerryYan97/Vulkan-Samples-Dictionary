#include <GGXModel.hlsl>

// NOTE: [[vk::binding(X[, Y])]] -- X: binding number, Y: descriptor set.
// NOTE: We assume that the metallic, roughness and occlusion are in the same texture. x: occlusion, y: roughness, z: metal.
// NOTE: Push Descriptor only supports one descriptor set.

const static float3 Albedo = float3(0.56, 0.57, 0.58); 

struct SceneInfoUbo
{
    float maxMipLevel;
};

[[vk::binding(1, 0)]] TextureCube i_diffuseCubeMapTexture;
[[vk::binding(1, 0)]] SamplerState i_diffuseCubemapSamplerState;

[[vk::binding(2, 0)]] TextureCube i_prefilterEnvCubeMapTexture;
[[vk::binding(2, 0)]] SamplerState i_prefilterEnvCubeMapSamplerState;

[[vk::binding(3, 0)]] Texture2D    i_envBrdfTexture;
[[vk::binding(3, 0)]] SamplerState i_envBrdfSamplerState;

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

    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, Albedo, float3(metalic, metalic, metalic));

    float3 diffuseIrradiance = i_diffuseCubeMapTexture.Sample(i_diffuseCubemapSamplerState, N).xyz;

    float3 prefilterEnv = i_prefilterEnvCubeMapTexture.SampleLevel(i_prefilterEnvCubeMapSamplerState,
                                                                   R, roughness * i_sceneInfo.maxMipLevel).xyz;

    float2 envBrdf = i_envBrdfTexture.Sample(i_envBrdfSamplerState, float2(NoV, roughness)).xy;

    float3 Ks = fresnelSchlickRoughness(NoV, F0, roughness);
    float3 Kd = float3(1.0, 1.0, 1.0) - Ks;
    Kd *= (1.0 - metalic);

    float3 diffuse = Kd * diffuseIrradiance * Albedo;
    float3 specular = prefilterEnv * (Ks * envBrdf.x + envBrdf.y);

    return float4(diffuse + specular, 1.0);
    // return float4(Ks, 1.0);
}