using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine;

namespace Game.Combat
{
    public enum DeathBehaviour
    {
        Destroy,
        TriggerEvent
    }

    [Component]
    public class HealthComponent : IStartListener
    {
        public Action<Entity> OnDeath;

        public double Health = 1.0;
        public double MaxHealth = 1.0;
        public DeathBehaviour DeathBehaviour = DeathBehaviour.Destroy;

        private Entity _entity;

        public void Start(Entity entity)
        {
            _entity = entity;
        }

        public void Damage(double dmg)
        {
            Health -= dmg;

            if (Health <= 0.0)
            {
                OnDeath?.Invoke(_entity);

                if (DeathBehaviour == DeathBehaviour.Destroy)
                {
                    Registry.DestroyNext(_entity);
                }
            }
        }
    }
}
