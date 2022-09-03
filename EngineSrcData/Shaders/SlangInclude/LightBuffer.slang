import Light;
import AOBox;
import AOSphere;
import Cubemap;

struct LightBuffer
{
    // 0
    float4x4 otherShadowMatrices[4];

    // 256
    uint lightCount;
    uint aoBoxCount;
    uint aoSphereCount;
    uint cubemapCount;

    // 272
    uint shadowmapIds[4];
    float4x4 cascadeMatrices[4];

    // 544
    Light lights[256];
    // 12832
    AOBox aoBox[128];
    // 21024
    AOSphere aoSphere[16];
    // 21280
    uint sphereIds[16];
    // 21344
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
        return rawLightBuffer.Load(272 + idx * 4);
    }

    uint GetLightCount()
    {
        return rawLightBuffer.Load(256);
    }

    Light GetLight(int lightIndex)
    {
        return rawLightBuffer.Load<Light>(544 + LIGHT_SIZE * lightIndex);
    }
    
    uint GetAOBoxCount()
    {
        return rawLightBuffer.Load(260);
    }

    AOBox GetAOBox(int boxIndex)
    {
        return rawLightBuffer.Load<AOBox>(12832 + AOBOX_SIZE * boxIndex);
    }

    uint GetAOSphereCount()
    {
        return rawLightBuffer.Load(264);
    }

    AOSphere GetAOSphere(int sphereIndex)
    {
        return rawLightBuffer.Load<AOSphere>(21024 + AOSPHERE_SIZE * sphereIndex);
    }

    uint GetSphereID(int sphereIndex)
    {
        return rawLightBuffer.Load(21280 + sphereIndex * 4);
    }

    uint GetCubemapCount()
    {
        return rawLightBuffer.Load(268);
    }

    Cubemap GetCubemap(int cubemapIndex)
    {
        return rawLightBuffer.Load<Cubemap>(21344 + CUBEMAP_SIZE * cubemapIndex);
    }
}
