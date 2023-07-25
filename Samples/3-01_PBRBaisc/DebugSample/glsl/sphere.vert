#version 450

layout(binding = 0) uniform Matices {
    mat4 vpMat;
    mat4 modelMat; // Assume uniform scale.
} i_matrices;

layout(location = 0) in vec3 i_pos;
layout(location = 1) in vec3 i_normal;

layout(location = 0) out vec3 o_worldPos;
layout(location = 1) out vec3 o_worldNormal;

void main() {
    mat4 mvpMat = i_matrices.modelMat * i_matrices.vpMat;

    vec4 worldPos = i_matrices.modelMat * vec4(i_pos, 1.0);
    vec4 worldNormal = i_matrices.modelMat * vec4(i_normal, 0.0);

    o_worldPos = worldPos.xyz;
    o_worldNormal = worldNormal.xyz;

    gl_Position = i_matrices.modelMat * i_matrices.vpMat * vec4(i_pos, 1.0);
}