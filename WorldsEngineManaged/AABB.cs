namespace WorldsEngine.Math;

public struct AABB
{
    public Vector3 HalfExtents { get; private set; }
    public Vector3 Extents => HalfExtents * 2f;

    public AABB(Vector3 extents)
    {
        HalfExtents = extents * 0.5f;
    }
    
    public bool ContainsPoint(Vector3 point)
    {
        return
            point.x < HalfExtents.x && point.x > -HalfExtents.x &&
            point.y < HalfExtents.y && point.y > -HalfExtents.y &&
            point.z < HalfExtents.z && point.z > -HalfExtents.z;
    }
}
