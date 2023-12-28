// We assume the 2x2x2 box in the world space with camera at the center.
// So, the near distance is always 1.
// The near plane's width and height are always 2.
// We don't have to pass them to the shader. Instead, we can just embed these data in the shader.
// However, we need to pass the screen width and height in pxiel anyway, so I just pass them from the app.
struct CameraInfoUbo
{
    float3 view[6];
    float3 right[6];
    float3 up[6];
    float  near; // Pack it with the following nearWidthHeight.
    float2 nearWidthHeight; // Near plane's width and height in the world.
    float2 viewportWidthHeight; // Screen width and height in the unit of pixels.
};

TextureCube i_cubeMapTexture : register(t0);
SamplerState samplerState : register(s0);

cbuffer UBO0 : register(b1) { CameraInfoUbo i_cameraInfo; }

static const float PI = 3.14159265359;

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

    float3 normalDir = x * (nearWorldWidth / 2.f) * curRight +
                       (-y) * (nearWorldHeight / 2.f) * curUp +
                       curView * i_cameraInfo.near;

    // It's equivalent to the normal of the fragment/point that uses this light map and is sampled.
    float3 normal = normalize(normalDir);

    float3 irradiance = float3(0.f, 0.f, 0.f);

    float3 up = float3(0.f, 1.f, 0.f);
    
    float3 right = normalize(cross(up, normal));
    
    up = normalize(cross(normal, right));

    // NOTE: On low-end GPU, if the sample delta is too small, it can make GPU reset itself, because it spends too much time in the shader.
    // float sampleDelta = 0.025;
    float sampleDelta = 0.05;
    float nrSamples = 0.f;

    for(float phi = 0.f; phi < 2.f * PI; phi += sampleDelta)
    {
        for(float theta = 0.f; theta < 0.5 * PI; theta += sampleDelta)
        {
            // The sample direction in the tangent space
            float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            // Tangent space to world space
            float3 sampleDir = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;
            sampleDir = normalize(sampleDir);

            float3 sampledIrradiance = i_cubeMapTexture.Sample(samplerState, sampleDir).xyz;
            // float3 sampledIrradiance = i_cubeMapTexture.SampleLevel(samplerState, sampleDir, 1.5f).xyz;

            irradiance += (sampledIrradiance * cos(theta) * sin(theta));
            nrSamples += 1.f;
        }
    }
    
    irradiance = PI * irradiance / nrSamples;

    return float4(irradiance, 1.f);
}