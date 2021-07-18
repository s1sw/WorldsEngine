using System;
using WorldsEngine;

namespace Game
{
    public class TestSystem : ISystem
    {
        private Registry registry;

        public TestSystem(Registry registry)
        {
            this.registry = registry;
        }

        public void OnUpdate(float deltaTime)
        {
            if (ImGui.Begin("WOOOOOO"))
            {
                ImGui.End();
            }
        }
    }
}
