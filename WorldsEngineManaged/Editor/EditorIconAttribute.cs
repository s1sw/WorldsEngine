using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WorldsEngine.Editor
{
    [AttributeUsage(AttributeTargets.Class | AttributeTargets.Struct)]
    public class EditorIconAttribute : Attribute
    {
        public readonly string Icon;

        public EditorIconAttribute(string icon)
        {
            Icon = icon;
        }
    }
}
