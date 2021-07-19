using System;
using System.Text;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using System.Collections;

namespace WorldsEngine
{
    public class BuiltinComponent
    {
    }

    internal class NativeRegistry
    {
        [DllImport(WorldsEngine.NativeModule)]
        public static extern void registry_getTransform(IntPtr regPtr, uint entityId, IntPtr transformPtr);

        [DllImport(WorldsEngine.NativeModule)]
        public static extern void registry_setTransform(IntPtr regPtr, uint entityId, IntPtr transformPtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void EntityCallbackDelegate(uint entityId);

        [DllImport(WorldsEngine.NativeModule)]
        public static extern void registry_eachTransform(IntPtr regPtr, EntityCallbackDelegate del);

        [DllImport(WorldsEngine.NativeModule)]
        public static extern uint registry_getEntityNameLength(IntPtr regPtr, uint entityId);

        [DllImport(WorldsEngine.NativeModule)]
        public static extern void registry_getEntityName(IntPtr regPtr, uint entityId, StringBuilder str);

        [DllImport(WorldsEngine.NativeModule)]
        public static extern void registry_destroy(IntPtr regPtr, uint entityId);

        [DllImport(WorldsEngine.NativeModule)]
        public static extern uint registry_create(IntPtr regPtr);
    }

    public class Registry
    {
        const int ComponentPoolCount = 32;

        private IntPtr nativeRegistryPtr;
        private IComponentStorage[] componentStorages = new IComponentStorage[ComponentPoolCount];

        internal static int typeCounter = 0;

        internal Registry(IntPtr nativePtr)
        {
            nativeRegistryPtr = nativePtr;
        }

        private ComponentStorage<T> AssureStorage<T>()
        {
            int typeIndex = ComponentStorage<T>.typeIndex;

            if (typeIndex >= ComponentPoolCount)
                throw new ArgumentOutOfRangeException("Out of component pools. Oops.");

            if (componentStorages[typeIndex] == null)
                componentStorages[typeIndex] = new ComponentStorage<T>();

            return (ComponentStorage<T>)componentStorages[typeIndex];
        }

        public T GetBuiltinComponent<T>(Entity entity) where T : BuiltinComponent
        {
            var type = typeof(T);

            if (type == typeof(WorldObject))
            {
                return (T)(object)new WorldObject(nativeRegistryPtr, entity.ID);
            }

            throw new ArgumentException($"Type {type.FullName} is not a builtin component.");
        }

        public bool HasBuiltinComponent<T>(Entity entity) where T : BuiltinComponent
        {
            var type = typeof(T);

            if (type == typeof(WorldObject))
            {
                return WorldObject.ExistsOn(nativeRegistryPtr, entity);
            }

            throw new ArgumentException($"Type {type.FullName} is not a builtin component.");
        }

        public ref T GetComponent<T>(Entity entity) where T : struct
        {
            var storage = AssureStorage<T>();

            return ref storage.Get(entity);
        }

        public void SetComponent<T>(Entity entity, T component) where T : struct
        {
            var type = typeof(T);
            var storage = AssureStorage<T>();

            if (storage.Contains(entity))
            {
                storage.Get(entity) = component;
                return;
            }

            storage.Set(entity, component);
        }

        public void RemoveComponent<T>(Entity entity) where T : struct
        {
            var storage = AssureStorage<T>();

            storage.Remove(entity);
        }

        public Transform GetTransform(Entity entity)
        {
            Transform t = new Transform();
            unsafe
            {
                Transform* tPtr = &t;
                NativeRegistry.registry_getTransform(nativeRegistryPtr, entity.ID, (IntPtr)tPtr);
            }
            return t;
        }

        public void SetTransform(Entity entity, Transform t)
        {
            unsafe
            {
                Transform* tPtr = &t;
                NativeRegistry.registry_setTransform(nativeRegistryPtr, entity.ID, (IntPtr)tPtr);
            }
        }

        public bool HasName(Entity entity)
        {
            uint length = NativeRegistry.registry_getEntityNameLength(nativeRegistryPtr, entity.ID);
            return length != uint.MaxValue;
        }

        public string GetName(Entity entity)
        {
            uint length = NativeRegistry.registry_getEntityNameLength(nativeRegistryPtr, entity.ID);

            if (length == uint.MaxValue)
                return null;

            StringBuilder sb = new StringBuilder((int)length);
            NativeRegistry.registry_getEntityName(nativeRegistryPtr, entity.ID, sb);
            return sb.ToString();
        }

#region Entity Iteration
        private static Action<Entity> currentEntityFunction;

        private static void EntityCallback(uint entityId)
        {
            currentEntityFunction(new Entity(entityId));
        }

        public void Each(Action<Entity> function)
        {
            currentEntityFunction = function;
            NativeRegistry.registry_eachTransform(nativeRegistryPtr, EntityCallback);
            currentEntityFunction = null;
        }
#endregion

        public void Destroy(Entity entity)
        {
            NativeRegistry.registry_destroy(nativeRegistryPtr, entity.ID);
        }

        public Entity Create()
        {
            return new Entity(NativeRegistry.registry_create(nativeRegistryPtr));
        }

        public ComponentStorage<T> View<T>()
        {
            return AssureStorage<T>();
        }
    }
}
