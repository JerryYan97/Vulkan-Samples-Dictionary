struct CameraInfoUbo
{
    float3 view[6];
    float3 right[6];
    float3 up[6];
    float  near; // Pack it with 'up'.
    float2 nearWidthHeight; // Near plane's width and height in the world.
    float2 viewportWidthHeight; // Screen width and height in the unit of pixels.
};

TextureCube i_cubeMapTexture : register(t0);
SamplerState samplerState : register(s0);

cbuffer UBO0 : register(b1) { CameraInfoUbo i_cameraInfo; }

float4 main(
    [[vk::location(0)]] nointerpolation uint viewId : BLENDINDICES0,
    float4 fragCoord : SV_Position) : SV_Target
{
    // Map current pixel coordinate to [-1.f, 1.f].
    float vpWidth = i_cameraInfo.viewportWidthHeight[0];
    float vpHeight = i_cameraInfo.viewportWidthHeight[1];

    float x = ((fragCoord[0] / vpWidth) * 2.f) - 1.f;
    float y = ((fragCoord[1] / vpHeight) * 2.f) - 1.f;

    // Generate the pixel world position on the near plane and its world direction.
    float nearWorldWidth = i_cameraInfo.nearWidthHeight[0];
    float nearWorldHeight = i_cameraInfo.nearWidthHeight[1];

    float3 curRight = i_cameraInfo.right[viewId];
    float3 curView  = i_cameraInfo.view[viewId];
    float3 curUp    = i_cameraInfo.up[viewId];

    float3 sampleDir = x * (nearWorldWidth / 2.f) * curRight +
                       (-y) * (nearWorldHeight / 2.f) * curUp +
                       curView * i_cameraInfo.near;

    sampleDir = normalize(sampleDir);

    // Sample the cubemap
    return i_cubeMapTexture.Sample(samplerState, sampleDir);
}