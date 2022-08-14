import Math;
import NormalMapUtils;
import PBRShade;
import PBRUtil;
import Light;
import LightBuffer;
import LightTiles;
import Cubemap;

struct VertexInput
{
    [[vk::location(0)]]
    float3 Position : POSITION;
    [[vk::location(1)]]
    float3 Normal : NORMAL;
    [[vk::location(2)]]
    float3 Tangent : TANGENT;
    [[vk::location(3)]]
    float BitangentSign : BITANGENTSIGN;
    [[vk::location(4)]]
    float2 UV : TEXCOORD;
}

struct VertexOutput
{
    float4 Position : SV_Position;
    float3 WorldPosition : WORLDPOS;
    float3 Normal : NORMAL;
    float4 TangentWithSign : TANGENT;
    float2 UV : TEXCOORD;
}

struct MultiVP
{
    float4x4 ViewMatrices[2];
    float4x4 ProjectionMatrices[2];
    float4x4 inverseVP[2];
    float4 viewPos[2];
    int screenWidth;
    int screenHeight;
}

struct PBRMaterial
{
    uint AlbedoTexture;
    uint NormalTexture;
    uint MRATexture;
    float DefaultRoughness;
    float DefaultMetallic;
    float3 EmissiveColor;

    float3 GetNormal(float2 uv, float3x3 tbn)
    {
        float3 n;

        if (NormalTexture != ~0u)
        {
            n = DecodeNormalMap(Textures[NonUniformResourceIndex(NormalTexture)].Sample(uv).xy);
        }
        else
        {
            n = float3(0.0, 0.0, 1.0);
        }

        return mul(tbn, n);
    }

    void LoadPBRInfo(inout ShadeInfo si, float2 uv)
    {
        if (MRATexture == ~0u)
        {
            si.metallic = DefaultMetallic;
            si.roughness = DefaultRoughness;
            si.ao = 1;
            return;
        }

        float3 texVals = Textures[NonUniformResourceIndex(MRATexture)].Sample(uv).xyz;
        si.metallic = texVals.r;
        si.roughness = texVals.g;
        si.ao = texVals.b;
    }
}

ConstantBuffer<MultiVP> VPBuffer;
StructuredBuffer<float4x4> ModelMatrices;
ByteAddressBuffer MaterialBuffer;
ByteAddressBuffer RawLightBuffer;
StructuredBuffer<LightingTile> LightTiles;

[[vk::binding(0, 1)]]
Sampler2D<float4> Textures[];

[[vk::binding(0, 1)]]
SamplerCube<float4> Cubemaps[];

[[vk::push_constant]]
cbuffer PushConstants
{
    uint ModelMatrixID;
    uint MaterialOffset;
    float2 TextureScale;
    float2 TextureOffset;
}

[shader("vertex")]
VertexOutput VertexShader(VertexInput input, uint viewIndex : SV_ViewID)
{
    VertexOutput output;

    float4x4 model = ModelMatrices[ModelMatrixID];
    float4x4 mvp = mul(VPBuffer.ProjectionMatrices[viewIndex], mul(VPBuffer.ViewMatrices[viewIndex], model));
    output.Position = mul(mvp, float4(input.Position, 1.0));
    output.WorldPosition = mul(model, float4(input.Position, 1.0)).xyz;
    output.Normal = normalize(mul(adjoint(model), input.Normal));
    output.TangentWithSign = float4(mul(adjoint(model), input.Tangent).xyz, input.BitangentSign);
    output.UV = (input.UV * TextureScale) + TextureOffset;
    output.Position.y = -output.Position.y;

    return output;
}

float3x3 ConstructTBN(VertexOutput input)
{
    float3 tangent = input.TangentWithSign.xyz;
    float3 bitangent = cross(input.Normal, tangent) * input.TangentWithSign.w;

    return transpose(float3x3(tangent, bitangent, input.Normal));
}

