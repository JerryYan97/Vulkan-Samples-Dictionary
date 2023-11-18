RaytracingAccelerationStructure tlas : register(t0);
RWTexture2D<float4> image : register(u1);

struct Payload
{
    [[vk::location(0)]] float3 hitValue;
};

// Assume the camera is at the center of the x-y and it looks toward the negative direction of the z axis.
// We shot ray from the pixel's world position to the negative z direction.
// The ray shotting is from x = [-1, 1], y = [-1, 1]

[shader("raygeneration")]
void main()
{
    const uint3 LaunchID = DispatchRaysIndex();
    const uint3 LaunchSize = DispatchRaysDimensions();

    const float2 pixelCenter = float2(LaunchID.xy) + float2(0.5, 0.5);
    const float2 pixelOffset = float2(LaunchSize.xy) / 2.0;

    const float3 pixelWorldPos = float3((pixelCenter - pixelOffset) / pixelOffset, 1.0); 

    RayDesc rayDesc;
    rayDesc.Origin = pixelWorldPos;
    rayDesc.Direction = float3(0.0, 0.0, -1.0);
    rayDesc.TMin = 0.001;
    rayDesc.TMax = 10000.0;

    Payload payload;
    TraceRay(tlas, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, rayDesc, payload);
    
    image[int2(LaunchID.xy)] = float4(payload.hitValue, 1.0);
}