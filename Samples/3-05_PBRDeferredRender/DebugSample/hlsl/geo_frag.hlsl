#pragma pack_matrix(row_major)

struct PSOutput
{
	float3 worldPos : SV_Target0;
	float3 worldNormal : SV_TARGET1;
	float3 albedo : SV_TARGET2;
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
	output.albedo = albedoStorageData[i_instId];
	output.param  = metallicRoughnessStorageData[i_instId];

	return output;
}