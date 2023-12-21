struct VSInput
{
    [[vk::location(0)]] float3 Pos : POSITION0;
};

struct VSOutput
{
	float4 Pos : SV_POSITION;
    nointerpolation uint instId : BLENDINDICES0;
};

[[vk::binding(0, 0)]] StructuredBuffer<float2> offsetStorageData;

VSOutput main(
    uint instId : SV_InstanceID,
    VSInput input)
{
    VSOutput output = (VSOutput)0;

    float2 offset = offsetStorageData[instId];

    output.Pos = float4(input.Pos, 1.0);
    output.Pos.x += offset.x;
    output.Pos.y += offset.y;

    return output;
}