float3 ComputeIncomingLight(ShadeInfo shadeInfo, float2 fragCoord, float3 worldPos)
{
    LightBufferLoader lbl;
    lbl.rawLightBuffer = RawLightBuffer;

    int tileIdxX = int(fragCoord.x / 32);
    int tileIdxY = int(fragCoord.y / 32);

    LightTileInfo lti = CalculateTileInfo(VPBuffer.screenWidth, VPBuffer.screenHeight);

    uint tileIdx = ((tileIdxY * lti.numTilesX) + tileIdxX);

    float3 lo = 0.0f;
    for (int i = 0; i < 8; i++)
    {
        // scalarize light reads if possible
        uint lightBits = WaveReadLaneFirst(WaveActiveBitOr(LightTiles[tileIdx].lightIdMasks[i]));

        while (lightBits != 0)
        {
            // find the next set light bit
            uint lightBitIndex = firstbitlow(lightBits);

            // remove it from the mask with an XOR
            lightBits ^= 1 << lightBitIndex;

            uint realIndex = lightBitIndex + (32 * i);
            lo += calculateLighting(lbl.GetLight(int(realIndex)), shadeInfo, worldPos);
        }
    }

    return lo;
}

// https://www.unrealengine.com/en-US/blog/physically-based-shading-on-mobile
float3 EnvBRDFApprox(float3 specColor, float roughness, float NoV)
{
    const float4 c0 = { -1, -0.0275, -0.572, 0.022 };
    const float4 c1 = { 1, 0.0425, 1.04, -0.04 };

    float4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2( -9.28 * NoV )) * r.x + r.y;
    float2 AB = float2(-1.04, 1.04) * a004 + r.zw;

    return specColor * AB.x + AB.y;
}

float3 sampleCubemap(Cubemap cubemap, float3 dir, float mip, float3 worldPos)
{
    if (cubemap.Flags != CubemapFlags.None)
    {
        dir = parallaxCorrectCubemap(dir, cubemap.Position, cubemap.Position + cubemap.Extent, cubemap.Position - cubemap.Extent, worldPos);
    }

    dir.x = -dir.x;
    dir = normalize(dir);

    if (cubemap.TextureID == ~0u) return pow(float3(0.392f, 0.584f, 0.992f), 2.2);
    return Cubemaps[NonUniformResourceIndex(cubemap.TextureID)].SampleLevel(dir, mip).rgb;
}

float calcBlendWeight(Cubemap c, float3 worldPos)
{
    float3 t = abs(worldPos - c.Position);
    const float blendDist = 1.0;
    t = (t - (c.Extent - float3(blendDist))) / (float3(blendDist));
    return 1.0 - saturate(max(t.x, max(t.y, t.z)));
}

float3 ComputeAmbient(ShadeInfo shadeInfo, float2 fragCoord, float3 worldPos)
{
    const float MAX_REFLECTION_LOD = 5.0;
    float3 R = reflect(-shadeInfo.viewDir, shadeInfo.normal);
    float3 f0 = lerp(0.04, shadeInfo.albedoColor, shadeInfo.metallic);

    float3 specularAmbient = float3(0.0);
    float3 diffuseAmbient = float3(0.0);

    int tileIdxX = int(fragCoord.x / 32);
    int tileIdxY = int(fragCoord.y / 32);

    LightTileInfo lti = CalculateTileInfo(VPBuffer.screenWidth, VPBuffer.screenHeight);
    uint tileIdx = ((tileIdxY * lti.numTilesX) + tileIdxX);

    float specMipLevel = shadeInfo.roughness * MAX_REFLECTION_LOD;

    LightBufferLoader lbl;
    lbl.rawLightBuffer = RawLightBuffer;

    int numCubemaps = 0;
    float weightSum = 0.0;
    for (int i = 0; i < 2; i++)
    {
        uint cubemapBits = LightTiles[tileIdx].cubemapIdMasks[i];

        while (cubemapBits != 0)
        {
            // find the next set sphere bit
            uint cubemapBitIndex = firstbitlow(cubemapBits);

            // remove it from the mask with an XOR
            cubemapBits ^= 1 << cubemapBitIndex;

            uint realIndex = cubemapBitIndex + (32 * i);
            Cubemap c = lbl.GetCubemap(realIndex);
            numCubemaps++;
            weightSum += calcBlendWeight(c, worldPos);
        }
    }

    for (int i = 0; i < 2; i++)
    {
        uint cubemapBits = LightTiles[tileIdx].cubemapIdMasks[i];

        while (cubemapBits != 0)
        {
            // find the next set sphere bit
            uint cubemapBitIndex = firstbitlow(cubemapBits);

            // remove it from the mask with an XOR
            cubemapBits ^= 1 << cubemapBitIndex;

            uint realIndex = cubemapBitIndex + (32 * i);
            Cubemap c = lbl.GetCubemap(realIndex);
            float blendWeight = calcBlendWeight(c, worldPos) / weightSum;
            specularAmbient += sampleCubemap(c, R, specMipLevel, worldPos) * blendWeight;
            diffuseAmbient += sampleCubemap(c, R, 12.0, worldPos) * blendWeight;
        }
    }

    if (weightSum == 0.0)
    {
        specularAmbient = sampleCubemap(lbl.GetCubemap(0), R, specMipLevel, worldPos);
        diffuseAmbient = sampleCubemap(lbl.GetCubemap(0), R, 12.0, worldPos);
    }

    float3 F = SchlickFresnelWithRoughness(saturate(dot(shadeInfo.normal, shadeInfo.viewDir)), f0, shadeInfo.roughness);
    //float2 coord = float2(roughness, max(dot(normal, viewDir), 0.0));
    //float2 brdf = textureLod(brdfLutSampler, coord, 0.0).rg;
    //float3 specularColor = F * (brdf.x + brdf.y);
    
    float3 specularColor = EnvBRDFApprox(F, shadeInfo.roughness, max(dot(shadeInfo.normal, shadeInfo.viewDir), 0.0));

    float3 totalAmbient = specularAmbient * specularColor;

    if (shadeInfo.metallic < 1.0)
    {
        float3 kD = (1.0 - f0) * (1.0 - shadeInfo.metallic);
        totalAmbient += kD * diffuseAmbient * shadeInfo.albedoColor;
    }

    return totalAmbient;
}

