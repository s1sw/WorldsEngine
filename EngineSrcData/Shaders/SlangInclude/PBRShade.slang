import PBRUtil;
import Light;

struct LightShadeInfo
{
    float3 radiance;
    float3 L;
    float lightDist;
};

float length2(float3 v)
{
    return dot(v, v);
}

float calculateFalloff(float pixelDist, float cutoffDist)
{
    return max((1.0 / (pixelDist * pixelDist)) * (1.0 - (pixelDist / cutoffDist)), 0.0);
}

float diffuseAA(float3 N, float3 l)
{
    float a = dot(N, l);
    float w = max(length(N), 0.95);
    float x = sqrt(1.0 - w);
    float x0 = 0.37837 * a;
    float x1 = 0.66874 * x;
    float n = x0 + x1;
    return w * ((abs(x0) <= x1) ? n * n / x : saturate(a));
}

LightShadeInfo calcLightShadeInfo(Light light, ShadeInfo shadeInfo, float3 worldPos)
{
    LightShadeInfo lsi;
    lsi.radiance = light.Radiance;
    lsi.L = float3(0.0f, 0.0f, 0.0f);

    if (light.Type == LightType.Directional)
    {
        lsi.L = normalize(light.pack1.xyz);
    } 
    else if (light.Type == LightType.Point)
    {
        float3 lightPos = light.pack2.xyz;
        lsi.L = lightPos - worldPos;

        // dot(L, L) = length(L) squared
        float cutoffDist = light.DistanceCutoff;
        lsi.radiance *= calculateFalloff(length(lsi.L), cutoffDist);

        lsi.L = normalize(lsi.L);
    }
    else if (light.Type == LightType.Spot)
    {
        float cutoff = light.SpotCutoff;
        float outerRadius = light.SpotOuterCutoff;
        float3 lightPos = light.pack2.xyz;

        lsi.L = normalize(lightPos - worldPos);

        float theta = dot(lsi.L, normalize(light.pack1.xyz));
        float3 lToFrag = lightPos - worldPos;
        lsi.radiance *= clamp((theta - outerRadius) / (cutoff - outerRadius), 0.0f, 1.0f);
        float distSq = dot(lToFrag, lToFrag);

        float cutoffDist = light.DistanceCutoff;
        lsi.radiance *= calculateFalloff(length(lToFrag), cutoffDist);
    }
    else if (light.Type == LightType.Tube)
    {
        float3 p0 = light.pack1.xyz;
        float3 p1 = light.pack2.xyz;
        float3 center = (p0 + p1) * 0.5;
        float3 r = reflect(-shadeInfo.viewDir, shadeInfo.normal);
        float tubeRadius = light.pack1.w;

        float3 l0 = p0 - worldPos;
        float3 l1 = p1 - worldPos;

        float3 Ldist = l1 - l0;
        float RoLd = dot( r, Ldist);
        float distLd2 = dot(Ldist, Ldist);
        float t = ( dot( r, l0 ) * RoLd - dot( l0, Ldist) ) / ( distLd2 - RoLd * RoLd );

        float3 closestPoint = l0 + Ldist * saturate(t);
        float3 centerToRay = dot(closestPoint, r) * r - closestPoint;
        closestPoint = closestPoint + centerToRay * saturate(tubeRadius / length(centerToRay));
        float3 L = normalize(closestPoint);
        float distLight = length(closestPoint);

        lsi.L = L;
        float falloff = tubeRadius * tubeRadius / max(tubeRadius * tubeRadius, distLight * distLight);
        float cutoffDist = light.DistanceCutoff;

        falloff *= max((1.0f - (distance(center, worldPos) / cutoffDist)), 0.0);
        lsi.radiance *= falloff;
        lsi.lightDist = distLight;
    }
    else if (light.Type == LightType.Sphere)
    {
        float3 lightPos = light.pack2.xyz;
        float sphereRadius = light.pack1.w;
        float sphereRadiusSq = sphereRadius * sphereRadius;

        float3 r = reflect(-shadeInfo.viewDir, shadeInfo.normal);
        
        lsi.L = lightPos - worldPos;

        float3 centerToRay = (dot(lsi.L, r) * r) - lsi.L;
        float3 closestPoint = lsi.L + centerToRay * saturate(sphereRadius / length(centerToRay));
        lsi.L = normalize(closestPoint);
        float lightDist = length(closestPoint);
        float sqrDist = lightDist * lightDist;
        float falloff = (sphereRadiusSq / (max(sphereRadiusSq, sqrDist)));
        falloff *= max((1.0 - (distance(lightPos, worldPos) / (light.DistanceCutoff))), 0.0);

        lsi.radiance *= falloff;
        lsi.lightDist = lightDist;
    }


    return lsi;
}

