struct Payload
{
    [[vk::location(0)]] float3 hitValue;
};

[shader("miss")]
void main(inout Payload p)
{
    p.hitValue = float3(1.0, 1.0, 1.0);
}