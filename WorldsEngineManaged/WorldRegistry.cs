using System;
using System.Text;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using System.Collections;
using System.Threading;

namespace WorldsEngine
{
    interface IComponentStorage
    {
    }

    class ComponentStorage<T> : IComponentStorage
    {
        public T[] components = new T[1000];
        public BitArray slotFree = new BitArray(1000, true);
        public static readonly int typeIndex;
        public static readonly Type type;

        static ComponentStorage()
        {
            typeIndex = Interlocked.Increment(ref Registry.typeCounter);
            type = typeof(T);
        }

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
        public int typeIndex;
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
        private IComponentStorage[] componentStorages = new IComponentStorage[32];
        private Dictionary<uint, EntityInfo> entityInfo = new Dictionary<uint, EntityInfo>();

        internal static int typeCounter = 0;

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
            var typeIndex = ComponentStorage<T>.typeIndex;

            if (componentStorages[typeIndex] == null)
                componentStorages[typeIndex] = new ComponentStorage<T>();

            var storage = (ComponentStorage<T>)componentStorages[ComponentStorage<T>.typeIndex];
            var entInfo = entityInfo[entity.ID];

            foreach (var componentInfo in entInfo.components)
            {
                if (componentInfo.typeIndex == typeIndex)
                    return ref storage.components[componentInfo.index];
            }

            throw new ArgumentException("Tried to get component that isn't there");
        }

        public void SetComponent<T>(Entity entity, T component) where T : struct
        {
            var type = typeof(T);
            var typeIndex = ComponentStorage<T>.typeIndex;

            if (componentStorages[typeIndex] == null)
                componentStorages[typeIndex] = new ComponentStorage<T>();

            var storage = (ComponentStorage<T>)componentStorages[typeIndex];

            if (!entityInfo.ContainsKey(entity.ID))
                entityInfo.Add(entity.ID, new EntityInfo());

            var entInfo = entityInfo[entity.ID];

            foreach (var componentInfo in entInfo.components)
            {
                if (componentInfo.typeIndex == typeIndex)
                    storage.components[componentInfo.index] = component;
            }

            int freeIndex = storage.GetFreeIndex();
            storage.components[freeIndex] = component;
            storage.slotFree[freeIndex] = false;

            var eci = new EntityComponentInfo();
            eci.index = freeIndex;
            eci.typeIndex = typeIndex;

            entInfo.components.Add(eci);
        }

        public void RemoveComponent<T>(Entity entity) where T : struct
        {
            var type = typeof(T);
            var typeIndex = ComponentStorage<T>.typeIndex;

            if (componentStorages[typeIndex] == null)
                componentStorages[typeIndex] = new ComponentStorage<T>();

            var storage = (ComponentStorage<T>)componentStorages[typeIndex];

            if (!entityInfo.ContainsKey(entity.ID))
                throw new ArgumentException("Non-existent entity");

            var entInfo = entityInfo[entity.ID];
            int componentIndex = entInfo.components.FindIndex(c => c.typeIndex == typeIndex);
            var componentInfo = entInfo.components[componentIndex];

            storage.slotFree[componentInfo.index] = true;
            entInfo.components.RemoveAt(componentIndex);
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
