struct LightingTile
{
    uint lightIdMasks[8];
    uint cubemapIdMasks[2];
    uint aoBoxIdMasks[2];
    uint aoSphereIdMasks[2];
}

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