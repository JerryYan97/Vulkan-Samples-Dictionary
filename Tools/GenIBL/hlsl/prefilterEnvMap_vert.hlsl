struct VSOutput
{
    float4 Pos : SV_POSITION;
};

static float2 positions[6] =
{
    float2(-1.f, -1.f),
    float2( 1.f, -1.f),
    float2(-1.f,  1.f),
    float2( 1.f, -1.f),
    float2( 1.f,  1.f),
    float2(-1.f,  1.f)
};

VSOutput main(
    uint instanceId : SV_InstanceID)
{
    VSOutput output = (VSOutput)0;
    output.Pos = float4(positions[instanceId], 0.5, 1.0);

    return output;
}