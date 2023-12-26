#include <GGXModel.hlsl>

struct RenderInfo
{
	float3 cameraPosition;
	float width;
	float height;
};

[[vk::binding(4, 0)]] Texture2D    i_worldPosTexture;
[[vk::binding(4, 0)]] SamplerState i_worldPosSamplerState;

[[vk::binding(5, 0)]] Texture2D i_worldNormalTexture;
[[vk::binding(5, 0)]] SamplerState i_worldNormalSamplerState;

[[vk::binding(6, 0)]] Texture2D i_albedoTexture;
[[vk::binding(6, 0)]] SamplerState i_albedoSamplerState;

[[vk::binding(7, 0)]] Texture2D i_metallicRoughnessTexture;
[[vk::binding(7, 0)]] SamplerState i_metallicRoughnessSamplerState;

[[vk::push_constant]] RenderInfo i_renderInfo;

float PointLightAttenuation(
	float3 pixelPos,
	float3 ptLightPos)
{
	float distance  = length(pixelPos - ptLightPos);

	// TODO: Same as the c++ implementation.
	const float kc = 1.f;
	const float kl = 0.14f;
	const float kq = 0.07f;
	float attenuation = 1.0 / (kc + kl * distance + kq * distance * distance);

	return attenuation;
}

float4 main(
	float4 i_fragCoord : SV_POSITION,
    float4 i_lightPos  : POSITION0,
	float4 i_radiance  : POSITION1) : SV_Target
{
	float x = i_fragCoord[0] / i_renderInfo.width;
	float y = i_fragCoord[1] / i_renderInfo.height;
	float2 uv = float2(x, y);

    float3 lightColor = i_radiance.xyz;

	float3 sphereRefAlbedo = i_albedoTexture.Sample(i_albedoSamplerState, uv).xyz; // F0
	float3 sphereDifAlbedo = i_albedoTexture.Sample(i_albedoSamplerState, uv).xyz;

	float3 pixelWorldPos = i_worldPosTexture.Sample(i_worldPosSamplerState, uv).xyz;
	float3 pixelWorldNormal = i_worldNormalTexture.Sample(i_worldNormalSamplerState, uv).xyz;
	float2 metallicRoughness = i_metallicRoughnessTexture.Sample(i_metallicRoughnessSamplerState, uv).xyz;

	float3 wo = normalize(i_renderInfo.cameraPosition - pixelWorldPos);
	
	float3 worldNormal = normalize(pixelWorldNormal);

	float viewNormalCosTheta = max(dot(worldNormal, wo), 0.0);

	float metallic = metallicRoughness.x;
	float roughness = metallicRoughness.y;

	float3 Lo = float3(0.0, 0.0, 0.0); // Output light values to the view direction.
	float3 wi = normalize(i_lightPos.xyz - pixelWorldPos);
	float3 H  = normalize(wi + wo);

	float  attenuation = PointLightAttenuation(pixelWorldPos, i_lightPos.xyz);
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

    float3 color = Lo;
	
    // Gamma Correction
    color = color / (color + float3(1.0, 1.0, 1.0));
    color = pow(color, float3(1.0/2.2, 1.0/2.2, 1.0/2.2));  

	return float4(color, 1.0);
}