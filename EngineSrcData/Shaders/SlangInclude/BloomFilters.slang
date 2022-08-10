float4 Downsample13(float2 uv, float2 resolution)
{
    float2 texelSize = 2.0 / resolution;
    
    // A  B  C
    //  D  E
    // F  G  H
    //  I  J
    // K  L  M
    
    // Samples going clockwise from the top left corner
    
    float4 a = samp(uv - float2(-1.0,  1.0) * texelSize);
    float4 b = samp(uv - float2( 0.0,  1.0) * texelSize);
    float4 c = samp(uv - float2( 1.0,  1.0) * texelSize);
    
    float4 d = samp(uv - float2(-0.5,  0.5) * texelSize);
    float4 e = samp(uv - float2( 0.5,  0.5) * texelSize);
    
    float4 f = samp(uv - float2(-1.0,  0.0) * texelSize);
    float4 g = samp(uv - float2( 0.0,  0.0) * texelSize);
    float4 h = samp(uv - float2( 1.0,  0.0) * texelSize);
    
    float4 i = samp(uv - float2(-0.5, -0.5) * texelSize);
    float4 j = samp(uv - float2( 0.5, -0.5) * texelSize);
    
    float4 k = samp(uv - float2(-1.0, -1.0) * texelSize);
    float4 l = samp(uv - float2( 0.0, -1.0) * texelSize);
    float4 m = samp(uv - float2( 1.0, -1.0) * texelSize);
    
    float sampleWeight = 0.25;
    
    // slides 154-158
    float4 final =
        (d + e + i + j) * sampleWeight * 0.5
      + (a + b + f + g) * sampleWeight * 0.125
      + (b + c + g + h) * sampleWeight * 0.125
      + (g + h + l + m) * sampleWeight * 0.125
      + (f + g + k + l) * sampleWeight * 0.125;

    return final;
}

float4 UpsampleTent(float2 uv, float2 resolution)
{
    const float radius = 1.0;
    float2 xOffset = float2(1.0 / resolution.x, 0.0) * radius;
    float2 yOffset = float2(0.0, 1.0 / resolution.y) * radius;
    float4 color = float4(0.0);
    
    color += samp(uv) * 4.0;

    color += samp(uv + xOffset) * 2.0;
    color += samp(uv - xOffset) * 2.0;

    color += samp(uv + yOffset) * 2.0;
    color += samp(uv - yOffset) * 2.0;

    color += samp(uv + xOffset + yOffset);
    color += samp(uv + xOffset - yOffset);
    color += samp(uv - xOffset + yOffset);
    color += samp(uv - xOffset - yOffset);
    
    color /= 16.0;

    return color;
}