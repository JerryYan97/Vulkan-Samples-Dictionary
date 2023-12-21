[[vk::binding(1, 0)]] StructuredBuffer<float4> colorStorageData;

float4 main(
  uint instId : BLENDINDICES0) : SV_Target
{
  return colorStorageData[instId];
}