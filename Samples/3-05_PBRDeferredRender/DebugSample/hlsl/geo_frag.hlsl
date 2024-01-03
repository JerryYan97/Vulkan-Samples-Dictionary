#pragma pack_matrix(row_major)

#include <GGXModel.hlsl>

struct RenderInfo
{
	float3 cameraPosition;
	int	   ptLightsCnt;
};

[[vk::binding(2, 0)]] StructuredBuffer<float3> i_albedoStorageData;
[[vk::binding(3, 0)]] StructuredBuffer<float2> i_metallicRoughnessStorageData;
[[vk::binding(4, 0)]] StructuredBuffer<float3> i_lightsPosStorageData;
[[vk::binding(5, 0)]] StructuredBuffer<float> i_lightsVolumeRadiusData;
[[vk::binding(6, 0)]] StructuredBuffer<float3> i_lightsRadianceStorageData;

[[vk::push_constant]] RenderInfo i_renderInfo;

float PointLightAttenuation(
	float3 pixelPos,
	float3 ptLightPos,
	float radius)
{
	float distance  = length(pixelPos - ptLightPos);

	// TODO: Same as the c++ implementation.
	const float kc = 1.f;
	const float kl = 1.f;
	const float kq = 1.f;
	// const float kl = 0.14f;
	// const float kq = 0.07f;
	float attenuation = 1.0 / (kc + kl * distance + kq * distance * distance);

	if(distance > radius)
	{
		attenuation = 0.0;
	}

	return attenuation;
}

float4 main(
    float4 i_pixelWorldPos    : POSITION0,
    float4 i_pixelWorldNormal : NORMAL0,
	uint   i_instId 		  : BLENDINDICES0) : SV_TARGET
{
    float3 lightColor = i_lightsRadianceStorageData[i_instId];

	float3 sphereRefAlbedo = i_albedoStorageData[i_instId]; // F0
	float3 sphereDifAlbedo = i_albedoStorageData[i_instId];
	
	float3 pixelWorldPos = i_pixelWorldPos.xyz;

	float3 wo = normalize(i_renderInfo.cameraPosition - pixelWorldPos);

	float3 worldNormal = normalize(i_pixelWorldNormal.xyz);

	float viewNormalCosTheta = max(dot(worldNormal, wo), 0.0);

	float metallic = i_metallicRoughnessStorageData[i_instId].x;
	float roughness = i_metallicRoughnessStorageData[i_instId].y;

	float3 Lo = float3(0.0, 0.0, 0.0); // Output light values to the view direction.
	for(int i = 0; i < i_renderInfo.ptLightsCnt; i++)
	{
		float3 lightPos = i_lightsPosStorageData[i];
		float3 wi       = normalize(lightPos - pixelWorldPos);
		float3 H	    = normalize(wi + wo);

		float  attenuation = PointLightAttenuation(pixelWorldPos, lightPos, i_lightsVolumeRadiusData[i]);
		float3 radiance    = lightColor * attenuation; 

		float lightNormalCosTheta = max(dot(worldNormal, wi), 0.0);

		float NDF = DistributionGGX(worldNormal, H, roughness);
	    float G   = GeometrySmithDirectLight(worldNormal, wo, wi, roughness);

		float3 F0 = float3(0.04, 0.04, 0.04);
	    F0        = lerp(F0, sphereRefAlbedo, float3(metallic, metallic, metallic));
	    float3 F  = FresnelSchlick(max(dot(H, wo), 0.0), F0);

		float3 NFG = NDF * F * G;

		float denominator = 4.0 * viewNormalCosTheta * lightNormalCosTheta  + 0.0001;
		
		float3 specular = NFG / denominator;

		float3 kD = float3(1.0, 1.0, 1.0) - F; // The amount of light goes into the material.
		kD *= (1.0 - metallic);

		Lo += (kD * (sphereDifAlbedo / 3.14159265359) + specular) * radiance * lightNormalCosTheta;
	}

	// float3 ambient = float3(0.0001, 0.0001, 0.0001) * sphereRefAlbedo;
    float3 color = Lo;
	
    // Gamma Correction
    // color = color / (color + float3(1.0, 1.0, 1.0));
    // color = pow(color, float3(1.0/2.2, 1.0/2.2, 1.0/2.2));  

	return float4(color, 1.0);
}