enum LightType
{
    Point = 0,
    Spot = 1,
    Directional = 2,
    Sphere = 3,
    Tube = 4
};

static const int LIGHT_SIZE = 48;
struct Light
{
    float4 pack0;
    float4 pack1;
    float3 pack2;
    float DistanceCutoff;

    property LightType Type
    {
        get
        {
            return LightType(asuint(pack0.w) & 7);
        }
    }

    property uint ShadowmapIndex
    {
        get
        {
            return (asuint(pack0.w) >> 3) & 0xF;
        }
    }

    float3 GetDirection(float3 worldPos)
    {
        switch(Type)
        {
        case LightType.Point:
        case LightType.Spot:
        case LightType.Sphere:
        case LightType.Tube:
            return pack2.xyz - worldPos;
        case LightType.Directional:
            return pack1.xyz;
        }

        return float3(0.0, 0.0, 0.0);
    }
};