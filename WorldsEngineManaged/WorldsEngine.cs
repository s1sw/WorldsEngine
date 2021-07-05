using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
namespace WorldsEngine
{
    internal class WorldsEngine
    {
        static bool Init()
        {
            Console.WriteLine("Initialising managed libraries");
            return true;
        }

        static void Update(float deltaTime)
        {
            using (ImGui.Window("Hello from .NET!"))
            {
                ImGui.Text("Hey, how's it goin'? :)");
            }
        }
    }
}
