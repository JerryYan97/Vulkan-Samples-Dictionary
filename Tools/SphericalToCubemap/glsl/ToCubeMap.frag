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

    // Note: The app assumes a 2x2x2 box.
    // Get the point position on the face in perspective image.
    float i = gl_FragCoord.x;
    float j = height - gl_FragCoord.y;
    vec3 p = vec3((2.0 * i / width) - 1.0, 1.0, (2.0 * j / height) - 1.0);

    // Rotate the position around different axises to get the desired point position.
    mat4 rotMat = i_sceneInfo.rotMats[inViewId];
    vec4 p_prime = rotMat * vec4(p, 0.0);

    // From a world position, we can derive it's sphereical coordinate system's coordinate.
    float longitude = atan(p_prime.x, p_prime.y);
    float latitude = atan(p_prime.z, sqrt(p_prime.x * p_prime.x + p_prime.y * p_prime.y));

    // Transform the sphereical coordinate system's coordinate to UV texture coordinate.
    float I = (longitude + M_PI) / (2.0 * M_PI);
    float J = (latitude + M_PI / 2.0) / M_PI;

    vec2 uv = vec2(I, 1.0 - J);

    vec4 cubemapColor = texture(hdriTexture, uv);

    outColor = cubemapColor;
}