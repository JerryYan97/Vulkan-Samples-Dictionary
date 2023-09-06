Texture2DArray cubemapTextures : register(t0);
SamplerState samplerState : register(s0);

cbuffer UBO0 : register(b1) { float2 vpWidthHeight; }

float4 main(
    [[vk::location(0)]] nointerpolation uint viewId : BLENDINDICES0,
    float4 fragCoord : SV_Position) : SV_Target
{
    float width =  vpWidthHeight[0];
    float height = vpWidthHeight[1];

    float2 uv = float2(fragCoord[0] / width, fragCoord[1] / height);

    if((viewId == 0) || (viewId == 1) || (viewId == 4) || (viewId == 5))
    {
        uv.x = 1.0 - uv.x;
    }
    else
    {
        // TOP
        if(viewId == 2)
        {
            // Swap
            float tmp = uv.x;
            uv.x = uv.y;
            uv.y = tmp;
        }
        // BOTTOM
        else
        {
            // Swap
            float tmp = uv.x;
            uv.x = uv.y;
            uv.y = tmp;
            // Flip both uv
            uv.x = 1.0 - uv.x;
            uv.y = 1.0 - uv.y;
        }
    }

    return cubemapTextures.Sample(samplerState, float3(uv, viewId));
}