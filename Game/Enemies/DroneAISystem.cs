using System;
using WorldsEngine;
using WorldsEngine.Math;
using WorldsEngine.Audio;
using System.Threading.Tasks;
using Game.Combat;
using WorldsEngine.Navigation;
using Game.Player;

namespace Game.Enemies;

[Component]
[EditorFriendlyName("Drone A.I.")]
[EditorIcon(FontAwesome.FontAwesomeIcons.Brain)]
public class DroneAI : Component, IStartListener, IThinkingComponent
{
    public float P = 0.0f;
    public float D = 0.0f;

    public float RotationP = 0f;
    public float RotationD = 0f;

    [EditorFriendlyName("Maximum Positional Force")]
    public Vector3 MaxPositionalForces = new(1000.0f);
    [EditorFriendlyName("Minimum Positional Force")]
    public Vector3 MinPositionalForces = new(-1000.0f);

    [WorldsEngine.Editor.EditRelativeTransform]
    public Transform FirePoint = new();

    private float timeSinceLastBurst = 0.0f;
    private bool burstInProgress = false;
    private bool currentlyFiring = false;

    private readonly StablePD PD = new();
    private readonly V3PidController RotationPID = new();

    private bool _awake = false;
    private bool _dead = false;
    private Transform _idleHoverPose;
    private Entity _target = Entity.Null;
    private Vector3 _targetPosition;

    private NavigationPath _navigationPath = null;
    private int _pathPointIdx = 0;
    private Vector3 _pathTargetPos;

    public void Alert()
    {
        if (_dead) return;
        _awake = true;
        PlayStartupSound(Registry.GetComponent<DynamicPhysicsActor>(Entity).Pose.Position);
    }

    public void Start()
    {
        var health = Registry.GetComponent<HealthComponent>(Entity);

        health.OnDeath += (Entity ent) => {
            _dead = true;
            if (Registry.TryGetComponent<AudioSource>(ent, out AudioSource source))
            {
                source.SetParameter("Alive", 0.0f);
            }
        };

        health.OnDamage += (Entity e, double dmg, Entity attacker) => {
            if (attacker == e) return;

            _target = attacker;
            if (!_awake && !_dead) {
                Alert();
            }
        };

        _idleHoverPose = Registry.GetTransform(Entity);
    }

    private void UpdateInspectorVals()
    {
        PD.P = P;
        PD.D = D;

        RotationPID.P = RotationP;
        RotationPID.D = RotationD;
    }

    private void AvoidOtherDrones(ref Vector3 targetPosition)
    {
        const float repulsionDistance = 3.0f;

        var physicsActor = Registry.GetComponent<DynamicPhysicsActor>(Entity);
        Transform pose = physicsActor.Pose;

        foreach (Entity entityB in Registry.View<DroneAI>())
        {
            if (Entity == entityB) continue;

            var physicsActorB = Registry.GetComponent<DynamicPhysicsActor>(entityB);
            Transform poseB = physicsActorB.Pose;

            Vector3 direction = poseB.Position - pose.Position;
            float distance = direction.Length;

            if (distance > repulsionDistance) continue;

            direction.y = 0.0f;

            direction /= distance;
            targetPosition -= direction * 2.5f;
        }
    }

    private Transform CalculateTargetPose(Vector3 targetLocation, float groundHeight)
    {
        var physicsActor = Registry.GetComponent<DynamicPhysicsActor>(Entity);

        Transform pose = physicsActor.Pose;

        Transform fPointTransform = FirePoint;
        fPointTransform = fPointTransform.TransformBy(pose);
        //playerDirection.y = MathFX.Clamp(playerDirection.y, -2.0f, 2.0f);
        Vector3 firePlayerDirection = fPointTransform.Position.DirectionTo(targetLocation);
        Vector3 playerDirection = pose.Position.DirectionTo(targetLocation);

        targetLocation -= playerDirection * 5f;
        targetLocation.y = groundHeight;

        Quaternion targetRotation = Quaternion.SafeLookAt(firePlayerDirection);

        return new Transform(targetLocation, targetRotation);
    }

