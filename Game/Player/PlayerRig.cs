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
using Game.Interaction;

namespace Game.Player;

[Component]
[EditorFriendlyName("Player Rig")]
public class PlayerRig : Component, IThinkingComponent, IStartListener
{
    public const float NormalMoveSpeed = 5.0f;
    public const float SprintMoveSpeed = 8.0f;

    private bool _grounded = false;
    private bool _groundedLast = false;
    private Vector3 _lastHMDPos = Vector3.Zero;
    private float _footstepTimer = 0.0f;
    private float _timeSinceJump = 0.0f;
    private float _airTime = 0.0f;

    private float _timeSinceLastPeak = 0f;
    private bool _peakHit = false;
    private int _lastPeakSign = 0;
    private bool _isSprinting = false;

    public Vector3 CurrentMovementVector = new();

    private void UpdateSprintToggle()
    {
        _timeSinceLastPeak += Time.DeltaTime;

        Vector2 inputVel = LocalPlayerSystem.MovementInput;
        if (MathF.Abs(inputVel.y) > 0.2f)
        {
            int peakSign = MathF.Sign(inputVel.y);
            if (_timeSinceLastPeak > 0.2f)
            {
                // we hit a peak!
                _peakHit = true;
                _lastPeakSign = MathF.Sign(inputVel.y);
            }
            else if (!_peakHit && _lastPeakSign == peakSign)
            {
                _isSprinting = true;
            }
        }

        if (MathF.Abs(inputVel.y) < 0.1f && _peakHit)
        {
            _peakHit = false;
            _timeSinceLastPeak = 0f;
            _isSprinting = false;
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
            _footstepTimer += Time.DeltaTime * inputDirCS.Length;
            if (_footstepTimer >= 0.45f)
            {
                //Audio.PlayOneShotEvent("event:/Player/Walking", Vector3.Zero);
                _footstepTimer = 0f;
            }
        }
    }

