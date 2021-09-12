using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WorldsEngine
{
    public interface ICollisionHandler
    {
        void OnCollision(Entity entity, ref PhysicsContactInfo contactInfo);
    }
}
