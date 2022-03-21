using System;
using WorldsEngine;
using WorldsEngine.Math;
using WorldsEngine.Util;
using WorldsEngine.Audio;
using WorldsEngine.Input;
using Game.Combat;
using Game.Player;

namespace Game.Enemies;

[Component]
public class Turret : Component, IStartListener, IThinkingComponent
{
    private Transform _initialPitchTransform;
    private Transform _initialYawTransform;
    private Vector3 _firePoint = new();
    private Vector3 _fireDirection = new();

    public void Start()
    {
        var swo = Entity.GetComponent<SkinnedWorldObject>();

        for (uint i = 0; i < MeshManager.GetBoneCount(swo.Mesh); i++)
        {
            var restPose = MeshManager.GetBoneRestTransform(swo.Mesh, i);
            swo.SetBoneTransform(i, restPose);
        }

        _initialPitchTransform = MeshManager.GetBoneRestTransform(swo.Mesh, MeshManager.GetBoneIndex(swo.Mesh, "PitchPivot"));
        _initialYawTransform = MeshManager.GetBoneRestTransform(swo.Mesh, MeshManager.GetBoneIndex(swo.Mesh, "YawPivot"));
    }

    private float _fireTimer = 0f;
    private int _shotsToFire = 15;
    private bool _isReloading = false;

    public void Think()
    {
        if (DebugGlobals.AIIgnorePlayer) return;

        var transform = Entity.Transform;
        var swo = Entity.GetComponent<SkinnedWorldObject>();
        uint pitchBoneIdx = MeshManager.GetBoneIndex(swo.Mesh, "PitchPivot");
        uint yawBoneIdx = MeshManager.GetBoneIndex(swo.Mesh, "YawPivot");

        Entity targetEntity = LocalPlayerSystem.PlayerBody;
        bool targetVisible = false;

        if (Physics.Raycast(_firePoint, _fireDirection, out RaycastHit hit, 20.0f))
        {
            targetVisible = hit.HitEntity == targetEntity;
        }

        Vector3 gunPos = new Vector3(0.0f, 0.875f, 0.0f);
        Vector3 target = transform.InverseTransformPoint(targetEntity.Transform.Position);
        float distance = gunPos.DistanceTo(target);
        Vector3 direction = (target - gunPos).Normalized;

        float pitchAngle = MathF.Asin(direction.y);
        float yawAngle = MathF.Atan2(direction.x, direction.z);

        if (_isReloading)
        {
            pitchAngle = -(MathF.PI * 0.25f) * ((5f - _fireTimer) / 5f);
        }

        Transform pitchPivotTransform = _initialPitchTransform; 
        Quaternion rotation = Quaternion.AngleAxis(pitchAngle - (MathF.PI * 0.5f), Vector3.Forward);
        pitchPivotTransform.Rotation = rotation;
        swo.SetBoneTransform(pitchBoneIdx, pitchPivotTransform);

        Transform yawPivotTransform = _initialYawTransform; 
        Quaternion yawRotation = Quaternion.AngleAxis(yawAngle - (MathF.PI * 0.5f), Vector3.Up);
        yawPivotTransform.Rotation = yawRotation;
        swo.SetBoneTransform(yawBoneIdx, yawPivotTransform);
        
        var finalFireTransform = pitchPivotTransform.TransformBy(yawPivotTransform.TransformBy(transform));
        finalFireTransform = new Transform(Vector3.Zero, Quaternion.AngleAxis(-MathF.PI * 0.5f, Vector3.Left)).TransformBy(finalFireTransform);
        _firePoint = finalFireTransform.TransformPoint(new Vector3(0.0f, 0.0f, 0.5f));
        _fireDirection = finalFireTransform.TransformDirection(new Vector3(0f, 0f, 1f));


        _fireTimer += Time.DeltaTime;
        if (!_isReloading)
        {
            if (_fireTimer > 0.2f && targetVisible)
            {
                AssetID projectileId = AssetDB.PathToId("Prefabs/gun_projectile.wprefab");
                Entity projectile = Registry.CreatePrefab(projectileId);

                Transform projectileTransform = Registry.GetTransform(projectile);

                projectileTransform.Position = _firePoint;
                projectileTransform.Rotation = Quaternion.SafeLookAt(_fireDirection);

                Registry.SetTransform(projectile, projectileTransform);

                DynamicPhysicsActor projectileDpa = Registry.GetComponent<DynamicPhysicsActor>(projectile);

                projectileDpa.Pose = projectileTransform;
                projectileDpa.AddForce(_fireDirection * 100.0f, ForceMode.VelocityChange);

                Audio.PlayOneShotAttachedEvent("event:/Weapons/Gun Shot", _firePoint, Entity);

                var damagingProjectile = Registry.GetComponent<DamagingProjectile>(projectile);
                damagingProjectile.Attacker = Entity;
                damagingProjectile.Damage = 5.0;
                _fireTimer = 0f;
                _shotsToFire--;

                if (_shotsToFire <= 0)
                {
                    _isReloading = true;
                }
            }
        }
        else
        {
            if (_fireTimer >= 5f)
            {
                _isReloading = false;
                _shotsToFire = 15;
                _fireTimer = 0f;
            }
        }
    }
}
