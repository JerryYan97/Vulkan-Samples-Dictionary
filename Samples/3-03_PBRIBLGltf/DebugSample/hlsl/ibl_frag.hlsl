#include <GGXModel.hlsl>

// const static float3 F0 = float3(1.0, 1.0, 1.0);
// const static float3 Albedo = float3(1.0, 1.0, 1.0); 

// NOTE: [[vk::binding(X[, Y])]] -- X: binding number, Y: descriptor set.
// TODO: We'll two descriptor sets since only the camera matrices are needed to be updated.

struct SceneInfoUbo
{
    float maxMipLevel;
};

[[vk::binding(0, 1)]] TextureCube i_diffuseCubeMapTexture;
[[vk::binding(0, 1)]] SamplerState i_diffuseCubemapSamplerState;

[[vk::binding(1, 1)]] TextureCube i_prefilterEnvCubeMapTexture;
[[vk::binding(1, 1)]] SamplerState i_prefilterEnvCubeMapSamplerState;

[[vk::binding(2, 1)]] Texture2D    i_envBrdfTexture;
[[vk::binding(2, 1)]] SamplerState i_envBrdfSamplerState;

[[vk::binding(0, 2)]] Texture2D i_baseColorTexture;
[[vk::binding(0, 2)]] SamplerState i_baseColorSamplerState;

[[vk::binding(1, 2)]] Texture2D i_normalTexture;
[[vk::binding(1, 2)]] SamplerState i_normalSamplerState;

// The textures for metalness and roughness properties are packed together in a single texture called
// metallicRoughnessTexture. Its green channel contains roughness values and its blue channel contains metalness
// values.
[[vk::binding(2, 2)]] Texture2D i_metallicRoughnessTexture;
[[vk::binding(2, 2)]] SamplerState i_metallicRoughnessSamplerState;

[[vk::binding(3, 2)]] Texture2D i_occlusionTexture;
[[vk::binding(3, 2)]] SamplerState i_occlusionSamplerState;

[[vk::push_constant]] SceneInfoUbo i_sceneInfo;

float4 main(
    float4 i_pixelWorldPos     : POSITION0,
    float4 i_pixelWorldNormal  : NORMAL0,
    float4 i_pixelWorldTangent : TANGENT0,
    float2 i_pixelWorldUv      : TEXCOORD0) : SV_Target
{
    float3 V = normalize(-i_pixelWorldPos.xyz);
    float3 N = normalize(i_pixelWorldNormal.xyz);
    float NoV = saturate(dot(N, V));
    float3 R = 2 * NoV * N - V;

    float2 roughnessMetalic = i_metallicRoughnessTexture.Sample(i_metallicRoughnessSamplerState, i_pixelWorldUv).xy;
    float3 baseColor = i_baseColorTexture.Sample(i_baseColorSamplerState, i_pixelWorldUv).xyz;
    float3 normalSampled = i_normalTexture.Sample(i_normalSamplerState, i_pixelWorldUv).xyz;
    float occlusion = i_occlusionTexture.Sample(i_occlusionSamplerState, i_pixelWorldUv).x;

    float metalic = roughnessMetalic[1];
    float roughness = roughnessMetalic[0];

    float3 diffuseIrradiance = i_diffuseCubeMapTexture.Sample(i_diffuseCubemapSamplerState, N).xyz;

    float3 prefilterEnv = i_prefilterEnvCubeMapTexture.SampleLevel(i_prefilterEnvCubeMapSamplerState,
                                                                   R, roughness * i_sceneInfo.maxMipLevel).xyz;

    float2 envBrdf = i_envBrdfTexture.Sample(i_envBrdfSamplerState, float2(NoV, roughness)).xy;

    float3 Ks = fresnelSchlickRoughness(NoV, baseColor, roughness);
    float3 Kd = float3(1.0, 1.0, 1.0) - Ks;
    Kd *= (1.0 - metalic);

    float3 diffuse = Kd * diffuseIrradiance * baseColor;
    float3 specular = prefilterEnv * (baseColor * envBrdf.x + envBrdf.y);

    // return float4(diffuse + specular, 1.0);
    return float4(baseColor, 1.0);
}