    private void ApplyTargetPose(Transform targetPose)
    {
        var physicsActor = Registry.GetComponent<DynamicPhysicsActor>(Entity);

        Transform pose = physicsActor.Pose;

        Vector3 force = PD.CalculateForce(pose.Position, targetPose.Position, physicsActor.Velocity, Time.DeltaTime);
        force = MathFX.Clamp(force, MinPositionalForces, MaxPositionalForces);

        if (!force.HasNaNComponent)
            physicsActor.AddForce(force);
        else
            Logger.LogWarning("Force had NaN component");

        Quaternion quatDiff = targetPose.Rotation * pose.Rotation.Inverse;

        float angle = quatDiff.Angle;
        angle = PDUtil.AngleToErr(angle);

        Vector3 axis = quatDiff.Axis;

        Vector3 torque = RotationPID.CalculateForce(angle * axis, Time.DeltaTime);

        if (!torque.HasNaNComponent)
            physicsActor.AddTorque(torque);
        else
            Logger.LogWarning("Torque had NaN component");
    }

    private void UpdateFiring()
    {
        const float burstPeriod = 2.0f;

        var physicsActor = Registry.GetComponent<DynamicPhysicsActor>(Entity);
        Transform pose = physicsActor.Pose;

        timeSinceLastBurst += Time.DeltaTime;

        Vector3 playerDirection = (_targetPosition - pose.Position).Normalized;
        float dotProduct = playerDirection.Dot(pose.Rotation * Vector3.Forward);

        Vector3 soundOrigin = pose.TransformPoint(FirePoint.Position);

        if (dotProduct > 0.95f && timeSinceLastBurst > burstPeriod - 1.0f && !burstInProgress)
        {
            FireBurst(soundOrigin, physicsActor);
        }
    }

    private async void FireBurst(Vector3 soundOrigin, DynamicPhysicsActor physicsActor)
    {
        burstInProgress = true;

        //Audio.PlayOneShotEvent("event:/Drone/Charging", physicsActor.Pose.Position + Vector3.Up);
        Audio.PlayOneShotAttachedEvent("event:/Drone/Charging", physicsActor.Pose.Position + Vector3.Up, Entity);

        await Task.Delay(800);
        currentlyFiring = true;
        await Task.Delay(200);

        for (int i = 0; i < 4; i++)
        {
            if (!Registry.Valid(Entity) || _dead) return;

            Transform pose = physicsActor.Pose;
            AssetID projectileId = AssetDB.PathToId("Prefabs/gun_projectile.wprefab");
            Entity projectile = Registry.CreatePrefab(projectileId);

            Transform firePoint = FirePoint; 

            Transform projectileTransform = Registry.GetTransform(projectile);
            Transform firePointTransform = firePoint.TransformBy(pose);

            Vector3 forward = firePointTransform.Rotation * Vector3.Forward;

            projectileTransform.Position = firePointTransform.Position;
            projectileTransform.Rotation = firePointTransform.Rotation;

            Registry.SetTransform(projectile, projectileTransform);

            DynamicPhysicsActor projectileDpa = Registry.GetComponent<DynamicPhysicsActor>(projectile);

            projectileDpa.Pose = projectileTransform;
            projectileDpa.AddForce(forward * 100.0f, ForceMode.VelocityChange);

            physicsActor.AddForce(-forward * 100.0f * projectileDpa.Mass, ForceMode.Impulse);

            Audio.PlayOneShotAttachedEvent("event:/Weapons/Gun Shot", soundOrigin, Entity);

            var damagingProjectile = Registry.GetComponent<DamagingProjectile>(projectile);
            damagingProjectile.Attacker = Entity;
            damagingProjectile.Damage = 30.0;

            await Task.Delay(100);
        }

        currentlyFiring = false;
        burstInProgress = false;
        timeSinceLastBurst = 0.0f;
    }

