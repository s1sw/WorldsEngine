using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WorldsEngine
{
    [AttributeUsage(AttributeTargets.Method)]
    public class ConsoleCommandAttribute : Attribute
    {
        internal string Command;
        internal string Help;

        public ConsoleCommandAttribute(string cmd, string help = null)
        {
            Command = cmd;
            Help = help;
        }
    }
}
