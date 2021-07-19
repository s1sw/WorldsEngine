using System;
using WorldsEngine;

namespace Game
{
    struct TestComponent
    {
        public int testValue;
        public int testValue2;
    }

    public class TestSystem : ISystem
    {
        private Registry registry;

        public TestSystem(Registry registry)
        {
            this.registry = registry;
        }

        public void OnSceneStart()
        {
            for (int i = 0; i < 5; i++)
            {
                Entity ent = registry.Create();
                TestComponent component;
                component.testValue = 1337;
                component.testValue2 = 1234;
                registry.SetComponent(ent, component);
                Logger.Log($"New entity is {ent.ID}");
            }
        }

        public void OnUpdate(float deltaTime)
        {
            if (ImGui.Begin("WOOOOOO"))
            {
                foreach (Entity ent in registry.View<TestComponent>())
                {
                    ref TestComponent tc = ref registry.GetComponent<TestComponent>(ent);
                    ImGui.Text($"tv: {tc.testValue}, tv2: {tc.testValue2}");
                    tc.testValue++;
                }

                ImGui.End();
            }

        }
    }
}
