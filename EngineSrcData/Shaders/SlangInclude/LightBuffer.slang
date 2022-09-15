import Light;
import AOBox;
import AOSphere;
import Cubemap;

struct LightBuffer
{
    // 0
    float4x4 otherShadowMatrices[16];

    // 1024
    uint lightCount;
    // 1028
    uint aoBoxCount;
    // 1032
    uint aoSphereCount;
    // 1036
    uint cubemapCount;

    // 1040
    uint shadowmapIds[16];
    // 1104
    float4x4 cascadeMatrices[4];

    // 1360
    Light lights[256];
    // 13648
    AOBox aoBox[128];
    // 21840
    AOSphere aoSphere[16];
    // 22096
    uint sphereIds[16];
    // 22160
    Cubemap cubemaps[64];
}

// HLSL can't load individual fields from a buffer, so we
// have to do alllllll of this to load just what we need
// from a ByteAddressBuffer otherwise performance is terrible
struct LightBufferLoader
{
    ByteAddressBuffer rawLightBuffer;

    float4x4 GetShadowMatrix(int idx)
    {
        return rawLightBuffer.Load<float4x4>(idx * 64);
    }

    uint GetShadowmapId(int idx)
    {
        return rawLightBuffer.Load(1040 + idx * 4);
    }

    uint GetLightCount()
    {
        return rawLightBuffer.Load(1024);
    }

    Light GetLight(int lightIndex)
    {
        return rawLightBuffer.Load<Light>(1360 + LIGHT_SIZE * lightIndex);
    }
    
    uint GetAOBoxCount()
    {
        return rawLightBuffer.Load(1028);
    }

    AOBox GetAOBox(int boxIndex)
    {
        return rawLightBuffer.Load<AOBox>(13648+ AOBOX_SIZE * boxIndex);
    }

    uint GetAOSphereCount()
    {
        return rawLightBuffer.Load(1032);
    }

    AOSphere GetAOSphere(int sphereIndex)
    {
        return rawLightBuffer.Load<AOSphere>(21840 + AOSPHERE_SIZE * sphereIndex);
    }

    uint GetSphereID(int sphereIndex)
    {
        return rawLightBuffer.Load(22096 + sphereIndex * 4);
    }

    uint GetCubemapCount()
    {
        return rawLightBuffer.Load(1036);
    }

    Cubemap GetCubemap(int cubemapIndex)
    {
        return rawLightBuffer.Load<Cubemap>(22160 + CUBEMAP_SIZE * cubemapIndex);
    }
}
