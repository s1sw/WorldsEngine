namespace WorldsEngine.Math;

public struct AABB
{
    public Vector3 HalfExtents { get; private set; }
    public Vector3 Extents => HalfExtents * 2f;
    public Vector3 Center;

    public AABB(Vector3 extents)
    {
        HalfExtents = extents * 0.5f;
        Center = new(0.0f);
    }

    public AABB(Vector3 extents, Vector3 center)
    {
        HalfExtents = extents * 0.5f;
        Center = center;
    }
    
    public bool ContainsPoint(Vector3 point)
    {
        point = point - Center;
        return
            point.x < HalfExtents.x && point.x > -HalfExtents.x &&
            point.y < HalfExtents.y && point.y > -HalfExtents.y &&
            point.z < HalfExtents.z && point.z > -HalfExtents.z;
    }
}