    public void Think()
    {
        UpdateInspectorVals();

        if (_dead) return;

        var physicsActor = Registry.GetComponent<DynamicPhysicsActor>(Entity);
        Transform pose = physicsActor.Pose;

        if (WorldsEngine.Input.Keyboard.KeyPressed(WorldsEngine.Input.KeyCode.L))
            FireBurst(pose.Position, physicsActor);

        if (!_awake)
        {
            // Look for a player nearby
            const int MaxOverlap = 1;
            Span<Entity> overlaps = stackalloc Entity[MaxOverlap];
            uint overlapCount = Physics.OverlapSphereMultiple(pose.Position, 7.5f, MaxOverlap, overlaps, ~PhysicsLayers.Player);


            ApplyTargetPose(_idleHoverPose);

            if (overlapCount > 0 && !DebugGlobals.AIIgnorePlayer)
            {
                var playerTransform = Registry.GetTransform(LocalPlayerSystem.PlayerBody);
                var playerDir = (playerTransform.Position - pose.Position).Normalized;
                // Check visibility
                if (Physics.Raycast(pose.Position + playerDir, playerDir, out RaycastHit hit) && hit.HitEntity == LocalPlayerSystem.PlayerBody)
                {
                    Alert();
                    _target = LocalPlayerSystem.PlayerBody;
                }
            }

            return;
        }

        if (DebugGlobals.AIIgnorePlayer && _target == LocalPlayerSystem.PlayerBody)
        {
            _target = Entity.Null;
            _awake = false;
        }

        if (!Registry.Valid(_target))
        {
            _target = Entity.Null;
            _awake = false;
            Logger.Log("Target invalid, going to sleep.");
            return;
        }

        if (_target.TryGetComponent<HealthComponent>(out var targetHealth))
        {
            if (targetHealth.Dead)
            {
                _target = Entity.Null;
                _awake = false;
            }
        }

        bool foundFloor = Physics.Raycast(pose.Position + (Vector3.Down * 0.2f), Vector3.Down, out RaycastHit rHit, 50.0f);

        if (!currentlyFiring)
        {
            if (!Registry.HasComponent<DynamicPhysicsActor>(_target))
            {
                _targetPosition = Registry.GetTransform(_target).Position;
            }
            else
            {
                var dpa = Registry.GetComponent<DynamicPhysicsActor>(_target);
                var com = dpa.CenterOfMassLocalPose.TransformBy(dpa.Pose);
                _targetPosition = com.Position;
            }
        }

        Transform targetPose = CalculateTargetPose(_targetPosition, foundFloor ? rHit.WorldHitPos.y : pose.Position.y);

        if (_navigationPath == null || !_navigationPath.Valid || targetPose.Position.DistanceTo(_pathTargetPos) > 1.5f)
        {
            _pathTargetPos = targetPose.Position;
            _navigationPath = NavigationSystem.FindPath(rHit.WorldHitPos, targetPose.Position);
            _pathPointIdx = 0;
            targetPose.Position = _targetPosition;
        }
        else
        {
            Vector3 targetPoint = _navigationPath[_pathPointIdx];

            if ((pose.Position - Vector3.Up * 2.0f).DistanceTo(targetPoint) < 1.0f && _pathPointIdx < _navigationPath.NumPoints - 1)
                _pathPointIdx++;

            targetPose.Position = _navigationPath[_pathPointIdx] + Vector3.Up * 2.0f;
        }

        AvoidOtherDrones(ref targetPose.Position);
        ApplyTargetPose(targetPose);

        UpdateFiring();
    }

    private void PlayStartupSound(Vector3 pos)
    {
        Audio.PlayOneShotAttachedEvent("event:/Drone/Alert", pos, Entity);
        if (!Registry.HasComponent<AudioSource>(Entity)) return;
        var audioSource = Registry.GetComponent<AudioSource>(Entity);

        audioSource.Start();
    }
}
