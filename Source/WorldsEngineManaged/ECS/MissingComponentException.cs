using System;

namespace WorldsEngine
{
    [Serializable]
    public class MissingComponentException : Exception
    {
        public MissingComponentException() : base("An attempt was made to get a component from an entity that doesn't have one attached.") { }
        public MissingComponentException(string componentName) : base($"An attempt was made to get a component ({componentName}) from an entity that doesn't have that component.") { }
        public MissingComponentException(Exception inner) : base("An attempt was made to get a component from an entity that doesn't have one attached.", inner) { }
        protected MissingComponentException(
          System.Runtime.Serialization.SerializationInfo info,
          System.Runtime.Serialization.StreamingContext context) : base(info, context) { }
    }
}
