using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace WorldsEngine
{
    public static class Editor
    {
        [DllImport(WorldsEngine.NativeModule)]
        private static extern uint editor_getCurrentlySelected();

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void editor_select(uint entity);

        public static Entity CurrentlySelected => new Entity(editor_getCurrentlySelected());

        public static void Select(Entity entity)
        {
            editor_select(entity.ID);
        }
    }
}
