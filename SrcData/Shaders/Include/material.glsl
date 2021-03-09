struct Material {
    float metallic;
    float roughness;
    int albedoTexIdx;
    int normalTexIdx;

    vec3 albedoColor;
    uint cutoffFlags;

    int heightmapIdx;
    float heightScale;
    int metalTexIdx;
    int roughTexIdx;

    vec3 emissiveColor;
    int aoTexIdx;
};
