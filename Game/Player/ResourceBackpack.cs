using WorldsEngine;
using Game.Interaction;
using WorldsEngine.Math;
using Game.World;
using WorldsEngine.Audio;

namespace Game.Player;

public class ResourceBackpack : ISystem
{
    public void OnSceneStart()
    {
        HandGrab.OnReleaseEntity += OnReleaseEntity;
    }

    private void OnReleaseEntity(Entity grabbed, AttachedHandFlags handFlags)
    {
        if (!grabbed.HasComponent<ResourcePickup>()) return;

        // Check if the entity is being released behind the player's head
        Vector3 localSpace = PlayerCameraSystem.WorldSpaceHeadTransform.InverseTransformPoint(grabbed.Transform.Position);

        AABB aabb = new(new Vector3(1.5f, 1.25f, 1.0f), Vector3.Backward * 0.5f);
        if (!aabb.ContainsPoint(localSpace)) return;

        var pickup = grabbed.GetComponent<ResourcePickup>();
        pickup.PickUp();
        Audio.PlayOneShotEvent("event:/Player/Backpack");
    }
}