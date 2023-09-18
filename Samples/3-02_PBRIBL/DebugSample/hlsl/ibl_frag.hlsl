struct SceneInfoUbo
{
    float3 cameraPosition;
};

TextureCube i_diffuseCubeMapTexture : register(t1);
SamplerState i_diffuseCubemapSamplerState : register(s1);

TextureCube i_prefilterEnvCubeMapTexture : register(t2);
SamplerState i_prefilterEnvCubeMapSamplerState : register(s2);

Texture2D    i_envBrdfTexture : register(t3);
SamplerState i_envBrdfSamplerState : register(s3);

cbuffer UBO0 : register(b4) { SceneInfoUbo i_cameraInfo; }

float4 main(
    float4 i_pixelWorldPos    : POSITION0,
    float4 i_pixelWorldNormal : NORMAL0,
    float2 i_params           : TEXCOORD0) : SV_Target
{
    float3 V = normalize(i_cameraInfo.cameraPosition);
    float3 N = i_pixelWorldNormal.xyz;
    float NoV = saturate(dot(N, V));
    float3 R = 2 * NoV * N - V;

    // float3 prefilterColor = i_prefilterEnvCubeMapTexture.texture();
    // float3 envBrdf = 

    return i_pixelWorldNormal;
}