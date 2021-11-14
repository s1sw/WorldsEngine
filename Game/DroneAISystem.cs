using System;
using WorldsEngine;
using WorldsEngine.Math;
using WorldsEngine.Audio;
using WorldsEngine.ComponentMeta;
using WorldsEngine.Editor;
using System.Threading.Tasks;
using Game.Combat;
using ImGuiNET;

namespace Game
{
    [Component]
    [EditorFriendlyName("Drone A.I.")]
    [EditorIcon(FontAwesome.FontAwesomeIcons.Brain)]
    public class DroneAI : IStartListener, IThinkingComponent
    {
        public float P = 0.0f;
        public float D = 0.0f;

        public float RotationP = 0f;
        public float RotationD = 0f;

        [EditorFriendlyName("Maximum Positional Force")]
        public Vector3 MaxPositionalForces = new Vector3(1000.0f);
        [EditorFriendlyName("Minimum Positional Force")]
        public Vector3 MinPositionalForces = new Vector3(-1000.0f);

        public Vector3 FirePointPosition = new Vector3();

        private float timeSinceLastBurst = 0.0f;
        private bool burstInProgress = false;
        private bool currentlyFiring = false;

        private StablePD PD = new StablePD();
        private V3PidController RotationPID = new V3PidController();

        private bool _awake = false;
        private bool _dead = false;
        private Entity _ent;
        private Transform _idleHoverPose;
        private Entity _target = Entity.Null;
        private Vector3 _targetPosition;

        public void Alert()
        {
            if (_dead) return;
            _awake = true;
            PlayStartupSound(_ent, Registry.GetComponent<DynamicPhysicsActor>(_ent).Pose.Position);
        }

        public void Start(Entity entity)
        {
            var health = Registry.GetComponent<HealthComponent>(entity);

            health.OnDeath += (Entity ent) => {
                _dead = true;
                if (Registry.TryGetComponent<AudioSource>(ent, out AudioSource source))
                {
                    source.SetParameter("Alive", 0.0f);
                }
                //Audio.PlayOneShotAttachedEvent("event:/Chromium/TheTea", Registry.GetTransform(ent).Position, ent);
                //Audio.PlayOneShot(AssetDB.PathToId("Audio/SFX/drone death.ogg"), Registry.GetTransform(ent).Position, 2.0f);
            };

            health.OnDamage += (Entity e, double dmg, Entity attacker) => {
                if (attacker == e) return;

                _target = attacker;
                if (!_awake && !_dead) {
                    Alert();
                }
            };

            _ent = entity;
            _idleHoverPose = Registry.GetTransform(entity);
        }

        private void UpdateInspectorVals()
        {
            PD.P = P;
            PD.D = D;

            RotationPID.P = RotationP;
            RotationPID.D = RotationD;
        }

        private void AvoidOtherDrones(Entity entity, ref Vector3 targetPosition)
        {
            const float repulsionDistance = 3.0f;

            var physicsActor = Registry.GetComponent<DynamicPhysicsActor>(entity);
            Transform pose = physicsActor.Pose;

            foreach (Entity entityB in Registry.View<DroneAI>())
            {
                if (entity == entityB) continue;

                var physicsActorB = Registry.GetComponent<DynamicPhysicsActor>(entityB);
                Transform poseB = physicsActorB.Pose;

                Vector3 direction = poseB.Position - pose.Position;
                float distance = direction.Length;

                if (distance > repulsionDistance) return;

                direction.y = 0.0f;

                direction /= distance;
                targetPosition -= direction * (distance + 1.0f);
            }
        }

        private Transform CalculateTargetPose(Entity entity, Vector3 targetLocation, float groundHeight)
        {
            var physicsActor = Registry.GetComponent<DynamicPhysicsActor>(entity);

            Transform pose = physicsActor.Pose;

            Transform fPointTransform = new(FirePointPosition, Quaternion.Identity);
            fPointTransform = fPointTransform.TransformBy(pose);
            //playerDirection.y = MathFX.Clamp(playerDirection.y, -2.0f, 2.0f);
            Vector3 firePlayerDirection = targetLocation - fPointTransform.Position;
            firePlayerDirection.Normalize();

            Vector3 playerDirection = targetLocation - pose.Position;
            playerDirection.Normalize();

            targetLocation -= playerDirection * 3.5f;
            targetLocation.y = MathF.Max(targetLocation.y, groundHeight + 2.5f);

            Quaternion targetRotation = Quaternion.SafeLookAt(firePlayerDirection);

            return new Transform(targetLocation, targetRotation);
        }