float3 calculateLightingDiffuse(Light light, ShadeInfo shadeInfo, float3 worldPos)
{
    LightShadeInfo lsi = calcLightShadeInfo(light, shadeInfo, worldPos);

    float3 halfway = normalize(shadeInfo.viewDir + lsi.L);
    float3 norm = shadeInfo.normal;
    float cosLh = max(0.0f, dot(norm, halfway));
#ifdef DIFFUSE_AA
    float cosLi = max(0.0f, diffuseAA(norm, lsi.L));
#else
    float cosLi = max(0.0f, dot(norm, lsi.L));
#endif

    float cosLo = max(0.0f, dot(norm, shadeInfo.viewDir));
    float3 f0 = lerp(float3(0.04), shadeInfo.albedoColor, shadeInfo.metallic);

    float3 f = SchlickFresnel(max(dot(norm, shadeInfo.viewDir), 0.0f), f0);
    f *= SchlickFresnel(max(dot(norm, lsi.L), 0.0f), f0);

    float3 kd = lerp(float3(1.0) - f, float3(0.25), shadeInfo.metallic);
    float3 diffuse = kd * shadeInfo.albedoColor;

    return diffuse * lsi.radiance * cosLi;
}

float3 calculateLighting(Light light, ShadeInfo shadeInfo, float3 worldPos)
{
    LightShadeInfo lsi = calcLightShadeInfo(light, shadeInfo, worldPos);

    float3 halfway = normalize(shadeInfo.viewDir + lsi.L);
    float3 norm = shadeInfo.normal;
    float cosLh = max(0.0f, dot(norm, halfway));
#ifdef DIFFUSE_AA
    float cosLi = max(0.0f, diffuseAA(norm, lsi.L));
#else
    float cosLi = max(0.0f, dot(norm, lsi.L));
#endif

    float cosLo = max(0.0f, dot(norm, shadeInfo.viewDir));

    float NDF;
    
    if (light.Type == LightType.Tube || light.Type == LightType.Sphere)
    {
        NDF = GGXNDFSphereLight(cosLh, shadeInfo.roughness, light.pack1.w, lsi.lightDist);
    }
    else
    {
        NDF = GGXNDF(cosLh, shadeInfo.roughness);
    }

    float3 f0 = lerp(float3(0.04), shadeInfo.albedoColor, shadeInfo.metallic);

    float G = GGXSmithG(cosLi, cosLo, shadeInfo.roughness);
    float3 f = SchlickFresnel(max(dot(norm, shadeInfo.viewDir), 0.0f), f0);
    f *= SchlickFresnel(max(dot(norm, lsi.L), 0.0f), f0);

    float3 kd = lerp(1.0 - f, 0.0, shadeInfo.metallic);

    float3 numerator = NDF * G * SchlickFresnel(max(dot(halfway, norm), 0.0f), f0);
    float denominator = 4.0f * cosLo * cosLi;
    float3 specular = numerator / max(denominator, 0.001f);
    float3 diffuse = kd * shadeInfo.albedoColor;

    return (diffuse + specular) * lsi.radiance * cosLi;
}