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
    [vk::location(0)]
    float3 Position : POSITION;
    [vk::location(1)]
    float3 Normal : NORMAL;
    [vk::location(2)]
    float3 Tangent : TANGENT;
    [vk::location(3)]
    float BitangentSign : BITANGENTSIGN;
    [vk::location(4)]
    float2 UV : TEXCOORD;
}

struct VertexOutput
{
    float4 Position : SV_Position;
    float3 WorldPosition : WORLDPOS;
    float3 Normal : NORMAL;
    float4 TangentWithSign : TANGENT;
    float2 UV : TEXCOORD;
    nointerpolation uint DrawID : DRAWID;
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
    uint Flags;

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

struct DrawInfo
{
    uint MaterialOffset;
    uint ModelMatrixID;
    float2 TextureOffset;
    float2 TextureScale;
}

ConstantBuffer<MultiVP> VPBuffer;
StructuredBuffer<float4x4> ModelMatrices;
ByteAddressBuffer MaterialBuffer;
ByteAddressBuffer RawLightBuffer;
StructuredBuffer<LightingTile> LightTiles;
StructuredBuffer<DrawInfo> DrawInfos;

cbuffer SceneGlobals 
{
    float time;
    float shadowmapResolution;
    float2 PoissonKernel[64];
};

[vk::binding(0, 1)]
[allow("parameterBindingsOverlap")]
Sampler2D<float4> Textures[];

[vk::binding(0, 1)]
[allow("parameterBindingsOverlap")]
SamplerCube<float4> Cubemaps[];


[shader("vertex")]
VertexOutput VertexShader(VertexInput input, uint viewIndex : SV_ViewID, uint drawIndex : SV_DrawID)
{
    VertexOutput output;
    DrawInfo di = DrawInfos[drawIndex];

    float4x4 model = ModelMatrices[di.ModelMatrixID];
    float4x4 mvp = mul(VPBuffer.ProjectionMatrices[viewIndex], mul(VPBuffer.ViewMatrices[viewIndex], model));
    output.Position = mul(mvp, float4(input.Position, 1.0));
    output.WorldPosition = mul(model, float4(input.Position, 1.0)).xyz;
    output.Normal = normalize(mul(adjoint(model), input.Normal));
    output.TangentWithSign = float4(mul(adjoint(model), input.Tangent).xyz, input.BitangentSign);
    output.UV = (input.UV * di.TextureScale) + di.TextureOffset;
    output.Position.y = -output.Position.y;
    output.DrawID = drawIndex;

    return output;
}

float3x3 ConstructTBN(VertexOutput input)
{
    float3 tangent = input.TangentWithSign.xyz;
    float3 bitangent = cross(input.Normal, tangent) * input.TangentWithSign.w;

    return transpose(float3x3(tangent, bitangent, input.Normal));
}

float hardenedKernel(float x)
{
    // this is basically a stronger smoothstep()
    x = 2.0 * x - 1.0;
    float s = sign(x);
    x = 1.0 - s * x;
    x = x * x * x;
    x = s - x * s;
    return 0.5 * x + 0.5;
}

float random(float2 p)
{
    float2 K1 = float2(
        23.14069263277926, // e^pi (Gelfond's constant)
        2.665144142690225 // 2^sqrt(2) (Gelfond-Schneider constant)
    );
    return frac( cos( dot(p,K1) ) * 12345.6789 );
}

float2 rotateSample(float2 inSample, float2 rotComponents)
{
    float sinTheta = rotComponents.x;
    float cosTheta = rotComponents.y;

    float2 outSample;
    outSample.x = inSample.x * cosTheta - inSample.y * sinTheta;
    outSample.y = inSample.x * sinTheta + inSample.y * cosTheta;
    return outSample;
}

float2 getRotationComponents(float angle)
{
    return float2(sin(angle), cos(angle));
}

float3 ComputeIncomingLight(ShadeInfo shadeInfo, float2 fragCoord, float3 worldPos, uint viewIndex)
{
    LightBufferLoader lbl;
    lbl.rawLightBuffer = RawLightBuffer;

    int tileIdxX = int(fragCoord.x / 32);
    int tileIdxY = int(fragCoord.y / 32);

    LightTileInfo lti = CalculateTileInfo(VPBuffer.screenWidth, VPBuffer.screenHeight);

    uint tileIdx = ((tileIdxY * lti.numTilesX) + tileIdxX) + (viewIndex * lti.tilesPerEye);

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
            Light light = lbl.GetLight(int(realIndex));
            float3 contribution = calculateLighting(light, shadeInfo, worldPos);

            if (light.ShadowmapIndex != 0xF)
            {
                float4x4 shadowVP = transpose(lbl.GetShadowMatrix(int(light.ShadowmapIndex)));
                float4 shadowPos = mul(shadowVP, float4(worldPos, 1.0));
                shadowPos.y = -shadowPos.y;
                shadowPos /= shadowPos.w;
                float2 coord = (shadowPos.xy * 0.5) + 0.5;

                if (all(coord.xy > 0.0) && all(coord.xy < 1.0))
                {
                    float occlusionAmount = 0.0;
                    uint shadowTexIdx = NonUniformResourceIndex(lbl.GetShadowmapId(int(light.ShadowmapIndex)));

                    // estimate blocker distance
                    float avgBlockDist = 0.0;
                    int blockCount = 0;

                    const bool jitterSamples = false;
                    int sampleCount = 8;
                    float penumbraSize = 0.5;

                    [[loop]]
                    for (int t = 0; t < sampleCount; t++)
                    {
                        float2 offset = ((PoissonKernel[t] * 2.0) - 1.0) * (1.0 / 512.0) * penumbraSize;
                        if (jitterSamples)
                        {
                            float2 rotationComponents = getRotationComponents((random(fragCoord) * 2.0 - 1.0) * 1.5);
                            offset = rotateSample(offset, rotationComponents);
                        }
                        float2 offsetCoord = coord + offset;
                        float4 depths = Textures[shadowTexIdx].Gather(offsetCoord);
                        float4 occlusions = float4(0.0);

                        for (int i = 0; i < 4; i++)
                        {
                            float d = depths[i];
                            //float d = Textures[shadowTexIdx].SampleLevel(offsetCoord, 0).x;
                            float dist = (d - shadowPos.z);

                            float bias = 0.000025;
                            occlusions[i] = step(dist, bias);

                            avgBlockDist += dist * occlusions[i];
                            blockCount += (dist >= bias ? 1 : 0);
                        }

                        // bilinearly interpolate the occlusions
                        float2 fr = frac((offsetCoord * shadowmapResolution) - 0.5);
                        float tr = lerp(occlusions.x, occlusions.y, fr.x);
                        float br = lerp(occlusions.w, occlusions.z, fr.x);
                        float final = lerp(br, tr, fr.y);
                        occlusionAmount += final;
                    }

                    const bool fancy = true;
                    if (fancy && blockCount > 0)
                    {
                        avgBlockDist /= float(blockCount);
                        occlusionAmount /= float(sampleCount);
                        float recv = shadowPos.z;
                        float block = avgBlockDist;
                        float nofmn = (0.05 / 100.0) - 0.05;

                        float blendAmount = saturate(avgBlockDist / recv);//saturate((nofmn + block) / (nofmn + recv) - 1.0);
                        contribution *= float3(saturate(lerp(hardenedKernel(occlusionAmount), occlusionAmount, blendAmount)));
                    }
                    else
                    {
                        contribution *= occlusionAmount / float(sampleCount);
                    }
                }
            }

            lo += contribution;
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
    float blendDist = c.BlendDistance;
    t = (t - (c.Extent - float3(blendDist))) / (float3(blendDist));
    return 1.0 - saturate(max(t.x, max(t.y, t.z)));
}

float3 ComputeAmbient(ShadeInfo shadeInfo, float2 fragCoord, float3 worldPos, uint viewIndex)
{
    const float MAX_REFLECTION_LOD = 5.0;
    float3 R = reflect(-shadeInfo.viewDir, shadeInfo.normal);
    float3 f0 = lerp(0.04, shadeInfo.albedoColor, shadeInfo.metallic);

    float3 specularAmbient = float3(0.0);
    float3 diffuseAmbient = float3(0.0);

    int tileIdxX = int(fragCoord.x / 32);
    int tileIdxY = int(fragCoord.y / 32);

    LightTileInfo lti = CalculateTileInfo(VPBuffer.screenWidth, VPBuffer.screenHeight);
    uint tileIdx = ((tileIdxY * lti.numTilesX) + tileIdxX) + (viewIndex * lti.tilesPerEye);

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
            Cubemap c = lbl.GetCubemap(int(realIndex));
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
            Cubemap c = lbl.GetCubemap(int(realIndex));
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
[earlydepthstencil]
float4 FragmentShader(VertexOutput input, float2 fragCoord : SV_Position, uint viewIndex : SV_ViewID) : SV_TARGET
{
    DrawInfo di = DrawInfos[WaveReadLaneFirst(input.DrawID)];
    PBRMaterial material = MaterialBuffer.Load<PBRMaterial>(int(di.MaterialOffset));


    float3x3 tbn = ConstructTBN(input);
    float3 normal = material.GetNormal(input.UV, tbn);

    ShadeInfo shadeInfo;
    if (material.AlbedoTexture != ~0u)
        shadeInfo.albedoColor = Textures[NonUniformResourceIndex(material.AlbedoTexture)].Sample(input.UV).xyz;
    else
        shadeInfo.albedoColor = float3(1.0, 0.0, 1.0);
    shadeInfo.normal = normalize(normal);
    shadeInfo.viewDir = normalize(VPBuffer.viewPos[0].xyz - input.WorldPosition.xyz);
    shadeInfo.ao = 1;

    material.LoadPBRInfo(shadeInfo, input.UV);

    float3 lo = ComputeIncomingLight(shadeInfo, fragCoord, input.WorldPosition.xyz, viewIndex);
    float3 ambient = ComputeAmbient(shadeInfo, fragCoord, input.WorldPosition.xyz, viewIndex);

    if ((material.Flags & 1) == 1)
    {
        material.EmissiveColor *= shadeInfo.albedoColor;
    }

    return float4(ambient * shadeInfo.ao + lo + material.EmissiveColor, 1.0);
    //return float4(ambient * shadeInfo.ao + material.EmissiveColor, 1.0);
    //return float4(GetHeatmapColor(GetCubemapTileCount(fragCoord), fragCoord), 1.0);
    //return float4(GetHeatmapColor(GetLightTileCount(fragCoord), fragCoord), 1.0);
}

[shader("fragment")]
void EmptyFragShader(VertexOutput input)
{
}