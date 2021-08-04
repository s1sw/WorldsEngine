using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine;
using WorldsEngine.Input;
using System.Threading;

namespace Game
{
    [Component]
    class TestComponent
    {
        public int testValue = 512;
        public int testValue2 = 535;
    }

    public class TestSystem : ISystem
    {
        public void OnUpdate(float deltaTime)
        {
            foreach (Entity e in Registry.View<TestComponent>())
            {
                TestComponent tc = Registry.GetComponent<TestComponent>(e);

                ImGui.Text($"testValue: {tc.testValue}");
                ImGui.Text($"testValue2: {tc.testValue2}");
            }
        }
    }
}
