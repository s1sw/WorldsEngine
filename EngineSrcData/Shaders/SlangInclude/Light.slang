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

        set
        {
            uint wVal = asuint(pack0.w);
            wVal &= ~7;
            wVal |= uint(newValue);
            pack0.w = asfloat(wVal);
        }
    }

    property uint ShadowmapIndex
    {
        get
        {
            return (asuint(pack0.w) >> 3) & 0xF;
        }
    }

    property float3 Radiance
    {
        get
        {
            return pack0.xyz;
        }

        set
        {
            pack0.xyz = newValue;
        }
    }

    property float3 DirectionalDir
    {
        get
        {
            return pack1.xyz;
        }

        set
        {
            pack1.xyz = newValue;
        }
    }

    property float SpotCutoff
    {
        get
        {
            return pack1.w;
        }

        set
        {
            pack1.w = newValue;
        }
    }

    property float SpotOuterCutoff
    {
        get
        {
            return (float)((asuint(pack0.w) >> 7) & 0xFF) / 255.0;
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