using WorldsEngine;
using WorldsEngine.Math;
using Game.Interaction;
using System;
using Game.Util;

namespace Game.Player;

[Component]
class ForceGrabbing : Component, IStartListener, IThinkingComponent
{
    public bool IsRightHand = false;
    private bool _hoveringEntity = false;
    private bool _bringToHand = false;
    private VRAction _triggerAction;
    private VRAction _grabAction;

    private Vector3 _floatingTargetPos;
    private Entity _lockedEntity = Entity.Null;
    private Entity _lastHoveredEntity = Entity.Null;
    private Vector3Lowpass _pushPalmDir = new(5.0f);
    private Vector3Lowpass _hoverPalmDir = new(10.0f);

    [EditableClass]
    public V3PidController PidController = new();
    public float MaxLiftMass = 30.0f;

    private Entity _lightEntity;

    public void Start()
    {
        if (!VR.Enabled) return;
        _grabAction = new VRAction(IsRightHand ? "/actions/main/in/GrabR" : "/actions/main/in/GrabL");
        _triggerAction = new VRAction(IsRightHand ? "/actions/main/in/TriggerR" : "/actions/main/in/TriggerL");
        _lightEntity = Registry.Create();
        var wl = Registry.AddComponent<WorldLight>(_lightEntity);
        wl.Type = LightType.Point;
        wl.Radius = 0.5f;
        wl.Color = new Vector3(0.098f, 0.117f, 0.651f);
        wl.Intensity = 2.0f;
        wl.Enabled = false;
    }

