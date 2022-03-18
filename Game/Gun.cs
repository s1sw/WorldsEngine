using Game.Interaction;
using System;
using WorldsEngine;
using WorldsEngine.Audio;
using WorldsEngine.Math;
using WorldsEngine.Editor;
using Game.Combat;
using Game.Player;

namespace Game;

public enum AmmoType
{
    Laser,
    Humongous
}

[Component]
[EditorFriendlyName("Gun")]
class Gun : Component, IThinkingComponent, IStartListener, ICollisionHandler
{
    public bool Automatic = false;
    public float ShotSpacing = 0.1f;
    public AmmoType ProjectileType = AmmoType.Laser;
    public bool MagazineRequired = false;

    [EditRelativeTransform]
    public Transform MagazineAttachPoint;

    [EditRelativeTransform]
    public Transform ProjectileSpawnTransform;

    private float _shotTimer = 0f;
    private AssetID _projectilePrefab;
    private bool _hasMagazine = false;
    private Entity _currentMagazine = Entity.Null;

    public void Start()
    {
        var grabbable = Registry.GetComponent<Grabbable>(Entity);

        grabbable.TriggerPressed += Grabbable_TriggerPressed;
        grabbable.TriggerHeld += Grabbable_TriggerHeld;

        _projectilePrefab = ProjectileType switch
        {
            AmmoType.Humongous => AssetDB.PathToId("Prefabs/big_ass_projectile.wprefab"),
            _ => AssetDB.PathToId("Prefabs/gun_projectile.wprefab"),
        };
    }

    private void Grabbable_TriggerPressed(Entity entity)
    {
        if (!Automatic && _shotTimer > ShotSpacing)
            Fire(entity);
    }

    private void Grabbable_TriggerHeld(Entity entity)
    {
        if (Automatic && _shotTimer > ShotSpacing)
            Fire(entity);
    }

    public void Fire(Entity entity)
    {
        if (MagazineRequired)
        {
            if (!_hasMagazine) return;

            var mag = Registry.GetComponent<Magazine>(_currentMagazine);
            if (mag.NumShots <= 0) return;

            mag.NumShots--;

            if (mag.NumShots <= 0)
            {
                EjectMag();
            }
        }

        _shotTimer = 0f;

        var dpa = Registry.GetComponent<DynamicPhysicsActor>(entity);
        var transform = dpa.Pose;

        Transform projectileTransform = ProjectileSpawnTransform.TransformBy(transform);

        string evt = ProjectileType switch
        {
            AmmoType.Humongous => "event:/Weapons/Big Gun",
            _ => "event:/Weapons/Gun Shot"
        };

        Audio.PlayOneShotAttachedEvent(evt, transform.Position, entity);

        Entity projectile = Registry.CreatePrefab(_projectilePrefab);

        float speed = ProjectileType switch
        {
            AmmoType.Humongous => 75f,
            _ => 100f
        };

        var projectileDpa = Registry.GetComponent<DynamicPhysicsActor>(projectile);
        projectileDpa.AddForce(transform.TransformDirection(Vector3.Forward) * speed, ForceMode.VelocityChange);
        projectileTransform.Scale = projectile.Transform.Scale;
        projectileDpa.Pose = projectileTransform;
        projectile.Transform = projectileTransform;

        dpa.AddForce(-transform.TransformDirection(Vector3.Forward) * speed * projectileDpa.Mass, ForceMode.Impulse);

        if (Registry.HasComponent<Grabbable>(entity))
        {
            var grabbable = Registry.GetComponent<Grabbable>(entity);

            DoHaptics(grabbable.AttachedHandFlags);
        }

        var damagingProjectile = Registry.GetComponent<DamagingProjectile>(projectile);
        damagingProjectile.Attacker = LocalPlayerSystem.PlayerBody;
    }

    public void Think()
    {
        if (_shotTimer < ShotSpacing * 2f)
            _shotTimer += Time.DeltaTime;

        if (WorldsEngine.Input.Keyboard.KeyPressed(WorldsEngine.Input.KeyCode.K))
            EjectMag();
    }

    private async void DoHaptics(AttachedHandFlags handFlags)
    {
        switch (ProjectileType)
        {
            case AmmoType.Laser:
                HapticManager.Trigger(handFlags, 0.0f, MathF.Min(ShotSpacing, 0.2f), 50f, 1.0f);
                break;
            case AmmoType.Humongous:
                HapticManager.Trigger(handFlags, 0.0f, 0.1f, 500f, 1.0f);
                await System.Threading.Tasks.Task.Delay(100);
                HapticManager.Trigger(handFlags, 0.0f, 0.1f, 200f, 1.0f);
                await System.Threading.Tasks.Task.Delay(100);
                HapticManager.Trigger(handFlags, 0.0f, 0.1f, 50f, 1.0f);
                break;
        }
    }

    public void OnCollision(ref PhysicsContactInfo contactInfo)
    {
        if (_hasMagazine) return;
        if (!Registry.HasComponent<Magazine>(contactInfo.OtherEntity) || contactInfo.OtherEntity == _currentMagazine) return;

        Transform gunTransform = Registry.GetTransform(Entity);
        if (contactInfo.AverageContactPoint.DistanceTo(MagazineAttachPoint.TransformBy(gunTransform).Position) > 0.05f) return;

        _hasMagazine = true;

        var d6 = Registry.AddComponent<D6Joint>(Entity);
        d6.Target = contactInfo.OtherEntity;
        d6.LocalPose = MagazineAttachPoint;

        d6.SetAllAxisMotion(D6Motion.Locked);
        _currentMagazine = contactInfo.OtherEntity;

        // i hate physx
        var dpa = Registry.GetComponent<DynamicPhysicsActor>(Entity);
        dpa.Kinematic = true;
        dpa.Kinematic = false;
    }

    public void EjectMag()
    {
        if (!_hasMagazine) return;

        _hasMagazine = false;
        Registry.RemoveComponent<D6Joint>(Entity);

        var dpa = Registry.GetComponent<DynamicPhysicsActor>(_currentMagazine);
        dpa.AddForce(dpa.Pose.TransformDirection(Vector3.Left), ForceMode.VelocityChange);

        _currentMagazine = Entity.Null;
    }
}
