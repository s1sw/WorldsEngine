using System;
using System.Text;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using System.Collections;

namespace WorldsEngine
{
    interface IComponentStorage
    {
    }

    class ComponentStorage<T> : IComponentStorage
    {
        public T[] components = new T[1000];
        public BitArray slotFree = new BitArray(1000);

        public int GetFreeIndex()
        {
            for (int i = 0; i < slotFree.Length; i++)
            {
                if (slotFree[i])
                    return i;
            }

            throw new ApplicationException("Out of component slots");
        }
    }

    public class BuiltinComponent
    {
    }

    struct EntityComponentInfo
    {
        public int index;
        public Type type;
    }

    class EntityInfo
    {
        public List<EntityComponentInfo> components = new List<EntityComponentInfo>();
    }

    public class Registry
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

        [DllImport(WorldsEngine.NATIVE_MODULE)]
        static extern void registry_destroy(IntPtr regPtr, uint entityId);

        private IntPtr nativeRegistryPtr;
        private Dictionary<Type, IComponentStorage> componentStorages = new Dictionary<Type, IComponentStorage>();
        private Dictionary<uint, EntityInfo> entityInfo = new Dictionary<uint, EntityInfo>();

        internal Registry(IntPtr nativePtr)
        {
            nativeRegistryPtr = nativePtr;
        }

        public T GetBuiltinComponent<T>(Entity entity)
        {
            var type = typeof(T);

            if (type == typeof(WorldObject))
            {
                return (T)(object)new WorldObject(nativeRegistryPtr, entity.ID);
            }
            else
            {
                throw new ArgumentException($"Type {type.FullName} is not a builtin component.");
            }
        }

        public bool HasBuiltinComponent<T>(Entity entity)
        {
            var type = typeof(T);

            if (type == typeof(WorldObject))
            {
                return WorldObject.ExistsOn(nativeRegistryPtr, entity);
            }
            else
            {
                throw new ArgumentException($"Type {type.FullName} is not a builtin component.");
            }
        }

        public ref T GetComponent<T>(Entity entity) where T : struct
        {
            var type = typeof(T);

            if (!componentStorages.ContainsKey(type))
            {
                componentStorages.Add(type, new ComponentStorage<T>());
            }

            var storage = (ComponentStorage<T>)componentStorages[type];
            var entInfo = entityInfo[entity.ID];

            foreach (var componentInfo in entInfo.components)
            {
                if (componentInfo.type == type)
                    return ref storage.components[componentInfo.index];
            }

            throw new ArgumentException("Tried to get component that isn't there");
        }

        public void SetComponent<T>(Entity entity, T component) where T : struct
        {
            var type = typeof(T);

            if (!componentStorages.ContainsKey(type))
            {
                componentStorages.Add(type, new ComponentStorage<T>());
            }

            var storage = (ComponentStorage<T>)componentStorages[type];

            if (!entityInfo.ContainsKey(entity.ID))
                entityInfo.Add(entity.ID, new EntityInfo());

            var entInfo = entityInfo[entity.ID];

            foreach (var componentInfo in entInfo.components)
            {
                if (componentInfo.type == type)
                    storage.components[componentInfo.index] = component;
            }

            int freeIndex = storage.GetFreeIndex();
            storage.components[freeIndex] = component;

            var eci = new EntityComponentInfo();
            eci.index = freeIndex;
            eci.type = type;

            entInfo.components.Add(eci);
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

#region Entity Iteration
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
#endregion

        public void Destroy(Entity entity)
        {
            registry_destroy(nativeRegistryPtr, entity.ID);
        }
    }
}
