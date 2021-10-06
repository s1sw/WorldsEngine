using System;

namespace WorldsEngine
{
    [Serializable]
    public class MissingComponentException : Exception
    {
        public MissingComponentException() : base("An attempt was made to get a component from an entity that doesn't have one attached.") { }
        public MissingComponentException(string message) : base(message) { }
        public MissingComponentException(string message, Exception inner) : base(message, inner) { }
        protected MissingComponentException(
          System.Runtime.Serialization.SerializationInfo info,
          System.Runtime.Serialization.StreamingContext context) : base(info, context) { }
    }
}