        private void ApplyTargetPose(Entity entity, Transform targetPose)
        {
            var physicsActor = Registry.GetComponent<DynamicPhysicsActor>(entity);

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

        private void UpdateFiring(Entity entity)
        {
            const float burstPeriod = 2.0f;

            var physicsActor = Registry.GetComponent<DynamicPhysicsActor>(entity);
            Transform pose = physicsActor.Pose;

            timeSinceLastBurst += Time.DeltaTime;

            Vector3 playerDirection = (_targetPosition - pose.Position).Normalized;
            float dotProduct = playerDirection.Dot(pose.Rotation * Vector3.Forward);

            Vector3 soundOrigin = pose.TransformPoint(FirePointPosition);

            if (dotProduct > 0.95f && timeSinceLastBurst > burstPeriod - 1.0f && !burstInProgress)
            {
                FireBurst(soundOrigin, entity, physicsActor);
            }
        }

        private async void FireBurst(Vector3 soundOrigin, Entity entity, DynamicPhysicsActor physicsActor)
        {
            burstInProgress = true;

            //Audio.PlayOneShotEvent("event:/Drone/Charging", physicsActor.Pose.Position + Vector3.Up);
            Audio.PlayOneShotAttachedEvent("event:/Drone/Charging", physicsActor.Pose.Position + Vector3.Up, entity);

            await Task.Delay(800);
            currentlyFiring = true;
            await Task.Delay(200);

            for (int i = 0; i < 4; i++)
            {
                if (!Registry.Valid(entity) || _dead) return;

                Transform pose = physicsActor.Pose;
                AssetID projectileId = AssetDB.PathToId("Prefabs/gun_projectile.wprefab");
                Entity projectile = Registry.CreatePrefab(projectileId);

                Transform firePoint = new(FirePointPosition, Quaternion.Identity);

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

                Audio.PlayOneShotAttachedEvent("event:/Weapons/Gun Shot", soundOrigin, entity);

                var damagingProjectile = Registry.GetComponent<DamagingProjectile>(projectile);
                damagingProjectile.Attacker = entity;
                damagingProjectile.Damage = 30.0;

                await Task.Delay(100);
            }

            currentlyFiring = false;
            burstInProgress = false;
            timeSinceLastBurst = 0.0f;
        }

        private readonly Entity[] _overlapBuf = new Entity[32];
        public void Think(Entity entity)
        {
            UpdateInspectorVals();

            if (_dead) return;

            var physicsActor = Registry.GetComponent<DynamicPhysicsActor>(entity);
            Transform pose = physicsActor.Pose;

            if (WorldsEngine.Input.Keyboard.KeyPressed(WorldsEngine.Input.KeyCode.L))
                FireBurst(pose.Position, entity, physicsActor);

            if (!_awake)
            {
                // Look for a player nearby
                const int MaxOverlap = 32;
                uint overlapCount = Physics.OverlapSphereMultiple(pose.Position, 7.5f, MaxOverlap, _overlapBuf, ~PhysicsLayers.Player);

                ApplyTargetPose(entity, _idleHoverPose);

                if (overlapCount > 0)
                {
                    Alert();
                    _target = PlayerRigSystem.PlayerBody;
                }

                return;
            }

            if (!Registry.Valid(_target))
            {
                _target = Entity.Null;
                _awake = false;
                Logger.Log("Target invalid, going to sleep.");
                return;
            }

            bool foundFloor = Physics.Raycast(pose.Position + (Vector3.Down * 0.1f), Vector3.Down, out RaycastHit rHit, 50.0f);

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

            Transform targetPose = CalculateTargetPose(entity, _targetPosition, foundFloor ? rHit.WorldHitPos.y : pose.Position.y + 0.01f);

            AvoidOtherDrones(entity, ref targetPose.Position);
            ApplyTargetPose(entity, targetPose);
            UpdateFiring(entity);
        }

        private void PlayStartupSound(Entity entity, Vector3 pos)
        {
            Audio.PlayOneShotAttachedEvent("event:/Drone/Alert", pos, entity);
            if (!Registry.HasComponent<AudioSource>(entity)) return;
            var audioSource = Registry.GetComponent<AudioSource>(entity);

            audioSource.Start();
        }
    }
}
