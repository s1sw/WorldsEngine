using System;

namespace WorldsEngine
{
    public abstract class BuiltinComponent
    {
        protected readonly IntPtr regPtr;
        protected readonly uint entityId;

        protected BuiltinComponent(IntPtr regPtr, uint entityId)
        {
            this.regPtr = regPtr;
            this.entityId = entityId;
        }

        public static bool operator ==(BuiltinComponent a, BuiltinComponent b)
        {
            return a.GetType() == b.GetType() && a.entityId == b.entityId;
        }

        public static bool operator !=(BuiltinComponent a, BuiltinComponent b)
        {
            return a.GetType() != b.GetType() || a.entityId != b.entityId;
        }

        public override bool Equals(object? obj)
        {
            if (obj == null || GetType().Equals(obj.GetType())) return false;

            var comp = (BuiltinComponent)obj;

            return comp == this;
        }

        public override int GetHashCode()
        {
            return (int)entityId;
        }
    }
}
