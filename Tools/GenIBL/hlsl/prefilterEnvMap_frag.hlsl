#include <GGXModel.hlsl>
#include <hammersley.hlsl>

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
    float roughness;
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

    float3 N = normal;
    float3 R = N;
    float3 V = R;

    float3 irradiance = float3(0.f, 0.f, 0.f);

    const uint SAMPLE_COUNT = 1024u;
    float totalWeight = 0.0;   
    float3 prefilteredColor = float3(0.f, 0.f, 0.f); 

    for(uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H  = ImportanceSampleGGX(Xi, N, i_cameraInfo.roughness);
        float3 L  = normalize(2.0 * dot(V, H) * H - V);
        float NdotL = max(dot(N, L), 0.0);
        if(NdotL > 0.0)
        {
            float mipLevel = 0.0;
            // Sample from the environment's mip level based on roughness/pdf.
            if(i_cameraInfo.roughness != 0)
            {
                float D = DistributionGGX(N, H, i_cameraInfo.roughness);
                float NoH = saturate( dot( N, H ) );
                float VoH = saturate( dot( V, H ) );
                float pdf = D * NoH / (4.0 * VoH) + 0.0001;

                float resolution = 1024.0;
                float saTexel = 4.0 * PI / (6.0 * resolution * resolution);
                float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);

                mipLevel = 0.5 * log2(saSample / saTexel);
            }

            prefilteredColor += i_cubeMapTexture.SampleLevel(samplerState, L, mipLevel).rgb * NdotL;
            totalWeight      += NdotL;
        }
    }
    prefilteredColor = prefilteredColor / totalWeight;

    return float4(prefilteredColor, 1.f);
}