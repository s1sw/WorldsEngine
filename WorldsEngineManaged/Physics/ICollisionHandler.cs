using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WorldsEngine
{
    /// <summary>
    /// Interface for a component that wants to receive collision events.
    /// </summary>
    public interface ICollisionHandler
    {
        /// <summary>
        /// Callback for a collision.
        /// </summary>
        /// <param name="entity">The entity this component is attached to.</param>
        /// <param name="contactInfo">Info regarding the collision.</param>
        void OnCollision(Entity entity, ref PhysicsContactInfo contactInfo);
    }
}
