using System;

namespace WorldsEngine
{
    internal class WorldsEngine
    {
        static bool Init()
        {
            Console.WriteLine("Initialising managed libraries");
            return true;
        }

        static void OnSceneStart()
        {
            Logger.Log("Scene started!");
        }

        static void Update(float deltaTime)
        {
            using (ImGui.Window("Hello from .NET!"))
            {
                ImGui.Text("Hey, how's it goin'? :)");

                if (ImGui.Button("Click me!"))
                {
                    Logger.Log("Tee-hee");
                }
            }
        }
    }
}