    private void ApplyBodyRotation(DynamicPhysicsActor dpa)
    {
        Vector3 lookDir;
        if (VR.Enabled)
        {
            lookDir = (Camera.Main.Rotation * VRTransforms.HMDTransform.Rotation).Forward;
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

        Vector3 centerOffset = new(0.0f, 0.0f, -0.150f);
        centerOffset = dpa.Pose.Rotation * centerOffset;
        Vector3 ro = dpa.Pose.Position + centerOffset + (Vector3.Down * 0.9f);
        _grounded = Physics.Raycast(
            ro,
            Vector3.Down,
            out RaycastHit hit,
            0.2f,
            PhysicsLayers.Player
        );

        if (_grounded && LocalPlayerSystem.Jump && _timeSinceJump > 0.1f)
        {
            Vector3 velChange = (dpa.Velocity * new Vector3(0.0f, -1.0f, 0.0f)) + Vector3.Up * 4.0f;
            LocalPlayerSystem.PlayerBody.GetComponent<DynamicPhysicsActor>().AddForce(velChange, ForceMode.VelocityChange);
            _timeSinceJump = 0.0f;
            Audio.PlayOneShotEvent("event:/Player/Jump", Vector3.Zero);
        }

        LocalPlayerSystem.ConsumeJump();

        if (!_grounded && _groundedLast)
            _airTime = 0.0f;

        if (!_grounded)
            _airTime += Time.DeltaTime;

        float moveSpeed = _isSprinting ? SprintMoveSpeed : NormalMoveSpeed;
        Vector3 targetVelocity = inputDirCS * moveSpeed * max;
        Vector3 appliedVelocity = (targetVelocity - dpa.Velocity) * (_grounded ? 20f : 0f);
        appliedVelocity.y = 0.0f;

        if (_grounded || targetVelocity.LengthSquared > 0.01f)
        {
            CurrentMovementVector = targetVelocity;
            //LocalPlayerSystem.AddForceToRig(appliedVelocity, ForceMode.Acceleration, false);
        }
        else
        {
            CurrentMovementVector = Vector3.Zero;
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

        UpdateSound(inputDir * max * (moveSpeed / NormalMoveSpeed));
        UpdateSprintToggle();

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
    public static Entity LeftHand => _leftHandEntity;
    public static Entity RightHand => _rightHandEntity;

    public static bool Jump { get; private set; }
    public static Vector2 MovementInput { get; private set; }
    public static Quaternion VirtualRotation = Quaternion.Identity;

    private VRAction _jumpAction;
    private Entity _hpTextEntity;
    private VRAction _movementAction;
    private VRAction _rightStickAction;
    private bool _snapTurned = false;
    private static Entity _leftHandEntity;
    private static Entity _rightHandEntity;
    private static RelativePlayerTransforms? _spawnRPT = null;
    private static Entity climbEntity = Entity.Null;
    private static Vector3 climbPoint = Vector3.Zero;

    private static void ContactModCallback(ContactModPairArray array)
    {
        PlayerRig r = PlayerBody.GetComponent<PlayerRig>();
        var playerPose = PlayerBody.GetComponent<DynamicPhysicsActor>().Pose;

        foreach (ContactModifyPair pair in array)
        {
            if (pair.InvolvesEntity(LeftHand))
            {
                foreach (ModifiableContact contact in pair.ContactSet)
                {
                    if (contact.Normal.y > 0.7f)
                    {
                        climbEntity = pair.EntityA == LeftHand ? pair.EntityB : pair.EntityA;
                        climbPoint = contact.Point;
                    }
                }
            }

            if (pair.InvolvesEntity(RightHand))
            {
                foreach (ModifiableContact contact in pair.ContactSet)
                {
                    if (contact.Normal.y > 0.7f)
                    {
                        climbEntity = pair.EntityA == RightHand ? pair.EntityB : pair.EntityA;
                        climbPoint = contact.Point;
                    }
                }
            }
        }

        foreach (ContactModifyPair pair in array)
        {
            if (!pair.InvolvesEntity(PlayerBody)) continue;
            Entity otherEntity = pair.EntityA == PlayerBody ? pair.EntityB : pair.EntityA;
            bool isOther = pair.EntityB == PlayerBody;

            foreach (ModifiableContact contact in pair.ContactSet)
            {
                Vector3 normal = pair.EntityB == PlayerBody ? -contact.Normal : contact.Normal;

                if (contact.Point.y < playerPose.Position.y - 0.75f)
                {
                    if (normal.Dot(Vector3.Up) < 0.05f)
                    {
                        contact.MaxImpulse = 0.000f;
                        contact.StaticFriction = 0f;
                        contact.DynamicFriction = 0f;
                        contact.TargetVelocity = Vector3.Zero;
                        contact.Normal = Vector3.Up;
                        continue;   
                    }

                    var tv = contact.TargetVelocity;
                    Vector3 projected = r.CurrentMovementVector.ProjectOntoPlane(contact.Normal);
                    if (pair.EntityB == PlayerBody) projected *= -1f;
                    //tv += projected;

                    if (normal.Dot(Vector3.Up) > 0.6f)
                    {
                        tv = projected;
                        contact.DynamicFriction = 5f;
                        contact.StaticFriction = 5f;
                        contact.TargetVelocity = tv;
                    }
                }
            }
        }
    }

    private void SpawnPlayer()
    {
        if (Registry.View<SpawnPoint>().Count == 0) return;

        if (Registry.View<SpawnPoint>().Count > 1)
        {
            Log.Warn("Multiple spawn points!!");
        }

        Entity spawnPointEntity = Registry.View<SpawnPoint>().GetFirst();
        Transform spawnPoint = Registry.GetTransform(spawnPointEntity);

        if (Registry.View<PlayerRig>().Count > 0)
        {
            // Player exists, don't spawn!
            return;
        }

        Log.Msg("Spawning player");
        Entity body = Registry.CreatePrefab(AssetDB.PathToId("Prefabs/player_body.wprefab"));
        Entity lh = Registry.CreatePrefab(AssetDB.PathToId("Prefabs/player_left_hand.wprefab"));
        Entity rh = Registry.CreatePrefab(AssetDB.PathToId("Prefabs/player_right_hand.wprefab"));


        if (_spawnRPT == null)
        {
            VirtualRotation = spawnPoint.Rotation;
            Vector3 offset = new(0.0f, 1.2f, 0.0f);
            spawnPoint.Position += offset;
            spawnPoint.Scale = body.Transform.Scale;
            body.Transform = spawnPoint;
            Vector3 handOffset = new(0.1f, 0.0f, 0.2f);
            spawnPoint.Position += handOffset; 
            spawnPoint.Scale = lh.Transform.Scale;
            lh.Transform = spawnPoint;
            spawnPoint.Position -= handOffset * 2f;
            spawnPoint.Scale = rh.Transform.Scale;
            rh.Transform = spawnPoint;
            Log.Msg($"body pos: {body.Transform.Position}");
        }
        else
        {
            Transform bodyT = _spawnRPT.Value.Body.TransformBy(spawnPoint);
            bodyT.Scale = body.Transform.Scale;

            Transform lhT = _spawnRPT.Value.LeftHand.TransformBy(spawnPoint);
            lhT.Scale = lh.Transform.Scale;

            Transform rhT = _spawnRPT.Value.RightHand.TransformBy(spawnPoint);
            rhT.Scale = rh.Transform.Scale;

            body.Transform = bodyT;
            lh.Transform = lhT;
            rh.Transform = rhT;

            body.GetComponent<DynamicPhysicsActor>().Velocity = spawnPoint.TransformDirection(_spawnRPT.Value.BodyVelocity);
            lh.GetComponent<DynamicPhysicsActor>().Velocity = spawnPoint.TransformDirection(_spawnRPT.Value.LeftHandVelocity);
            rh.GetComponent<DynamicPhysicsActor>().Velocity = spawnPoint.TransformDirection(_spawnRPT.Value.RightHandVelocity);
            VirtualRotation *= spawnPoint.Rotation;

            _spawnRPT = null;
        }
    }

    public void OnSceneStart()
    {
        PlayerResources.Metal = 0;
        MovementInput = Vector2.Zero;
        Camera.Main.Rotation = Quaternion.Identity;
        Camera.Main.Position = Vector3.Zero;
        SpawnPlayer();

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
        Physics.ContactModCallback = ContactModCallback;

        var bodyDpa = PlayerBody.GetComponent<DynamicPhysicsActor>();
        bodyDpa.UseContactMod = true;
        bodyDpa.ContactOffset = 0.00001f;

        _leftHandEntity.GetComponent<DynamicPhysicsActor>().UseContactMod = true;
        _rightHandEntity.GetComponent<DynamicPhysicsActor>().UseContactMod = true;
    }

    public void OnUpdate()
    {
        if (PlayerBody.IsNull) return;

        if (Keyboard.KeyPressed(KeyCode.Space))
            Jump = true;

        if (Controller.ButtonPressed(ControllerButton.A))
            Jump = true;

        if (VR.Enabled && _jumpAction.Pressed)
        {
            Jump = true;
        }

        var hpText = Registry.GetComponent<WorldText>(_hpTextEntity);
        var hc = PlayerBody.GetComponent<Combat.HealthComponent>();
        if (DebugGlobals.PlayerInvincible) hc.Health = hc.MaxHealth;
        hpText.Size = 0.001f;
        hpText.Text = $"HP: {hc.Health}\nMetal: {PlayerResources.Metal}";

        var lhTransform = Registry.GetTransform(_leftHandEntity);
        var hpTransform = Registry.GetTransform(_hpTextEntity);
        hpTransform.Rotation = lhTransform.Rotation *
            Quaternion.AngleAxis(MathF.PI / 2f, Vector3.Left) * Quaternion.AngleAxis(MathF.PI, Vector3.Up);
        Vector3 offset = Vector3.Forward * 0.05f + Vector3.Down * 0.02f;
        hpTransform.Position = lhTransform.Position + hpTransform.Rotation * offset;
        Registry.SetTransform(_hpTextEntity, hpTransform);
        MovementInput = GetInputVelocity();
    }

    public void OnSimulate()
    {
        if (climbEntity.IsValid && LeftHand.Transform.Position.DistanceTo(climbPoint) > 0.3f && RightHand.Transform.Position.DistanceTo(climbPoint) > 0.3f)
        {
            Log.Msg("nulled");
            climbEntity = Entity.Null;
        }
    }

    public static void ConsumeJump()
    {
        Jump = false;
    }

    private Vector2 GetInputVelocity()
    {
        //if (FreecamSystem.Enabled) return Vector2.Zero;
        Vector2 inputVel = new(-Controller.DeadzonedAxisValue(ControllerAxis.LeftX), -Controller.DeadzonedAxisValue(ControllerAxis.LeftY));

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

    public static void SetTransitionSpawn(Transform trigger)
    {
        _spawnRPT = new RelativePlayerTransforms()
        {
            Body = PlayerBody.Transform.TransformByInverse(trigger),
            LeftHand = _leftHandEntity.Transform.TransformByInverse(trigger),
            RightHand = _rightHandEntity.Transform.TransformByInverse(trigger),
            BodyVelocity = PlayerBody.GetComponent<DynamicPhysicsActor>().Velocity,
            LeftHandVelocity = trigger.InverseTransformDirection(_leftHandEntity.GetComponent<DynamicPhysicsActor>().Velocity),
            RightHandVelocity = trigger.InverseTransformDirection(_rightHandEntity.GetComponent<DynamicPhysicsActor>().Velocity)
        };
    }
}

public struct RelativePlayerTransforms
{
    public Transform Body;
    public Transform LeftHand;
    public Transform RightHand;
    public Vector3 BodyVelocity;
    public Vector3 LeftHandVelocity;
    public Vector3 RightHandVelocity;
}
