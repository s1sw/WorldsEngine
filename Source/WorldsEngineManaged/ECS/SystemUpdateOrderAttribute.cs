using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WorldsEngine
{
    [AttributeUsage(AttributeTargets.Class)]
    public class SystemUpdateOrderAttribute : Attribute
    {
        internal int Priority;

        public SystemUpdateOrderAttribute(int priority)
        {
            Priority = priority;
        }
    }
}
