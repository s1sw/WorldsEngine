using System.Collections.Generic;
using System.Text.Json;
using WorldsEngine.ECS;
using WorldsEngine.Math;
using WorldsEngine.PhysFS;
namespace WorldsEngine.Editor;

struct GizmoEdge
{
    public Vector3 VertA;
    public Vector3 VertB;
}

struct LoadedGizmo
{
    public GizmoEdge[] edges;
}

static class GizmoRenderer
{
    private readonly static Dictionary<string, LoadedGizmo> _gizmoCache = new();

    private static Vector3 DeserializeVec(JsonElement el)
    {
        return new Vector3(el.GetProperty("x").GetSingle(), el.GetProperty("y").GetSingle(), el.GetProperty("z").GetSingle());
    }

    private static void CacheGizmo(string gizmoPath)
    {
        using (PhysFSFileStream stream = PhysFS.PhysFS.OpenRead(gizmoPath))
        {
            LoadedGizmo lg = new();
            using (var jdoc = JsonDocument.Parse(stream))
            {
                var edgeArray = jdoc.RootElement.GetProperty("edges");
                lg.edges = new GizmoEdge[edgeArray.GetArrayLength()];

                for (int i = 0; i < edgeArray.GetArrayLength(); i += 2)
                {
                    Vector3 vA = DeserializeVec(edgeArray[i]);
                    Vector3 vB = DeserializeVec(edgeArray[i + 1]);
                    lg.edges[i] = new GizmoEdge{VertA = vA, VertB = vB};
                }
            }
            _gizmoCache.Add(gizmoPath, lg);
        }
    }

    public static void DrawGizmo(Entity entity, string gizmoPath)
    {
        if (!_gizmoCache.ContainsKey(gizmoPath)) CacheGizmo(gizmoPath);

        LoadedGizmo gizmo = _gizmoCache[gizmoPath];

        Transform t = entity.Transform;
        for (int i = 0; i < gizmo.edges.Length; i++)
        {
            GizmoEdge edge = gizmo.edges[i];
            DebugShapes.DrawLine(t.TransformPoint(edge.VertA), t.TransformPoint(edge.VertB), new Vector4(10.0f, 0.0f, 0.0f, 1.0f));
        }
    }
}