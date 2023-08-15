#version 450

// layout (binding = 1) uniform sampler2D hdriTexture;

layout (location = 0) flat in int inViewId;

layout (location = 0) out vec4 outColor;

void main()
{
    float viewIdFloat = float(inViewId);
    vec3 viewColor = vec3(viewIdFloat / 6.0, viewIdFloat / 6.0, viewIdFloat / 6.0);
    outColor = vec4(viewColor, 0.0);
}