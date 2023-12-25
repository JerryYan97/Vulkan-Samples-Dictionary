#include <GGXModel.hlsl>

struct SceneInfoUbo
{
	float3 cameraPosition;
};

struct RenderTargetInfo
{
	float width;
	float height;
};

[[vk::binding(2, 0)]] cbuffer UBO1 { SceneInfoUbo i_sceneInfo; }

[[vk::binding(3, 0)]] Texture2D    i_worldPosTexture;
[[vk::binding(3, 0)]] SamplerState i_worldPosSamplerState;

[[vk::binding(4, 0)]] Texture2D i_worldNormalTexture;
[[vk::binding(4, 0)]] SamplerState i_worldNormalSamplerState;

[[vk::binding(5, 0)]] Texture2D i_albedoTexture;
[[vk::binding(5, 0)]] SamplerState i_albedoSamplerState;

[[vk::binding(5, 0)]] Texture2D i_metallicRoughnessTexture;
[[vk::binding(5, 0)]] SamplerState i_metallicRoughnessSamplerState;

[[vk::binding(1, 0)]] StructuredBuffer<float3> lightsPosStorageData;
[[vk::binding(6, 0)]] StructuredBuffer<float3> lightsRadianceStorageData;

[[vk::push_constant]] RenderTargetInfo pixelWidthHeight;

float4 main(
	float4 i_pixelPos : SV_POSITION,
    float4 i_instId : BLENDINDICES0) : SV_Target
{
	/*
    float3 lightColor = float3(24.0, 24.0, 24.0);

	// Gold
	float3 sphereRefAlbedo = float3(1.00, 0.71, 0.09); // F0
	float3 sphereDifAlbedo = float3(1.00, 0.71, 0.09);

	float3 wo = normalize(i_sceneInfo.cameraPosition - i_pixelWorldPos.xyz);
	
	float3 worldNormal = normalize(i_pixelWorldNormal.xyz);

	float viewNormalCosTheta = max(dot(worldNormal, wo), 0.0);

	float metallic = i_params.x;
	float roughness = i_params.y;

	float3 Lo = float3(0.0, 0.0, 0.0); // Output light values to the view direction.
	for(int i = 0; i < 4; i++)
	{
		float3 lightPos = i_sceneInfo.lightPositions[i];
		float3 wi       = normalize(lightPos - i_pixelWorldPos.xyz);
		float3 H	    = normalize(wi + wo);
		float distance  = length(lightPos - i_pixelWorldPos.xyz);

		float  attenuation = 1.0 / (distance * distance);
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

	float3 ambient = float3(0.0001, 0.0001, 0.0001) * sphereRefAlbedo;
    float3 color = ambient + Lo;
	
    // Gamma Correction
    color = color / (color + float3(1.0, 1.0, 1.0));
    color = pow(color, float3(1.0/2.2, 1.0/2.2, 1.0/2.2));  

	return float4(color, 1.0);
	*/
	return i_pixelPos;
}