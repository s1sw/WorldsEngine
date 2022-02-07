using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using JetBrains.Annotations;

namespace WorldsEngine;

public static partial class Physics
{
    struct Collision
    {
        public uint EntityID;
        public PhysicsContactInfo ContactInfo;
    }

    private static readonly Queue<Collision> _collisionQueue = new();

    [UsedImplicitly]
    [SuppressMessage("CodeQuality", "IDE0051:Remove unused private members",
        Justification = "Called from native C++ during deserialization")]
    private static void HandleCollisionFromNative(uint entityId, ref PhysicsContactInfo contactInfo)
    {
        _collisionQueue.Enqueue(new Collision() { EntityID = entityId, ContactInfo = contactInfo });
    }

    internal static void FlushCollisionQueue()
    {
        while (_collisionQueue.Count > 0)
        {
            var collision = _collisionQueue.Dequeue();
            try
            {
                if (Registry.Valid(new Entity(collision.EntityID)))
                {
                    Registry.HandleCollision(collision.EntityID, ref collision.ContactInfo);
                }
            }
            catch (Exception e)
            {
                Logger.LogError($"Caught exception: {e}");
            }
        }
    }

    internal static void ClearCollisionQueue()
    {
        _collisionQueue.Clear();
    }
}
