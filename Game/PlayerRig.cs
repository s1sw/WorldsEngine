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
        [EditableClass]
        public V3PidController pidController = new V3PidController();

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

            return inputVel;
        }

        public void Think(Entity entity)
        {
            var dpa = Registry.GetComponent<DynamicPhysicsActor>(entity);

            Vector2 inputVel = GetInputVelocity();
            inputVel.Normalize();

            Vector3 inputDir = new Vector3(inputVel.x, 0.0f, inputVel.y);

            Vector3 inputDirCS = Camera.Main.Rotation * inputDir;
            inputDirCS.y = 0.0f;
            inputDirCS.Normalize();

            Vector3 desiredAngVel = new Vector3(inputDirCS.z, 0.0f, -inputDirCS.x) * 25.0f;

            Vector3 currentAngVel = dpa.AngularVelocity;

            ImGui.Text($"Ang vel: {currentAngVel}");
            Vector3 torque = pidController.CalculateForce(desiredAngVel - currentAngVel, Time.DeltaTime);

            dpa.AddTorque(torque);
        }
    }

    public class PlayerRigSystem : ISystem
    {
        private PhysicsMaterial physMat = new PhysicsMaterial(15.0f, 15.0f, 0.2f);

        public void OnSceneStart()
        {
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
    }
}
