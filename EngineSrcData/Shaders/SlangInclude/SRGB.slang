// adapted from https://stackoverflow.com/a/34480536
float3 ConvertToSRGB(float3 col)
{
    bool3 cutoff = col > 0.0031308;

    float3 above = 1.055 * (pow(col, float3(1.0 / 2.4))) - 0.055;
    float3 below = 12.92 * col;

    return lerp(below, above, cutoff);
}

float3 ConvertFromSRGB(float3 col)
{
    bool3 cutoff = col > 0.0404482;

    float3 above = pow(((col + 0.055) / 1.055), float3(2.4));
    float3 below = col / 12.92;

    return lerp(below, above, cutoff);
}