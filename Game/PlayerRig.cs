using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine;
using WorldsEngine.Input;
using WorldsEngine.Math;
using WorldsEngine.Audio;
using ImGuiNET;

namespace Game;

[Component]
[EditorFriendlyName("Player Rig")]
public class PlayerRig : Component, IThinkingComponent, IStartListener
{
    public const float HoverDistance = 0.2f;
    [EditableClass]
    public V3PidController pidController = new();

    private bool _grounded = false;
    private bool _groundedLast = false;
    private Vector3 _lastHMDPos = Vector3.Zero;
    private float _footstepTimer = 0.0f;
    private float _timeSinceDodge = 1000.0f;
    private float _timeSinceJump = 0.0f;
    private float _airTime = 0.0f;
    private Vector3 _lastDodgeDirection = Vector3.Zero;

    private float _timeSinceLastPeak = 0f;
    private bool _peakHit = false;
    private int _lastPeakSign = 0;

    private void UpdateDodge()
    {
        var dpa = Entity.GetComponent<DynamicPhysicsActor>();

        _timeSinceDodge += Time.DeltaTime;
        _timeSinceLastPeak += Time.DeltaTime;

        Vector2 inputVel = LocalPlayerSystem.MovementInput;
        if (_timeSinceDodge > 0.5f)
        {
            if (Keyboard.KeyPressed(KeyCode.NumberRow1))
            {
                Vector3 dodgeDir = Camera.Main.Rotation * Vector3.Left;
                _lastDodgeDirection = dodgeDir;
                LocalPlayerSystem.AddForceToRig(Camera.Main.Rotation * Vector3.Left * 15f, ForceMode.VelocityChange);
                _timeSinceDodge = 0.0f;
            }

            if (Keyboard.KeyPressed(KeyCode.NumberRow2))
            {
                Vector3 dodgeDir = Camera.Main.Rotation * Vector3.Right;
                _lastDodgeDirection = dodgeDir;
                LocalPlayerSystem.AddForceToRig(Camera.Main.Rotation * Vector3.Right * 15f, ForceMode.VelocityChange);
                _timeSinceDodge = 0.0f;
            }

            if (MathF.Abs(inputVel.x) > 0.5f)
            {
                int peakSign = MathF.Sign(inputVel.x);
                if (_timeSinceLastPeak > 0.2f)
                {
                    // we hit a peak!
                    _peakHit = true;
                    _lastPeakSign = MathF.Sign(inputVel.x);
                }
                else if (!_peakHit && _lastPeakSign == peakSign)
                {
                    // dodge time
                    Vector3 dodgeDir = dpa.Pose.Rotation * (inputVel.x < 0f ? Vector3.Right : Vector3.Left);
                    _lastDodgeDirection = dodgeDir;
                    LocalPlayerSystem.AddForceToRig(dodgeDir * 15f, ForceMode.VelocityChange);
                    _timeSinceDodge = 0.0f;
                }
            }

            if (MathF.Abs(inputVel.x) < 0.1f && _peakHit)
            {
                _peakHit = false;
                _timeSinceLastPeak = 0f;
            }
        }
    }

    private void UpdateSound(Vector3 inputDirCS)
    {
        if (_grounded && !_groundedLast && _airTime > 0.1f)
        {
            Audio.PlayOneShotEvent("event:/Player/Land", Vector3.Zero);
        }

        if (inputDirCS.LengthSquared > 0.0f && _grounded)
        {
            _footstepTimer += Time.DeltaTime * 2f;
        }

        if (_footstepTimer >= 1.0f)
        {
            Audio.PlayOneShotEvent("event:/Player/Walking", Vector3.Zero);
            _footstepTimer = 0f;
        }
    }

    private void ApplyBodyRotation(DynamicPhysicsActor dpa)
    {
        Vector3 lookDir;
        if (VR.Enabled)
        {
            lookDir = (VRTransforms.HMDTransform.Rotation * Camera.Main.Rotation).Forward;
        }
        else
        {
            lookDir = Camera.Main.Rotation.Forward;
        }

        lookDir.y = 0.0f;
        lookDir.Normalize();
        var pose = dpa.Pose;
        pose.Rotation = Quaternion.LookAt(lookDir, Vector3.Up);
        dpa.Pose = pose;
    }

