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
        public event Action<Entity> OnDeath;
        public event Action<Entity, double, Entity> OnDamage;

        public double Health = 1.0;
        public double MaxHealth = 1.0;
        public DeathBehaviour DeathBehaviour = DeathBehaviour.Destroy;
        public bool Dead { get; private set; }

        private Entity _entity;

        public void Start(Entity entity)
        {
            _entity = entity;
        }

        public void Damage(double dmg, Entity damager)
        {
            Health -= dmg;
            OnDamage?.Invoke(_entity, dmg, damager);

            if (Health <= 0.0 && !Dead)
            {
                Dead = true;
                OnDeath?.Invoke(_entity);

                if (DeathBehaviour == DeathBehaviour.Destroy)
                {
                    Registry.DestroyNext(_entity);
                }
            }
        }

        public void Damage(double dmg)
        {
            Damage(dmg, Entity.Null);
        }
    }
}
