import Math;

float RoughRemap(float roughness)
{
    float a = roughness + 1;
    a *= a;
    return a / 8.0;
}

float GGXSmithG1(float cosNv, float roughness)
{
    float k = RoughRemap(roughness);

    return cosNv / max(cosNv * (1 - k) + k, 0.000001);
}

float GGXSmithG(float cosNl, float cosNv, float roughness)
{
    return GGXSmithG1(cosNl, roughness) * GGXSmithG1(cosNv, roughness);
}

float GGXNDF(float cosNh, float roughness)
{
    float alpha = roughness * roughness;
    float alphaSq = alpha * alpha;
    float cosNhSq = cosNh * cosNh;

    float partialDenom = (cosNhSq * (alphaSq - 1.0) + 1.0);
    partialDenom *= partialDenom;

    return alphaSq / max(PI * partialDenom, 0.000001);
}

float GGXNDFSphereLight(float cosNh, float roughness, float sphereRadius, float lightDist)
{
    float alpha = roughness * roughness;
    float alphaPrime = saturate(sphereRadius / (lightDist * 2.0) + alpha);
    float alphaSq = alpha * alphaPrime;
    float cosNhSq = cosNh * cosNh;

    float partialDenom = (cosNhSq * (alphaSq - 1.0) + 1.0);
    partialDenom *= partialDenom;

    return alphaSq / max(PI * partialDenom, 0.000001);
}

float3 SchlickFresnel(float cosVh, float3 f0)
{
    return f0 + (1 - f0) * pow5(1.0 - cosVh);
}

// https://seblagarde.wordpress.com/2011/08/17/hello-world/
float3 SchlickFresnelWithRoughness(float cosVh, float3 f0, float roughness)
{
    return f0 + (max(1.0 - roughness, f0) - f0) * pow5(1.0 - cosVh);
}

struct ShadeInfo
{
    float3 albedoColor;
    float metallic;
    float3 normal;
    float roughness;
    float3 viewDir;
    float ao;
};