    public void Think()
    {
        var dpa = Registry.GetComponent<DynamicPhysicsActor>(Entity);

        Vector2 inputVel = LocalPlayerSystem.MovementInput;
        float max = MathF.Max(MathF.Abs(inputVel.x), MathF.Abs(inputVel.y));
        inputVel.Normalize();

        Vector3 inputDir = new(inputVel.x, 0.0f, inputVel.y);

        Vector3 inputDirCS = Camera.Main.Rotation * inputDir;

        if (VR.Enabled)
        {
            inputDirCS = VRTransforms.HMDTransform.Rotation * Camera.Main.Rotation * inputDir;
        }

        inputDirCS.y = 0.0f;
        inputDirCS.Normalize();

        ApplyBodyRotation(dpa);

        bool nearGround = Physics.SweepSphere(
            dpa.Pose.Position + (Vector3.Down * 0.8f),
            0.1f,
            Vector3.Down,
            0.3f,
            out RaycastHit hit,
            PhysicsLayers.Player
        );

        _grounded = nearGround && hit.Distance < 0.2f + HoverDistance;

        Vector3 targetPosition = hit.WorldHitPos + (Vector3.Up * (1f + HoverDistance));

        if (_grounded && LocalPlayerSystem.Jump && _timeSinceJump > 0.1f)
        {
            Vector3 velChange = (dpa.Velocity * new Vector3(0.0f, -1.0f, 0.0f)) + Vector3.Up * 4.0f;
            LocalPlayerSystem.AddForceToRig(velChange, ForceMode.VelocityChange);
            _timeSinceJump = 0.0f;
            Audio.PlayOneShotEvent("event:/Player/Jump", Vector3.Zero);
        }

        LocalPlayerSystem.ConsumeJump();

        // Add floating force if we're near the ground
        if (_timeSinceJump > 0.3f && nearGround && hit.Distance != 0f)
        {
            Vector3 force = pidController.CalculateForce(targetPosition - dpa.Pose.Position, Time.DeltaTime);
            force.x = 0.0f;
            force.z = 0.0f;
            LocalPlayerSystem.AddForceToRig(force, ForceMode.Force);
        }

        if (!_grounded && _groundedLast)
            _airTime = 0.0f;

        if (!_grounded)
            _airTime += Time.DeltaTime;

        Vector3 targetVelocity = inputDirCS * 7.5f * max;
        Vector3 appliedVelocity = (targetVelocity - dpa.Velocity) * (_grounded ? 10f : 2.5f);
        appliedVelocity.y = 0.0f;

        if (_timeSinceDodge < 0.1f)
        {
            appliedVelocity -= _lastDodgeDirection * Vector3.Dot(appliedVelocity, _lastDodgeDirection) * (1.0f - (_timeSinceDodge * 10f));
        }

        if (_grounded || targetVelocity.LengthSquared > 0.01f)
        {
            LocalPlayerSystem.AddForceToRig(appliedVelocity, ForceMode.Acceleration);
        }

        // Take into account roomscale movement
        // For now, just change the pose directly
        if (VR.Enabled)
        {
            Vector3 movement = VRTransforms.HMDTransform.Position - _lastHMDPos;
            movement.y = 0f;
            var pose = dpa.Pose;
            pose.Position += Camera.Main.Rotation * movement;
            dpa.Pose = pose;
            _lastHMDPos = VRTransforms.HMDTransform.Position;
        }

        UpdateSound(inputDirCS);
        UpdateDodge();

        _groundedLast = _grounded;
        _timeSinceJump += Time.DeltaTime;
    }

    public void Start()
    {
        if (VR.Enabled)
            _lastHMDPos = VRTransforms.HMDTransform.Position;
    }
}

[SystemUpdateOrder(-2)]
public class LocalPlayerSystem : ISystem
{
    public static Entity PlayerBody { get; private set; }
    public static bool Jump { get; private set; }
    public static Vector2 MovementInput { get; private set; }
    public static Quaternion VirtualRotation { get; private set; } = Quaternion.Identity;

    private VRAction _jumpAction;
    private Entity _hpTextEntity;
    private VRAction _movementAction;
    private VRAction _rightStickAction;
    private bool _snapTurned = false;
    private static Entity _leftHandEntity;
    private static Entity _rightHandEntity;

