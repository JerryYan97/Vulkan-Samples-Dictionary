#version 450

layout (binding = 0) uniform sampler2D cubemapTextures[6];
layout (binding = 1) uniform UboInfo
{
    vec2 widthHeight;
} screenInfo;

layout (location = 0) flat in int inViewId;

layout (location = 0) out vec4 outColor;

#define M_PI 3.1415926535897932384626433832795

void main()
{
    float width =  screenInfo.widthHeight.x;
    float height = screenInfo.widthHeight.y;

    vec2 uv = vec2(gl_FragCoord.x / width, gl_FragCoord.y / height);

    vec4 cubemapColor = texture(cubemapTextures[inViewId], uv);

    outColor = cubemapColor;
}