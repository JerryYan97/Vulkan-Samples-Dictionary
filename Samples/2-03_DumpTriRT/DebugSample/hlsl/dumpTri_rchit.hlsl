struct Payload
{
    [[vk::location(0)]] float3 hitValue;
};

[shader("closesthit")]
void main(inout Payload p, in BuiltInTriangleIntersectionAttributes attribs)
{
    const float3 barycentricCoords = float3(1.0f - attribs.barycentrics.x - attribs.barycentrics.y, attribs.barycentrics.x, attribs.barycentrics.y);
    p.hitValue = barycentricCoords;
}