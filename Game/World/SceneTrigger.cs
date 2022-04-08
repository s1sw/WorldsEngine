using WorldsEngine;
using WorldsEngine.Math;
using Game.Player;

namespace Game.World;

[Component]
public class SceneTrigger : Component, IThinkingComponent
{
    public Vector3 Size = new();
    public string Scene = string.Empty;

    public void Think()
    {
        Transform t = Entity.Transform;
        AABB aabb = new AABB(Size);

        if (aabb.ContainsPoint(t.InverseTransformPoint(PlayerCameraSystem.WorldSpaceHeadPosition)))
        {
            LocalPlayerSystem.SetTransitionSpawn(t);
            SceneLoader.LoadScene(AssetDB.PathToId(Scene));
        }
    }
}
