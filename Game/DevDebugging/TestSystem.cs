using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine;
using WorldsEngine.Input;
using System.Threading;
using ImGuiNET;
using WorldsEngine.Math;
using WorldsEngine.Util;
using Game.Combat;

namespace Game
{
    public class TestSystem : ISystem
    {
        private PhysicsMaterial physicsMaterial;

        private static void ContactMod(ContactModPairArray pairArray)
        {
            for (int i = 0; i < pairArray.Count; i++)
            {
                var contactSet = pairArray[i].ContactSet;
                for (int j = 0; j < pairArray[i].ContactSet.Count; j++)
                {
                    contactSet.SetTargetVelocity(j, contactSet.GetTargetVelocity(j) + Vector3.Up * 2f);
                }
            }
        }

        public void OnSceneStart()
        {
            physicsMaterial = new PhysicsMaterial(0.25f, 0.25f, 0.5f);
            Physics.ContactModCallback = ContactMod;
        }

        public void OnSimulate()
        {
            if (Keyboard.KeyHeld(KeyCode.K))
            {
                Vector3 camRelativeSpawnPoint = new(0.0f, 0.0f, 1.0f);

                Entity entity = Registry.Create();
                WorldObject wo = Registry.AddComponent<WorldObject>(entity);
                wo.Mesh = AssetDB.PathToId("Models/sphere.wmdl");
                wo.SetMaterial(0, DevMaterials.Blue);

                var transform = Registry.GetTransform(entity);
                transform.Position = Camera.Main.TransformPoint(camRelativeSpawnPoint);
                Registry.SetTransform(entity, transform);

                var dpa = Registry.AddComponent<DynamicPhysicsActor>(entity);
                dpa.Mass = 5.0f;

                var shapes = new PhysicsShape[1] { 
                    new SpherePhysicsShape(0.25f, physicsMaterial)
                };

                dpa.SetPhysicsShapes(shapes);
                dpa.AddForce(Camera.Main.Rotation.Forward * 25.0f, ForceMode.VelocityChange);
                var idd = Registry.AddComponent<ImpactDamageDealer>(entity);
                idd.Damage = 15.0;
                idd.MinimumVelocity = 1.0f;
            }
        }

        public void OnUpdate()
        {
            if (!VR.Enabled) return;

            if (Keyboard.KeyHeld(KeyCode.V))
            {
                var left = new VRAction("/actions/main/out/VibrationLeft");
                var right = new VRAction("/actions/main/out/VibrationRight");
                left.TriggerHaptics(0.0f, 0.1f, 60.0f, 1.0f);
                right.TriggerHaptics(0.0f, 0.1f, 60.0f, 1.0f);
            }
        }
    }
}
