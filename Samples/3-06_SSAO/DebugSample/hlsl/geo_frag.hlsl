#pragma pack_matrix(row_major)

struct PSOutput
{
	float4 worldPos : SV_TARGET0;
	float4 worldNormal : SV_TARGET1;
	float4 albedo : SV_TARGET2;
	float2 param : SV_TARGET3; // metallic, roughness.
};

[[vk::binding(2, 0)]] StructuredBuffer<float3> albedoStorageData;
[[vk::binding(3, 0)]] StructuredBuffer<float2> metallicRoughnessStorageData;


PSOutput main(
    float4 i_pixelWorldPos    : POSITION0,
    float4 i_pixelWorldNormal : NORMAL0,
	uint   i_instId 		  : BLENDINDICES0)
{
    PSOutput output = (PSOutput)0;

	output.worldPos = i_pixelWorldPos;
	output.worldNormal = i_pixelWorldNormal;
	output.albedo = float4(albedoStorageData[i_instId], 1.0);
	output.param  = metallicRoughnessStorageData[i_instId];

	return output;
}