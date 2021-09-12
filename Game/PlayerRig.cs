using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine;
using WorldsEngine.Input;
using WorldsEngine.Math;
using ImGuiNET;

namespace Game
{
    [Component]
    [EditorFriendlyName("C# Player Rig")]
    public class PlayerRig : IThinkingComponent
    {
        const float LocosphereRadius = 0.15f;

        [EditableClass]
        public V3PidController pidController = new V3PidController();

        private bool _grounded = false;
        private VRAction _movementAction;
        private Vector3 _lastHMDPos = Vector3.Zero;

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

            Vector3 desiredAngVel = new Vector3(inputDirCS.z, 0.0f, -inputDirCS.x) * (Keyboard.KeyHeld(KeyCode.LeftShift) ? 50f : 25.0f);

            Vector3 currentAngVel = dpa.AngularVelocity;

            //ImGui.Text($"Ang vel: {currentAngVel}");
            Vector3 torque = pidController.CalculateForce(desiredAngVel - currentAngVel, Time.DeltaTime);

            dpa.AddTorque(torque);

            _grounded = Physics.Raycast(
                dpa.Pose.Position - new Vector3(0.0f, LocosphereRadius - 0.01f, 0.0f),
                Vector3.Down,
                LocosphereRadius,
                PhysicsLayers.Player
            );

            Transform bodyTransform = Registry.GetTransform(PlayerRigSystem.PlayerBody);
            D6Joint bodyJoint = Registry.GetComponent<D6Joint>(PlayerRigSystem.PlayerBody);
            var bodyDpa = Registry.GetComponent<DynamicPhysicsActor>(PlayerRigSystem.PlayerBody);

            if (_grounded && PlayerRigSystem.Jump)
            {
                var fenderDpa = Registry.GetComponent<DynamicPhysicsActor>(PlayerRigSystem.PlayerFender);

                Vector3 forceVector = Vector3.Up * 5.0f;
                bodyDpa.AddForce(forceVector, ForceMode.VelocityChange);
                dpa.AddForce(forceVector, ForceMode.VelocityChange);
                fenderDpa.AddForce(forceVector, ForceMode.VelocityChange);
                PlayerRigSystem.Jump = false;
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
        }
    }

    [SystemUpdateOrder(-2)]
    public class PlayerRigSystem : ISystem
    {
        public static Entity PlayerBody { get; private set; }
        public static Entity PlayerFender { get; private set; }
        public static Entity PlayerLocosphere { get; private set; }
        private readonly PhysicsMaterial physMat = new(15.0f, 15.0f, 0.0f);

        public static bool Jump = false;

        private VRAction _jumpAction;

        public void OnSceneStart()
        {
            if (VR.Enabled)
                _jumpAction = new VRAction("/actions/main/in/Jump");

            PlayerBody = Registry.Find("Player Body");
            PlayerFender = Registry.Find("Fender");
            PlayerLocosphere = Registry.Find("Player Locosphere");

            physMat.FrictionCombineMode = CombineMode.Max;

            foreach(Entity ent in Registry.View<PlayerRig>())
            {
                var dpa = Registry.GetComponent<DynamicPhysicsActor>(ent);

                List<PhysicsShape> shapes = dpa.GetPhysicsShapes();

                foreach (PhysicsShape shape in shapes)
                {
                    shape.physicsMaterial = physMat;
                }

                dpa.SetPhysicsShapes(shapes);
            }
        }

        public void OnUpdate()
        {
            if (Keyboard.KeyPressed(KeyCode.Space))
                Jump = true;

            if (VR.Enabled && _jumpAction.Held)
            {
                Logger.Log("jump!!");
                Jump = true;
            }
        }
    }
}
