import LightBuffer;
import Cubemap;
struct MultiVP
{
    float4x4 ViewMatrices[2];
    float4x4 ProjectionMatrices[2];
    float4x4 inverseVP[2];
    float4 viewPos[2];
    int screenWidth;
    int screenHeight;
}

[[vk::binding(0)]]
ConstantBuffer<MultiVP> VPBuffer;

[[vk::binding(3)]]
ByteAddressBuffer RawLightBuffer;

[[vk::binding(0, 1)]]
SamplerCube<float4> Cubemaps[];

struct VertexOutput
{
    float4 Position : SV_Position;
    float3 TexCoord : TexCoord;
}


[shader("vertex")]
VertexOutput VertexShader(uint vertexID : SV_VertexID, uint viewIndex : SV_ViewID)
{
    int tri = vertexID / 3;
    int idx = vertexID % 3;
    int face = tri / 2;
    int top = tri % 2;

    int dir = face % 3;
    int pos = face / 3;

    int nz = dir >> 1;
    int ny = dir & 1;
    int nx = 1 ^ (ny | nz);

    float3 d = float3(nx, ny, nz);
    float flip = 1 - 2 * pos;

    float3 n = flip * d;
    float3 u = -d.yzx;
    float3 v = flip * d.zxy;

    float mirrored = -1 + 2 * top;
    float3 xyz = n + mirrored*(1-2*(idx&1))*u + mirrored*(1-2*(idx>>1))*v;


    float4x4 view = VPBuffer.ViewMatrices[viewIndex];
    view[0] = float4(view[0].xyz, 0);
    view[1] = float4(view[1].xyz, 0);
    view[2] = float4(view[2].xyz, 0);
    view[3] = float4(view[3].xyz, 0);
    float4x4 vp = mul(VPBuffer.ProjectionMatrices[viewIndex], view);

    float4 transformedPos = mul(vp, float4(xyz, 1.0));
    transformedPos.z = 1.0e-9;

    VertexOutput output;
    output.Position = transformedPos.xyww;
    output.Position.z = 0.0;
    output.Position.y = -output.Position.y;
    xyz.x = -xyz.x;
    output.TexCoord = xyz;

    return output;
}

[shader("fragment")]
float4 FragmentShader(VertexOutput input) : SV_TARGET
{
    LightBufferLoader lbl;
    lbl.rawLightBuffer = RawLightBuffer;
    Cubemap c = lbl.GetCubemap(0);
    input.TexCoord.x = -input.TexCoord.x;
    return Cubemaps[c.TextureID].SampleLevel(input.TexCoord, 0);
}