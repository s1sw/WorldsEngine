using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WorldsEngine
{

    [Serializable]
    public class NullEntityException : Exception
    {
        public NullEntityException() { }
        public NullEntityException(string message) : base(message) { }
        public NullEntityException(string message, Exception inner) : base(message, inner) { }
        protected NullEntityException(
          System.Runtime.Serialization.SerializationInfo info,
          System.Runtime.Serialization.StreamingContext context) : base(info, context) { }
    }
}
