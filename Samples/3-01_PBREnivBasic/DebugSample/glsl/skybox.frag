#version 450

layout(binding = 0) uniform samplerCube i_cubeMapTexture;
layout(binding = 1) uniform mat4 i_invViewPersMat;
layout(binding = 2) uniform vec3 i_viewVec; // In the world space

layout(location = 0) in vec2 i_viewXy;

layout(location = 0) out vec4 outColor;

float M_PI = 3.1415926;

void main() {
    vec3 view_pos = vec3(i_viewXy, -0.1);
    vec3 view_dir = normalize(view_pos);
    
    outColor = texture(i_cubeMapTexture, view_dir);
}