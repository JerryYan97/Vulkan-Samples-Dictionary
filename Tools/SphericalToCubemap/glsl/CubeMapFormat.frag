#version 450

// It looks like Vulkan's uv is also different: The upper-left corner of the image is (0, 0) and the bottom-right corner of the image is (width, height).
// Vulkan cubemap sequence: pos-x, neg-x, pos-y, neg-y, pos-z, neg-z.
layout (binding = 0) uniform sampler2D hdriTexture;
layout (binding = 1) uniform SceneInfo
{
    mat4 rotMats[6]; // NOTE: Alignment is mat4.
    vec2 widthHeight; // Screen width and height.
} i_sceneInfo;

layout (location = 0) flat in int inViewId;

layout (location = 0) out vec4 outColor;

#define M_PI 3.1415926535897932384626433832795

void main()
{
    float width = i_sceneInfo.widthHeight.x;
    float height = i_sceneInfo.widthHeight.y;

    float i = gl_FragCoord.x;
    float j = height - gl_FragCoord.y;
    vec3 p = vec3((2.0 * i / width) - 1.0, 1.0, (2.0 * j / height) - 1.0);

    mat4 rotMat = i_sceneInfo.rotMats[inViewId];

    vec4 p_prime = rotMat * vec4(p, 0.0);

    // NOTE: The problem relates to both camera coordinate system and the primary axis for longtitude.
    // float longitude = atan(p_prime.y, p_prime.x);
    float longitude = atan(p_prime.x, p_prime.y);
    // float longitude = atan(p_prime.y / p_prime.x);
    float latitude = atan(p_prime.z, sqrt(p_prime.x * p_prime.x + p_prime.y * p_prime.y));

    float I = (longitude + M_PI) / (2.0 * M_PI);
    float J = (latitude + M_PI / 2.0) / M_PI;

    vec2 uv = vec2(I, 1.0 - J);

    // Remap uv to accomodate vulkan's cubemap s_face, t_face sampling rule.
    vec2 st = uv;

    vec4 cubemapColor = texture(hdriTexture, uv);

    outColor = cubemapColor;
}