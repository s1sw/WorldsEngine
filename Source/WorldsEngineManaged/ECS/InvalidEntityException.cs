using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WorldsEngine
{

    [Serializable]
    public class InvalidEntityException : Exception
    {
        public InvalidEntityException() : base("An attempt was made to access an invalid entity.") { }
        public InvalidEntityException(string message) : base(message) { }
        public InvalidEntityException(string message, Exception inner) : base(message, inner) { }
        protected InvalidEntityException(
          System.Runtime.Serialization.SerializationInfo info,
          System.Runtime.Serialization.StreamingContext context) : base(info, context) { }
    }
}
