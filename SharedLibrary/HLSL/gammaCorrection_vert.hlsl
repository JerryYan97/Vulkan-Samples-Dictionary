struct VSOutput
{
    float4 Pos : SV_POSITION;
    float2 UV  : SV_TEXCOORD0;
};

static float4 verts[6] =
{
    float4(-1.f,  1.f, 0.f, 0.f),
    float2( 1.f, -1.f),
    float2(-1.f, -1.f),
    float2(-1.f,  1.f),
    float2( 1.f,  1.f),
    float2( 1.f, -1.f)
};

VSOutput main(
    uint vertId : SV_VertexID)
{
    VSOutput output = (VSOutput)0;
    output.Pos = float4(positions[vertId], 0.5, 1.0);
    return output;
}