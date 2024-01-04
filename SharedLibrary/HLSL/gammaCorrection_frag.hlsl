struct RenderInfo
{
	float width;
	float height;
};

[[vk::binding(0, 0)]] Texture2D i_finalRadianceTexture;
[[vk::binding(0, 0)]] SamplerState i_finalRadianceSamplerState;

[[vk::push_constant]] RenderInfo i_renderInfo;

float4 main(
	float4 i_fragCoord : SV_POSITION) : SV_TARGET
{
    float x = i_fragCoord[0] / i_renderInfo.width;
	float y = i_fragCoord[1] / i_renderInfo.height;
	float2 uv = float2(x, y);

    float3 radiance = i_finalRadianceTexture.Sample(i_finalRadianceSamplerState, uv);

    float3 color = radiance / (radiance + float3(1.0, 1.0, 1.0));
    color = pow(color, float3(1.0/2.2, 1.0/2.2, 1.0/2.2));  

    return float4(color, 1.0);
}