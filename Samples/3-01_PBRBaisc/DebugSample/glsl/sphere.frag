#version 450

layout(binding = 0) uniform Lights
{
	vec3 lightPositions[4];
} i_lights;

layout(location = 0) in vec3 i_worldPos;
layout(location = 1) in vec3 i_worldNormal;

layout(location = 0) out vec4 outColor;

void main()
{
	outColor = vec4(i_worldNormal, 0.0);
}