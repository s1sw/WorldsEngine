using WorldsEngine;
using Game.Player;
namespace Game.World;

[Component]
public class ResourcePickup : Component
{
    public int Value = 1;
    public ResourceType ResourceType = ResourceType.Metal;
    private bool _pickedUp = false;

    public void PickUp()
    {
        if (_pickedUp) return;
        _pickedUp = true;
        Registry.DestroyNext(Entity);
        PlayerResources.Metal += Value;
    }
}