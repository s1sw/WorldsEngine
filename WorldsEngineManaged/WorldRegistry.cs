using System;
using System.Text;
using System.Runtime.InteropServices;

namespace WorldsEngine
{
    class ComponentStorage<T>
    {
        public Dictionary<uint, T> component;
    }

    class Registry
    {
        [DllImport(WorldsEngine.NATIVE_MODULE)]
        static extern void registry_getTransform(IntPtr regPtr, uint entityId, IntPtr transformPtr);

        [DllImport(WorldsEngine.NATIVE_MODULE)]
        static extern void registry_setTransform(IntPtr regPtr, uint entityId, IntPtr transformPtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void EntityCallbackDelegate(uint entityId);

        [DllImport(WorldsEngine.NATIVE_MODULE)]
        static extern void registry_eachTransform(IntPtr regPtr, EntityCallbackDelegate del);

        [DllImport(WorldsEngine.NATIVE_MODULE)]
        static extern uint registry_getEntityNameLength(IntPtr regPtr, uint entityId);

        [DllImport(WorldsEngine.NATIVE_MODULE)]
        static extern void registry_getEntityName(IntPtr regPtr, uint entityId, StringBuilder str);

        private IntPtr nativeRegistryPtr;

        internal Registry(IntPtr nativePtr)
        {
            nativeRegistryPtr = nativePtr;
        }

        public T GetComponent<T>(Entity entity) where T : struct
        {
            var type = typeof(T);
            throw new NotImplementedException();
        }

        public void SetComponent<T>(Entity entity, T component) where T : struct
        {
            throw new NotImplementedException();
        }

        public Transform GetTransform(Entity entity)
        {
            Transform t = new Transform();
            unsafe
            {
                Transform* tPtr = &t;
                registry_getTransform(nativeRegistryPtr, entity.ID, (IntPtr)tPtr);
            }
            return t;
        }

        public void SetTransform(Entity entity, Transform t)
        {
            unsafe
            {
                Transform* tPtr = &t;
                registry_setTransform(nativeRegistryPtr, entity.ID, (IntPtr)tPtr);
            }
        }

        public bool HasName(Entity entity)
        {
            uint length = registry_getEntityNameLength(nativeRegistryPtr, entity.ID);
            return length != uint.MaxValue;
        }

        public string GetName(Entity entity)
        {
            uint length = registry_getEntityNameLength(nativeRegistryPtr, entity.ID);

            if (length == uint.MaxValue)
                return null;

            StringBuilder sb = new StringBuilder((int)length);
            registry_getEntityName(nativeRegistryPtr, entity.ID, sb);
            return sb.ToString();
        }

        private static Action<Entity> currentEntityFunction;

        private static void EntityCallback(uint entityId)
        {
            currentEntityFunction(new Entity(entityId));
        }

        public void Each(Action<Entity> function)
        {
            currentEntityFunction = function;
            registry_eachTransform(nativeRegistryPtr, EntityCallback);
            currentEntityFunction = null;
        }
    }
}
