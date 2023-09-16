#include <GGXModel.hlsl>
#include <hammersley.hlsl>

struct PushConstant
{
    float2 viewportWidthHeight;
};

[[vk::push_constant]] const PushConstant i_pushConstant;

float4 main(
    float4 fragCoord : SV_Position) : SV_Target
{
    // Map current pixel coordinate to [0.f, 1.f].
    float vpWidth = i_pushConstant.viewportWidthHeight[0];
    float vpHeight = i_pushConstant.viewportWidthHeight[1];

    float x = fragCoord[0] / vpWidth;
    float y = fragCoord[1] / vpHeight;

    float roughness = y;
    float NdotV     = x;

    float3 V;
    V.x = sqrt(1.0 - NdotV*NdotV);
    V.y = 0.0;
    V.z = NdotV;

    float A = 0.0;
    float B = 0.0;

    float3 N = float3(0.0, 0.0, 1.0);

    const uint SAMPLE_COUNT = 1024u;

    for(uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H  = ImportanceSampleGGX(Xi, N, roughness);
        float3 L  = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.0);
        float NdotH = max(H.z, 0.0);
        float VdotH = max(dot(V, H), 0.0);

        if(NdotL > 0.0)
        {
            float G = GeometrySmith(N, V, L, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc = pow(1.0 - VdotH, 5.0);

            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }

    A = A / float(SAMPLE_COUNT);
    B = B / float(SAMPLE_COUNT);

    // Sample the cubemap
    return float4(A, B, 0.f, 1.f);
}