uint GetCubemapTileCount(float2 fragCoord)
{
    int tileIdxX = int(fragCoord.x / 32);
    int tileIdxY = int(fragCoord.y / 32);

    LightTileInfo lti = CalculateTileInfo(VPBuffer.screenWidth, VPBuffer.screenHeight);
    uint tileIdx = ((tileIdxY * lti.numTilesX) + tileIdxX);
    uint numCubemaps = 0;
    for (int i = 0; i < 2; i++)
    {
        uint cubemapBits = LightTiles[tileIdx].cubemapIdMasks[i];
        numCubemaps += countbits(cubemapBits);
    }

    return numCubemaps;
}

uint GetLightTileCount(float2 fragCoord)
{
    int tileIdxX = int(fragCoord.x / 32);
    int tileIdxY = int(fragCoord.y / 32);

    LightTileInfo lti = CalculateTileInfo(VPBuffer.screenWidth, VPBuffer.screenHeight);
    uint tileIdx = ((tileIdxY * lti.numTilesX) + tileIdxX);
    uint foundLightCount = 0;
    for (int i = 0; i < 8; i++)
    {
        uint lightBits = LightTiles[tileIdx].lightIdMasks[i];
        foundLightCount += countbits(lightBits);
    }

    return foundLightCount;
}

float3 GetHeatmapColor(uint number, float2 fragCoord)
{
    float3 heatmapCol = float3(saturate((number - 8.0) / 4.0), saturate(number / 4.0), saturate((number - 4.0) / 4.0));

    if (int(fragCoord.x) % 32 == 0 || int(fragCoord.y) % int(32) == 0)
        heatmapCol.z = 1.0;

    return heatmapCol;
}

[shader("fragment")]
float4 FragmentShader(VertexOutput input, float2 fragCoord : SV_Position) : SV_TARGET
{
    PBRMaterial material = MaterialBuffer.Load<PBRMaterial>(MaterialOffset);

    float3x3 tbn = ConstructTBN(input);
    float3 normal = material.GetNormal(input.UV, tbn);

    ShadeInfo shadeInfo;
    shadeInfo.albedoColor = Textures[NonUniformResourceIndex(material.AlbedoTexture)].Sample(input.UV).xyz;
    shadeInfo.normal = normalize(normal);
    shadeInfo.viewDir = normalize(VPBuffer.viewPos[0].xyz - input.WorldPosition.xyz);
    shadeInfo.ao = 1;

    material.LoadPBRInfo(shadeInfo, input.UV);

    float3 lo = ComputeIncomingLight(shadeInfo, fragCoord, input.WorldPosition.xyz);
    float3 ambient = ComputeAmbient(shadeInfo, fragCoord, input.WorldPosition.xyz);

    return float4(ambient * shadeInfo.ao + lo + material.EmissiveColor, 1.0);
    //return float4(GetHeatmapColor(GetLightTileCount(fragCoord), fragCoord), 1.0);
}

[shader("fragment")]
void EmptyFragShader(VertexOutput input)
{
}