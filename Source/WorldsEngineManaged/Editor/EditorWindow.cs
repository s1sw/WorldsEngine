using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WorldsEngine.Editor
{
    /// <summary>
    /// Base class for all managed editor windows. Each instance of this class corresponds to an open editor window.
    /// </summary>
    public abstract class EditorWindow
    {
        public bool IsOpen { get; protected set; }

        public void Open()
        {
            IsOpen = true;
        }

        public abstract void Draw();

        public void Close()
        {
            IsOpen = false;
        }
    }
}
