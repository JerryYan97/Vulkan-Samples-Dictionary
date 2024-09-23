#pragma pack_matrix(row_major)

struct PSOutput
{
	float4 worldPos : SV_TARGET0;
	float4 worldNormal : SV_TARGET1;
	float4 albedo : SV_TARGET2;
	float3 param : SV_TARGET3; // roughness, metallic, occlusion
};

[[vk::binding(1, 0)]] Texture2D i_baseColorTexture;
[[vk::binding(1, 0)]] SamplerState i_baseColorSamplerState;

[[vk::binding(2, 0)]] Texture2D i_normalTexture;
[[vk::binding(2, 0)]] SamplerState i_normalSamplerState;

// The textures for metalness and roughness properties are packed together in a single texture called
// metallicRoughnessTexture. Its green channel contains roughness values and its blue channel contains metalness
// values.
[[vk::binding(3, 0)]] Texture2D i_roughnessMetallicTexture;
[[vk::binding(3, 0)]] SamplerState i_roughnessMetallicSamplerState;

[[vk::binding(4, 0)]] Texture2D i_occlusionTexture;
[[vk::binding(4, 0)]] SamplerState i_occlusionSamplerState;

PSOutput main(
    float4 i_pixelWorldPos     : POSITION0,
    float4 i_pixelWorldNormal  : NORMAL0,
	float4 i_pixelWorldTangent : TANGENT,
    float2 i_uv 			   : TEXCOORD)
{
    PSOutput output = (PSOutput)0;

	output.worldPos = i_pixelWorldPos;
    output.worldNormal = i_normalTexture.Sample(i_normalSamplerState, i_uv);
	output.albedo = i_baseColorTexture.Sample(i_baseColorSamplerState, i_uv);
	output.param.xy  = i_roughnessMetallicTexture.Sample(i_roughnessMetallicSamplerState, i_uv).yz;
	output.param.z = i_occlusionTexture.Sample(i_occlusionSamplerState, i_uv).x;

	return output;
}