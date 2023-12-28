struct SceneInfo
{
    float4x4 rotMats[6]; // NOTE: Alignment is mat4.
    float2 widthHeight; // Screen width and height.
};

[[vk::binding(0, 0)]] Texture2D    i_hdriTexture;
[[vk::binding(0, 0)]] SamplerState i_hdriTextureSamplerState;

[[vk::binding(1, 0)]] cbuffer UBO0 { SceneInfo i_sceneInfo; }

#define M_PI 3.1415926535897932384626433832795

float4 main(
    [[vk::location(0)]] nointerpolation uint i_viewId : BLENDINDICES0,
    float4 i_fragCoord : SV_Position) : SV_TARGET
{
    float width = i_sceneInfo.widthHeight.x;
    float height = i_sceneInfo.widthHeight.y;

    // Note: The app assumes a 2x2x2 box.
    // Get the point position on the face in perspective image.
    float i = i_fragCoord.x;
    float j = height - i_fragCoord.y;
    float3 p = float3((2.0 * i / width) - 1.0, 1.0, (2.0 * j / height) - 1.0);

    // Rotate the position around different axises to get the desired point position.
    float4x4 rotMat = i_sceneInfo.rotMats[i_viewId];
    float4 p_prime = mul(rotMat, float4(p, 0.0));

    // From a world position, we can derive it's sphereical coordinate system's coordinate.
    float longitude = atan2(p_prime.x, p_prime.y);
    float latitude = atan2(p_prime.z, sqrt(p_prime.x * p_prime.x + p_prime.y * p_prime.y));

    // Transform the sphereical coordinate system's coordinate to UV texture coordinate.
    float I = (longitude + M_PI) / (2.0 * M_PI);
    float J = (latitude + M_PI / 2.0) / M_PI;

    float2 uv = float2(I, 1.0 - J);

    float4 cubemapColor = i_hdriTexture.Sample(i_hdriTextureSamplerState, uv);

    return cubemapColor;
}