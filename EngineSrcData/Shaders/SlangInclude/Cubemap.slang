enum class CubemapFlags
{
    None = 0,
    Parallax = 1
}

static const int CUBEMAP_SIZE = 32;
struct Cubemap
{
    float3 Extent;
    uint TextureID;
    float3 Position;
    CubemapFlags Flags;
};