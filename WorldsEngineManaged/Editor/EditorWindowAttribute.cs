using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WorldsEngine.Editor
{
    [AttributeUsage(AttributeTargets.Class)]
    sealed class EditorWindowAttribute : Attribute
    {
        public bool AllowMultipleInstances { get; private set; }

        public EditorWindowAttribute(bool allowMultipleInstances = false)
        {
            AllowMultipleInstances = allowMultipleInstances;
        }
    }
}
