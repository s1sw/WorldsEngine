using WorldsEngine;
using Game.Interaction;

namespace Game;

[Component]
class GrapplingHook : IThinkingComponent, IStartListener
{
    private bool _grappling = false;

    public void Start(Entity entity)
    {
        var grabbable = Registry.GetComponent<Grabbable>(entity);
        grabbable.TriggerPressed += Grabbable_TriggerPressed;
        grabbable.TriggerReleased += Grabbable_TriggerReleased;
    }

    private void Grabbable_TriggerPressed(Entity entity)
    {
        var transform = Registry.GetTransform(entity);
        // Raycast and find a hit point to pull the player to
        if (Physics.Raycast(transform.Position, transform.Forward))
        {
                
        }
    }

    private void Grabbable_TriggerReleased(Entity entity)
    {
        _grappling = false;
    }

    public void Think(Entity entity)
    {
    }
}
