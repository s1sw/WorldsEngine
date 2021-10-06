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

namespace Game
{
    [Component]
    [EditorFriendlyName("C# Player Rig")]
    public class PlayerRig : IThinkingComponent, IStartListener
    {
        const float LocosphereRadius = 0.15f;

        [EditableClass]
        public V3PidController pidController = new V3PidController();

        private bool _grounded = false;
        private bool _groundedLast = false;
        private VRAction _movementAction;
        private Vector3 _lastHMDPos = Vector3.Zero;
        private float _footstepTimer = 0.0f;

        private Vector2 GetInputVelocity()
        {
            if (FreecamSystem.Enabled) return Vector2.Zero;
            Vector2 inputVel = new Vector2();

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

                inputVel = _movementAction.Vector2Value;
                inputVel.x = -inputVel.x;
            }

            return inputVel;
        }

        public void Think(Entity entity)
        {
            var dpa = Registry.GetComponent<DynamicPhysicsActor>(entity);

            Vector2 inputVel = GetInputVelocity();
            inputVel.Normalize();

            Vector3 inputDir = new Vector3(inputVel.x, 0.0f, inputVel.y);

            Vector3 inputDirCS = Camera.Main.Rotation * inputDir;

            if (VR.Enabled)
            {
                inputDirCS = VR.HMDTransform.Rotation * Camera.Main.Rotation * inputDir;
            }

            inputDirCS.y = 0.0f;
            inputDirCS.Normalize();

            _grounded = Physics.Raycast(
                dpa.Pose.Position,
                Vector3.Down,
                out RaycastHit hit,
                1.1f,
                PhysicsLayers.Player
            );

            Vector3 targetPosition = hit.WorldHitPos + (Vector3.Up * 1.1f);

            if (_grounded && PlayerRigSystem.Jump)
            {
                dpa.Velocity = (dpa.Velocity * new Vector3(1.0f, 0.0f, 1.0f)) + Vector3.Up * 6.0f;
                Audio.PlayOneShotEvent("event:/Player/Jump", Vector3.Zero);
            }
            PlayerRigSystem.Jump = false;

            if (_grounded && !PlayerRigSystem.Jump)
                dpa.AddForce(pidController.CalculateForce(targetPosition - dpa.Pose.Position, Time.DeltaTime));

            Vector3 targetVelocity = inputDirCS * 7.5f;
            Vector3 appliedVelocity = (targetVelocity - dpa.Velocity) * (_grounded ? 10f : 2.5f);
            appliedVelocity.y = 0.0f;

            if (_grounded || targetVelocity.LengthSquared > 0.01f)
                dpa.AddForce(appliedVelocity, ForceMode.Acceleration);

            if (_grounded && !_groundedLast)
            {
                Audio.PlayOneShotEvent("event:/Player/Land", Vector3.Zero);
            }

            if (VR.Enabled)
            {
                Vector3 movement = VR.HMDTransform.Position - _lastHMDPos;
                movement.y = 0f;
                var pose = dpa.Pose;
                pose.Position += movement;
                dpa.Pose = pose;
                _lastHMDPos = VR.HMDTransform.Position;
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

            _groundedLast = _grounded;
        }

        public void Start(Entity entity)
        {
            if (VR.Enabled)
                _lastHMDPos = VR.HMDTransform.Position;
        }
    }

    [SystemUpdateOrder(-2)]
    public class PlayerRigSystem : ISystem
    {
        public static Entity PlayerBody { get; private set; }
        public static Entity PlayerFender { get; private set; }

        public static bool Jump = false;

        private VRAction _jumpAction;

        private static AssetID[] _footstepNoises;
        private static Random _footstepRng = new();

        public static AssetID GetRandomFootstepSound()
        {
            return _footstepNoises[_footstepRng.Next(0, 10)];
        }

        public void OnSceneStart()
        {
            if (VR.Enabled)
                _jumpAction = new VRAction("/actions/main/in/Jump");

            PlayerBody = Registry.Find("Player Body");
            PlayerFender = Registry.Find("Fender");

            _footstepNoises = new AssetID[10];
            for (int i = 1; i <= 10; i++)
            {
                string path = "Audio/SFX/Footsteps/Concrete/step" +
                    (i < 10 ? "0" : "") + i + ".ogg";
                _footstepNoises[i - 1] = AssetDB.PathToId(path);
            }
        }

        public void OnUpdate()
        {
            if (Keyboard.KeyPressed(KeyCode.Space))
                Jump = true;

            if (VR.Enabled && _jumpAction.Held)
            {
                Jump = true;
            }
        }

    }
}
