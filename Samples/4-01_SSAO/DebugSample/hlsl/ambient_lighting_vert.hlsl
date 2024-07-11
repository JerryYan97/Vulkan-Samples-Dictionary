#pragma pack_matrix(row_major)



static float2 positions[6] =
{
    float2(-1.f,  1.f),
    float2( 1.f, -1.f),
    float2(-1.f, -1.f),
    float2(-1.f,  1.f),
    float2( 1.f,  1.f),
    float2( 1.f, -1.f)
};

struct VSOutput
{
    float4 Pos : SV_POSITION;
};