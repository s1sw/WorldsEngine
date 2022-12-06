using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WorldsEngine
{
    [Serializable]
    public class ComponentDestroyedException : Exception
    {
        public ComponentDestroyedException() : base("An attempt was made to access a destroyed component.") { }
        public ComponentDestroyedException(string message) : base(message) { }
        public ComponentDestroyedException(string message, Exception inner) : base(message, inner) { }
        protected ComponentDestroyedException(
          System.Runtime.Serialization.SerializationInfo info,
          System.Runtime.Serialization.StreamingContext context) : base(info, context) { }
    }
}