    public void Think()
    {
        if (!VR.Enabled) return;
        var hg = Entity.GetComponent<HandGrab>();

        if (hg.GrippedEntity != Entity.Null)
        {
            _lightEntity.GetComponent<WorldLight>().Enabled = false;
            return;
        }

        var transform = Entity.Transform;

        // Search outwords from the palm with a cone
        // For now, let's just do a single raycast
        Vector3 palmPos = transform.Position + transform.Forward * 0.1f;
        Vector3 palmDir = transform.TransformDirection(new Vector3(IsRightHand ? 0.75f : -0.75f, 0.0f, 0.25f).Normalized);
        _pushPalmDir.Update(palmDir, Time.DeltaTime);
        _hoverPalmDir.Update(palmDir, Time.DeltaTime);

        // DebugShapes.DrawLine(palmPos, palmPos + palmDir, new Vector4(1.0f, 1.0f, 1.0f, 1.0f));
        // DebugShapes.DrawLine(palmPos, palmPos + _pushPalmDir.Value, new Vector4(1.0f, 0.0f, 0.0f, 1.0f));
        // DebugShapes.DrawLine(palmPos, palmPos + _hoverPalmDir.Value, new Vector4(0.0f, 1.0f, 0.0f, 1.0f));

        var audioSource = Registry.GetComponent<AudioSource>(Entity);
        if (_bringToHand)
        {
            if (audioSource.PlaybackState == PlaybackState.Stopped)
                audioSource.Start();

            if (!Registry.Valid(_lockedEntity))
            {
                Reset();
                return;
            }

            var dpa = _lockedEntity.GetComponent<DynamicPhysicsActor>();

            // Let go when grab or trigger is released
            if (_grabAction.Released)
            {
                Vector3 handVel = Entity.GetComponent<DynamicPhysicsActor>().Velocity;

                // Limit acceleration due to the launch to 20ms^-2
                float launchForce = MathF.Min(300.0f, dpa.Mass * 20.0f);

                // Launch the object if the hands are going forward
                if (handVel.Normalized.Dot(_pushPalmDir.Value) > 0.7f && handVel.LengthSquared > 4.0f)
                {
                    dpa.AddForce(_pushPalmDir.Value * launchForce, ForceMode.Impulse);
                    WorldsEngine.Audio.Audio.PlayOneShotEvent("event:/Player/Telekinesis Launch", Entity.Transform.Position);
                }
                else if (handVel.Normalized.Dot(_pushPalmDir.Value) < -0.7f && handVel.LengthSquared > 2.0f)
                {
                    //dpa.AddForce(dpa.Pose.Position.DirectionTo(transform.Position) * launchForce * 0.33f, ForceMode.Impulse);
                    dpa.AddForce(CalculateBallisticAcceleration(dpa.Pose.Position, transform.Position) - dpa.Velocity, ForceMode.VelocityChange);
                    WorldsEngine.Audio.Audio.PlayOneShotEvent("event:/Player/Telekinesis Launch", Entity.Transform.Position);
                }
                Reset();
                return;
            }

            float dist = 1.0f;

            if (_lockedEntity.HasComponent<WorldObject>())
            {
                var obj = _lockedEntity.GetComponent<WorldObject>();
                dist = MeshManager.GetMeshSphereBoundRadius(obj.Mesh) * 1.5f * _lockedEntity.Transform.Scale.ComponentMax + 1.5f;
            }

            Vector3 targetPos = palmPos + _hoverPalmDir.Value * dist;
            Vector3 force = PidController.CalculateForce(-(dpa.WorldSpaceCenterOfMass.Position - targetPos) * MathF.Min(dpa.Mass, MaxLiftMass), Time.DeltaTime, Vector3.Zero);
            force = force.ClampMagnitude(25.0f * dpa.Mass);
            audioSource.SetParameter("TKAppliedForce", force.Length * 0.01f);
            dpa.AddForce(force);
            return;
        }
        else
        {
            if (audioSource.PlaybackState == PlaybackState.Playing)
                audioSource.Stop(StopMode.AllowFadeout);
        }

        if (!Physics.Raycast(palmPos + palmDir * 0.2f, _hoverPalmDir.Value, out RaycastHit rayHit, 10.0f, PhysicsLayers.Player))
        {
            Reset();
            _lightEntity.GetComponent<WorldLight>().Enabled = false;
            return;
        }

        // If we're not pointing at the same entity as last frame, remove the lock
        // on that entity
        if (rayHit.HitEntity != _lastHoveredEntity)
        {
            Reset();
        }

        if (rayHit.HitEntity.HasComponent<DynamicPhysicsActor>())
        {
            var t = _lightEntity.Transform;
            t.Position = rayHit.WorldHitPos;
            _lightEntity.Transform = t;

            _lastHoveredEntity = rayHit.HitEntity;
            var dpa = rayHit.HitEntity.GetComponent<DynamicPhysicsActor>();
            Vector4 col = new Vector4(1f, 0f, 0f, 1f);

            if (_hoveringEntity)
                col = new Vector4(0f, 1f, 0f, 1f);

            if (_bringToHand)
                col = new Vector4(0f, 0f, 1f, 1f);

            _lightEntity.GetComponent<WorldLight>().Enabled = true;
            //DebugShapes.DrawSphere(rayHit.WorldHitPos, 0.5f, Quaternion.Identity, col);

            // Lock on to the entity the player's pointing at
            if (_grabAction.Pressed)
            {
                _bringToHand = false;
                _hoveringEntity = true;
                _floatingTargetPos = dpa.Pose.Position + Vector3.Up * 0.5f;
            }

            // Unlock the entity the player's pointing at
            if (_grabAction.Released)
            {
                _hoveringEntity = false;
                _bringToHand = false;
                _lightEntity.GetComponent<WorldLight>().Enabled = false;
            }

            // Player wants to pick up the object, go to that
            if (_hoveringEntity && _triggerAction.Pressed)
            {
                _bringToHand = true;
                _lockedEntity = rayHit.HitEntity;
                PidController.ResetState();
                _lightEntity.GetComponent<WorldLight>().Enabled = false;
            }
        }
        else
        {
            _lightEntity.GetComponent<WorldLight>().Enabled = false;
        }
    }

    private Vector3 CalculateBallisticAcceleration(Vector3 from, Vector3 to)
    {
        const float time = 0.5f;
        const float timeSq = time * time;
        // Handle Y separately
        float yDistance = to.y - from.y;
        float uY = (yDistance - 0.5f * -9.81f * timeSq) / time;

        Vector3 uXZ = (to - from) / time;
        uXZ.y = uY;

        return uXZ;
    }

    private void Reset()
    {
        _bringToHand = false;
        _hoveringEntity = false;
        _lockedEntity = Entity.Null;
    }
}

