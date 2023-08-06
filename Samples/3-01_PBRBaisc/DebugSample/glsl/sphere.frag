#version 450

layout(binding = 1) uniform SceneInfo
{
	vec3 lightPositions[4];
	vec3 cameraPosition;
} i_sceneInfo;

layout(location = 0) in vec3 i_worldPos;
layout(location = 1) in vec3 i_worldNormal;
layout(location = 2) flat in vec2 i_params; // [metallic, roughness].

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

vec3 FresnelSchlick(float lightCosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - lightCosTheta, 0.0, 1.0), 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

void main()
{
	vec3 lightColor = vec3(23.47, 21.31, 20.79);
	vec3 sphereRefAlbedo = vec3(1.0, 0.3, 0.3);
	vec3 sphereDifAlbedo = vec3(1.0, 0.3, 0.3);

	vec3 wo = normalize(i_sceneInfo.cameraPosition - i_worldPos);
	
	vec3 worldNormal = normalize(i_worldNormal);

	float viewNormalCosTheta = max(dot(worldNormal, wo), 0.0);

	float metallic = i_params.x;
	float roughness = i_params.y;

	vec3 tempColor;

	vec3 Lo = vec3(0.0); // Output light values to the view direction.
	for(int i = 0; i < 4; i++)
	{
		vec3 lightPos = i_sceneInfo.lightPositions[i];
		vec3 wi       = normalize(lightPos - i_worldPos);
		vec3 H	      = normalize(wi + wo);
		float distance = length(lightPos - i_worldPos);

		float attenuation = 1.0 / (distance * distance);
		vec3 radiance     = lightColor * attenuation; 

		tempColor = radiance;

		float lightNormalCosTheta = max(dot(worldNormal, wi), 0.0);

		float NDF = DistributionGGX(worldNormal, H, roughness);
	    float G   = GeometrySmith(worldNormal, wo, wi, roughness);

		vec3 F0 = vec3(0.04);
	    F0      = mix(F0, sphereRefAlbedo, metallic);
	    vec3 F  = FresnelSchlick(max(dot(H, wo), 0.0), F0);

		vec3 NFG = NDF * F * G;

		float denominator = 4.0 * viewNormalCosTheta * lightNormalCosTheta  + 0.0001;
		
		vec3 specular = NFG / denominator;

		// tempColor = vec3(viewNormalCosTheta);

		vec3 kD = vec3(1.0) - F; // The amount of light goes into the material.
		kD *= (1.0 - metallic);

		Lo += (kD * (sphereDifAlbedo / PI) + specular) * radiance * lightNormalCosTheta;
	}

	vec3 ambient = vec3(0.03) * sphereRefAlbedo;
    vec3 color = ambient + Lo;
	
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));  

	// tempColor = vec3(viewNormalCosTheta);
	tempColor = normalize(i_worldNormal + vec3(1.0));
	
	outColor = vec4(tempColor, 1.0);
	// outColor = vec4(i_sceneInfo.lightPositions[0], 0.0);
	// outColor = vec4(tempColor, 0.0);
}