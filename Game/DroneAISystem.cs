using System;
using WorldsEngine;
using WorldsEngine.Math;
using WorldsEngine.Audio;
using WorldsEngine.ComponentMeta;
using System.Threading.Tasks;
using Game.Combat;

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

        private Vector3 target = new Vector3();
        private float timeSinceLastBurst = 0.0f;
        private bool burstInProgress = false;

        private StablePD PD = new StablePD();
        private V3PidController RotationPID = new V3PidController();

        private bool _awake = false;
        private bool _dead = false;
        private Entity _ent;

        public void Alert()
        {
            _awake = true;
            PlayStartupSound(_ent, Registry.GetComponent<DynamicPhysicsActor>(_ent).Pose.Position);
        }

        public void Start(Entity entity)
        {
            var health = Registry.GetComponent<HealthComponent>(entity);

            health.OnDeath += (Entity ent) => {
                _dead = true;
                Registry.GetComponent<AudioSource>(ent).IsPlaying = false;
                Audio.PlayOneShot(AssetDB.PathToId("Audio/SFX/drone death.ogg"), Registry.GetTransform(ent).Position, 1.5f);
            };

            health.OnDamage += (Entity e, double dmg) => {
                if (!_awake && !_dead)
                    Alert();
            };

            _ent = entity;
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

            Vector3 playerDirection = targetLocation - pose.Position;
            playerDirection.y = MathFX.Clamp(playerDirection.y, -2.0f, 2.0f);
            playerDirection.Normalize();

            targetLocation -= playerDirection * 3.5f;
            targetLocation.y = MathF.Max(targetLocation.y, groundHeight + 2.5f);

            Quaternion targetRotation = Quaternion.SafeLookAt(playerDirection);


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
            const float burstPeriod = 3.0f;

            var physicsActor = Registry.GetComponent<DynamicPhysicsActor>(entity);
            Transform pose = physicsActor.Pose;

            timeSinceLastBurst += Time.DeltaTime;

            Vector3 playerDirection = (target - pose.Position).Normalized;
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

            Logger.Log("Begin burst...");
            Audio.PlayOneShot(AssetDB.PathToId("Audio/SFX/drone pre burst.ogg"), physicsActor.Pose.Position + Vector3.Up, 1.0f);
            await Task.Delay(1000);
            Logger.Log("Firing!");

            for (int i = 0; i < 4; i++)
            {
                if (!Registry.Valid(entity) || _dead) return;

                Transform pose = physicsActor.Pose;
                AssetID projectileId = AssetDB.PathToId("Prefabs/gun_projectile.wprefab");
                Entity projectile = Registry.CreatePrefab(projectileId);

                Transform firePoint = new Transform(FirePointPosition, Quaternion.Identity);

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

                Audio.PlayOneShot(AssetDB.PathToId("Audio/SFX/gunshot.ogg"), soundOrigin, 1.0f);

                await Task.Delay(100);
            }

            burstInProgress = false;
            timeSinceLastBurst = 0.0f;
        }

        public void Think(Entity entity)
        {
            UpdateInspectorVals();

            if (_dead) return;

            var physicsActor = Registry.GetComponent<DynamicPhysicsActor>(entity);
            Transform pose = physicsActor.Pose;

            if (!_awake)
            {
                // Look for a player nearby
                Entity[] buf = new Entity[8];
                uint overlapCount = Physics.OverlapSphereMultiple(pose.Position, 5.0f, 8, buf, ~PhysicsLayers.Player);
                Registry.GetComponent<AudioSource>(entity).IsPlaying = false;

                if (overlapCount > 0)
                {
                    Alert();
                }

                return;
            }

            bool foundFloor = Physics.Raycast(pose.Position + (Vector3.Down * 0.05f), Vector3.Down, out RaycastHit rHit, 50.0f);

            if (!burstInProgress)
                target = Camera.Main.Position;

            Transform targetPose = CalculateTargetPose(entity, target, foundFloor ? rHit.WorldHitPos.y : pose.Position.y + 0.01f);

            AvoidOtherDrones(entity, ref targetPose.Position);
            ApplyTargetPose(entity, targetPose);
            UpdateFiring(entity);
        }

        private async void PlayStartupSound(Entity entity, Vector3 pos)
        {
            var audioSource = Registry.GetComponent<AudioSource>(entity);

            Audio.PlayOneShot(AssetDB.PathToId("Audio/SFX/drone alert.ogg"), pos, 1.5f);

            audioSource.Loop = true;
            audioSource.ClipId = AssetDB.PathToId("Audio/SFX/drone startup.ogg");
            audioSource.IsPlaying = true;

            await Task.Delay(500);

            if (_dead)
            {
                audioSource.Loop = false;
                audioSource.IsPlaying = false;
                return;
            }

            audioSource.ClipId = AssetDB.PathToId("Audio/SFX/drone idle.ogg");
            audioSource.IsPlaying = true;
        }
    }
}
