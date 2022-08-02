float3 DecodeNormalMap(float2 texVal)
{
    float3 n;
    n.xy = texVal * 2.0 - 1.0;
    float2 xySq = n.xy * n.xy;
    n.z = max(sqrt(1.0 - xySq.x - xySq.y), 0.0);
    n.y = -n.y;
    return n;
}