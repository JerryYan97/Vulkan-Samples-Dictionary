#version 450

#extension GL_EXT_multiview : enable

layout (location = 0) flat out int outViewId;

vec2 positions[6] = vec2[](
    vec2(-1.f, -1.f),
    vec2( 1.f, -1.f),
    vec2(-1.f,  1.f),
    vec2( 1.f, -1.f),
    vec2( 1.f,  1.f),
    vec2(-1.f,  1.f)
);

void main() 
{
    outViewId = gl_ViewIndex;

    gl_Position = vec4(positions[gl_VertexIndex], 0.5, 1.0);
}