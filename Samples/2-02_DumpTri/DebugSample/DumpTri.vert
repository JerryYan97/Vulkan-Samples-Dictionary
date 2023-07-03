#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) out vec3 outColor;

vec2 positions[3] = vec2[](
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, 0.5)
);

vec4 checkPos[3] = vec4[](
    vec4(-1.73205, 2.44525, -1.91919, -2.0),
    vec4(1.73205, 2.44525, -3.93939, -4.0),
    vec4(1.73205, 2.44525, -1.91919, -2.0)
);

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 2.0);
    // gl_Position = checkPos[gl_VertexIndex];
    outColor = colors[gl_VertexIndex];
}