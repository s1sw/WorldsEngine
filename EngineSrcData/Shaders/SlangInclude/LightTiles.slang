struct LightingTile
{
    // 0
    uint lightIdMasks[8];
    // 32
    uint cubemapIdMasks[2];
    // 40
    uint aoBoxIdMasks[2];
    // 48
    uint aoSphereIdMasks[2];
    // 56
}
static const int LIGHT_TILE_SIZE = 56;

struct LightTileInfo
{
    uint tileSize;
    uint tilesPerEye;
    uint numTilesX;
    uint numTilesY;
}

LightTileInfo CalculateTileInfo(int eyeWidth, int eyeHeight)
{
    const int TileSize = 32;

    LightTileInfo ti;
    ti.tileSize = TileSize;
    ti.numTilesX = (eyeWidth + TileSize - 1) / TileSize;
    ti.numTilesY = (eyeHeight + TileSize - 1) / TileSize;
    ti.tilesPerEye = ti.numTilesX * ti.numTilesY;

    return ti;
}

struct LightTileLoader
{
    ByteAddressBuffer lightTileBuffer;
    
    uint GetLightIDMask(uint tileIdx, int maskIdx)
    {
        return lightTileBuffer.Load(int(tileIdx) * LIGHT_TILE_SIZE + maskIdx * 4);
    }
    
    uint GetCubemapIDMask(uint tileIdx, int maskIdx)
    {
        return lightTileBuffer.Load(int(tileIdx) * LIGHT_TILE_SIZE + 32 + maskIdx * 4);
    }
}