    public void OnSceneStart()
    {
        if (VR.Enabled)
            _jumpAction = new VRAction("/actions/main/in/Jump");

        PlayerBody = Registry.Find("Player Body");

        if (PlayerBody.IsNull) return;

        var healthComp = Registry.GetComponent<Combat.HealthComponent>(PlayerBody);
        healthComp.OnDeath += (Entity e) => {
            SceneLoader.LoadScene(SceneLoader.CurrentSceneID);
        };

        _hpTextEntity = Registry.Create();
        Registry.AddComponent<WorldText>(_hpTextEntity);
        _leftHandEntity = Registry.Find("LeftHand");
        _rightHandEntity = Registry.Find("RightHand");
    }

    public void OnUpdate()
    {
        if (PlayerBody.IsNull) return;

        if (Keyboard.KeyPressed(KeyCode.Space))
            Jump = true;

        if (VR.Enabled && _jumpAction.Pressed)
        {
            Jump = true;
        }

        var hpText = Registry.GetComponent<WorldText>(_hpTextEntity);
        hpText.Size = 0.001f;
        hpText.Text = $"HP: {Registry.GetComponent<Combat.HealthComponent>(PlayerBody).Health}";

        var lhTransform = Registry.GetTransform(_leftHandEntity);
        var hpTransform = Registry.GetTransform(_hpTextEntity);
        hpTransform.Rotation = lhTransform.Rotation *
            Quaternion.AngleAxis(MathF.PI / 2f, Vector3.Left) * Quaternion.AngleAxis(MathF.PI, Vector3.Up);
        Vector3 offset = Vector3.Forward * 0.05f + Vector3.Down * 0.02f;
        hpTransform.Position = lhTransform.Position + hpTransform.Rotation * offset;
        Registry.SetTransform(_hpTextEntity, hpTransform);
        MovementInput = GetInputVelocity();
    }

    public static void AddForceToRig(Vector3 force, ForceMode mode = ForceMode.Force)
    {
        var bodyDpa = Registry.GetComponent<DynamicPhysicsActor>(PlayerBody);
        var lhDpa = Registry.GetComponent<DynamicPhysicsActor>(_leftHandEntity);
        var rhDpa = Registry.GetComponent<DynamicPhysicsActor>(_rightHandEntity);

        bodyDpa.AddForce(force, mode);
        if (mode == ForceMode.Force || mode == ForceMode.Impulse)
        {
            force /= bodyDpa.Mass;
        }

        switch (mode)
        {
            case ForceMode.Force:
                lhDpa.AddForce(force, ForceMode.Acceleration);
                rhDpa.AddForce(force, ForceMode.Acceleration);
                break;
            case ForceMode.Impulse:
                lhDpa.AddForce(force, ForceMode.VelocityChange);
                rhDpa.AddForce(force, ForceMode.VelocityChange);
                break;
            case ForceMode.VelocityChange:
            case ForceMode.Acceleration:
                lhDpa.AddForce(force, mode);
                rhDpa.AddForce(force, mode);
                break;
        }
    }

    public static void ConsumeJump()
    {
        Jump = false;
    }

    private Vector2 GetInputVelocity()
    {
        if (FreecamSystem.Enabled) return Vector2.Zero;
        Vector2 inputVel = new();

        if (Keyboard.KeyHeld(KeyCode.W))
        {
            inputVel.y += 1.0f;
        }

        if (Keyboard.KeyHeld(KeyCode.S))
        {
            inputVel.y -= 1.0f;
        }

        if (Keyboard.KeyHeld(KeyCode.A))
        {
            inputVel.x += 1.0f;
        }

        if (Keyboard.KeyHeld(KeyCode.D))
        {
            inputVel.x -= 1.0f;
        }

        if (VR.Enabled)
        {
            if (_movementAction == null)
                _movementAction = new VRAction("/actions/main/in/Movement");

            if (_rightStickAction == null)
                _rightStickAction = new VRAction("/actions/main/in/RStick");

            Vector2 rsVal = _rightStickAction.Vector2Value;
            if (MathF.Abs(rsVal.x) < 0.5f)
                _snapTurned = false;
            else if (!_snapTurned)
            {
                VirtualRotation *= Quaternion.AngleAxis(-MathF.Sign(rsVal.x) * MathF.PI / 4.0f, Vector3.Up);
                _snapTurned = true;
            }

            inputVel = _movementAction.Vector2Value;
            inputVel.x = -inputVel.x;
        }

        return inputVel;
    }
}
