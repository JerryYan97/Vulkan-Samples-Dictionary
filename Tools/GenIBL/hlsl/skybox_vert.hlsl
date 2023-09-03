#version 450

layout(location = 0) out vec2 o_screenUv;

vec2 positions[6] = vec2[](
    vec2(-1.f, -1.f),
    vec2( 1.f, -1.f),
    vec2(-1.f,  1.f),
    vec2( 1.f, -1.f),
    vec2( 1.f,  1.f),
    vec2(-1.f,  1.f)
);

vec2 screenUvs[6] = vec2[](
    vec2(-1.f,  1.f),
    vec2( 1.f,  1.f),
    vec2(-1.f, -1.f),
    vec2( 1.f,  1.f),
    vec2( 1.f, -1.f),
    vec2(-1.f, -1.f)
);

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.5, 1.0);
    o_screenUv = screenUvs[gl_VertexIndex];
}