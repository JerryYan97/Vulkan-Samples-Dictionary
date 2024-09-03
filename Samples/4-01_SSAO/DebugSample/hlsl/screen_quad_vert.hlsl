#pragma pack_matrix(row_major)

struct VSOutput
{
    float4 Pos      : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

static float2 positions[6] =
{
    float2(-1.f,  1.f),
    float2( 1.f, -1.f),
    float2(-1.f, -1.f),

    float2(-1.f,  1.f),
    float2( 1.f,  1.f),
    float2( 1.f, -1.f)
};

static float2 uvs[6] =
{
    float2(0.f, 0.f),
    float2(1.f, 1.f),
    float2(0.f, 1.f),

    float2(0.f, 0.f),
    float2(1.f, 1.f),
    float2(1.f, 0.f)
};

VSOutput main(
    uint vertId : SV_VertexID)
{
    VSOutput output = (VSOutput)0;
    output.Pos = float4(positions[vertId], 0.5, 1.0);
    output.TexCoord = uvs[vertId];
    return output;
}
