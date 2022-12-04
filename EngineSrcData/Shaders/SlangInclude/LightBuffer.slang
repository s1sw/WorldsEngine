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
    uint cubemapCount;

    // 1032
    uint shadowmapIds[16];
    // 1096
    float4x4 cascadeMatrices[4];

    // 1352
    Light lights[256];
    // 14664
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
        return rawLightBuffer.Load(1032 + idx * 4);
    }

    uint GetLightCount()
    {
        return rawLightBuffer.Load(1024);
    }

    Light GetLight(int lightIndex)
    {
        return rawLightBuffer.Load<Light>(1352 + LIGHT_SIZE * lightIndex);
    }

    uint GetCubemapCount()
    {
        return rawLightBuffer.Load(1028);
    }

    Cubemap GetCubemap(int cubemapIndex)
    {
        return rawLightBuffer.Load<Cubemap>(14664 + CUBEMAP_SIZE * cubemapIndex);
    }
}
