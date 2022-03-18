using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine;
using WorldsEngine.Audio;

namespace Game.World;

[Component]
[EditorFriendlyName("Collision Sounds")]
public class CollisionSound : ICollisionHandler
{
    public string EventPath;

    public void OnCollision(ref PhysicsContactInfo contactInfo)
    {
        Audio.PlayOneShotEvent(EventPath, contactInfo.AverageContactPoint, MathF.Min(contactInfo.RelativeSpeed * 0.125f, 1.0f));
    }
}
