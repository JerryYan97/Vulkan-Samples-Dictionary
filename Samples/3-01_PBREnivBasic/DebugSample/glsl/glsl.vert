#version 450

layout(location = 0) out vec2 o_uv;
layout(location = 1) out vec2 o_viewXy;

float M_PI = 3.1415926;
float aspect = 640.0 / 1280.0;
float fov = 120.0 * M_PI / 180.0; // horiztontial field of view.
float width = 0.2 * tan(fov / 2.0);
float height = aspect * width;

vec2 positions[6] = vec2[](
    vec2(-1.f, -1.f),
    vec2( 1.f, -1.f),
    vec2(-1.f,  1.f),
    vec2( 1.f, -1.f),
    vec2( 1.f,  1.f),
    vec2(-1.f,  1.f)
);

vec2 uvs[6] = vec2[](
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(0.0, 1.0),
    vec2(1.0, 0.0),
    vec2(1.0, 1.0),
    vec2(0.0, 1.0)
);

vec2 viewXy[6] = vec2[](
    vec2(-width / 2.0,  height / 2.0),
    vec2( width / 2.0,  height / 2.0),
    vec2(-width / 2.0, -height / 2.0),
    vec2( width / 2.0,  height / 2.0),
    vec2( width / 2.0, -height / 2.0),
    vec2(-width / 2.0, -height / 2.0)
);

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.5, 1.0);
    o_uv = uvs[gl_VertexIndex];
    o_viewXy = viewXy[gl_VertexIndex];
}