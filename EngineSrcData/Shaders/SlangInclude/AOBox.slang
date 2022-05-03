float4x4 CreateMatrixFromCols(float4 c0, float4 c1, float4 c2, float4 c3) {
    return float4x4(c0.x, c1.x, c2.x, c3.x,
                    c0.y, c1.y, c2.y, c3.y,
                    c0.z, c1.z, c2.z, c3.z,
                    c0.w, c1.w, c2.w, c3.w);
}

static const int AOBOX_SIZE = 64;
struct AOBox
{   
    float4 pack0;
    float4 pack1;
    float4 pack2;
    float4 pack3;

    property float3x3 RotationMatrix
    {
        get
        {
            return float3x3(
                pack0.x, pack0.y, pack0.z,
                pack0.w, pack1.x, pack1.y,
                pack1.z, pack1.w, pack2.x
            );
        }
    }

    property float3 Translation
    {
        get
        {
            return float3(pack2.y, pack2.z, pack2.w);
        }
    }

    property float3 Scale
    {
        get
        {
            return pack3.xyz;
        }
    }

    property float4x4 InverseTransform
    {
        get
        {
            float4x4 rot = CreateMatrixFromCols(
                float4(RotationMatrix[0], 0.0),
                float4(RotationMatrix[1], 0.0),
                float4(RotationMatrix[2], 0.0),
                float4(0.0, 0.0, 0.0, 1.0)
            );
            
            float4x4 translationMatrix = float4x4(
                1.0, 0.0, 0.0, 0.0,
                0.0, 1.0, 0.0, 0.0,
                0.0, 0.0, 1.0, 0.0,
                Translation.x, Translation.y, Translation.z, 1.0
            );

            return mul(translationMatrix, rot);
        }
    }
}

float CalculateBoxOcclusion(AOBox box, float3 pos, float3 normal)
{
    float3 boxSize = box.Scale;
    float3x3 rotation = box.RotationMatrix;

    float3 boxCenter = box.Translation;
    float3 dv = pos - boxCenter;

    float3 xAxis = mul(rotation, float3(1.0, 0.0, 0.0));
    float3 yAxis = mul(rotation, float3(0.0, 1.0, 0.0));
    float3 zAxis = mul(rotation, float3(0.0, 0.0, 1.0));

    float xDist = dot(xAxis, dv);
    float yDist = dot(yAxis, dv);
    float zDist = dot(zAxis, dv);

    xDist = clamp(xDist, -boxSize.x, boxSize.x);
    yDist = clamp(yDist, -boxSize.y, boxSize.y);
    zDist = clamp(zDist, -boxSize.z, boxSize.z);

    float3 point = boxCenter + (xAxis * xDist) + (yAxis * yDist) + (zAxis * zDist);
    float d = distance(point, pos);

    float3 dirToPoint = normalize(point - pos);

    return saturate(pow(1.0f / 1.0 - d, 5.0)) * smoothstep(0.0, 1.0, pow(dot(dirToPoint, normal), 0.25));
}