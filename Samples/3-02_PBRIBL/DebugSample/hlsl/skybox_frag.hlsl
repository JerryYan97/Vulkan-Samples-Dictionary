struct CameraInfoUbo
{
    float3 view;
    float3 right;
    float3 up;
    float  near; // Pack it with 'up'.
    float2 nearWidthHeight; // Near plane's width and height in the world.
    float2 viewportWidthHeight; // Screen width and height in the unit of pixels.
};

[[vk::binding(0, 0)]] TextureCube i_cubeMapTexture;
[[vk::binding(0, 0)]] SamplerState samplerState;

[[vk::binding(1, 0)]] cbuffer UBO0 { CameraInfoUbo i_cameraInfo; }

float4 main(
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

    float3 sampleDir = x * (nearWorldWidth / 2.f) * i_cameraInfo.right +
                       (-y) * (nearWorldHeight / 2.f) * i_cameraInfo.up +
                       i_cameraInfo.view * i_cameraInfo.near;

    sampleDir = normalize(sampleDir);

    // Sample the cubemap
    return i_cubeMapTexture.Sample(samplerState, sampleDir);
}