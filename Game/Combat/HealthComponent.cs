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
    public class HealthComponent : Component
    {
        public event Action<Entity> OnDeath;
        public event Action<Entity, double, Entity> OnDamage;

        public double Health = 1.0;
        public double MaxHealth = 1.0;
        public DeathBehaviour DeathBehaviour = DeathBehaviour.Destroy;
        public bool Dead { get; private set; }

        public void Damage(double dmg, Entity damager)
        {
            Health -= dmg;
            OnDamage?.Invoke(Entity, dmg, damager);

            if (Health <= 0.0 && !Dead)
            {
                Dead = true;
                OnDeath?.Invoke(Entity);

                if (DeathBehaviour == DeathBehaviour.Destroy)
                {
                    Registry.DestroyNext(Entity);
                }
            }
        }

        public void Damage(double dmg)
        {
            Damage(dmg, Entity.Null);
        }
    }
}
