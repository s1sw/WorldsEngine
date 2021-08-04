using System;
using System.Text;
using System.Runtime.InteropServices;
using System.Reflection;
using System.Text.Json;
using System.Collections.Generic;

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

        [DllImport(WorldsEngine.NativeModule)]
        public static extern void registry_setSerializedEntityInfo(IntPtr serializationContext, IntPtr key, IntPtr value);

        [DllImport(WorldsEngine.NativeModule)]
        public static extern uint registry_createPrefab(IntPtr regPtr, uint assetId);
    }

    public static class Registry
    {
        const int ComponentPoolCount = 32;

        internal static IntPtr NativePtr => nativeRegistryPtr;

        internal static IntPtr nativeRegistryPtr;
        private static readonly IComponentStorage[] componentStorages = new IComponentStorage[ComponentPoolCount];

        internal static int typeCounter = 0;

        static Registry()
        {
            GameAssemblyManager.OnAssemblyLoad += DeserializeStorages;
        }

        private static void OnNativeEntityDestroy(uint id)
        {
            for (int i = 0; i < ComponentPoolCount; i++)
            {
                var storage = componentStorages[i];
                if (storage != null && storage.Contains(new Entity(id)))
                    storage.Remove(new Entity(id));
            }
        }

        private static void SerializeManagedComponents(IntPtr serializationContext, uint entityId)
        {
            var entity = new Entity(entityId);
            var serializerOptions = new JsonSerializerOptions()
            {
                IncludeFields = true,
                IgnoreReadOnlyProperties = true
            };

            for (int i = 0; i < ComponentPoolCount; i++)
            {
                var storage = componentStorages[i];
                if (storage != null && storage.Contains(entity))
                {
                    object component = storage.GetBoxed(entity);
                    var type = storage.Type;

                    byte[] serialized = JsonSerializer.SerializeToUtf8Bytes(component, type, serializerOptions);
                    string key = type.FullName;

                    IntPtr keyUTF8 = Marshal.StringToCoTaskMemUTF8(key);
                    GCHandle serializedHandle = GCHandle.Alloc(serialized, GCHandleType.Pinned);

                    NativeRegistry.registry_setSerializedEntityInfo(serializationContext, keyUTF8, serializedHandle.AddrOfPinnedObject());

                    serializedHandle.Free();
                    Marshal.FreeCoTaskMem(keyUTF8);
                }
            }
        }

        private static void DeserializeManagedComponent(IntPtr idPtr, IntPtr jsonPtr, uint entityId)
        {
            var entity = new Entity(entityId);
            var serializerOptions = new JsonSerializerOptions()
            {
                IncludeFields = true,
                IgnoreReadOnlyProperties = true
            };

            string idStr = Marshal.PtrToStringAnsi(idPtr);
            string jsonStr = Marshal.PtrToStringAnsi(jsonPtr);

            Type type = HotloadSerialization.CurrentGameAssembly.GetType(idStr);

            IComponentStorage storage = AssureStorage(type);
            var deserialized = JsonSerializer.Deserialize(jsonStr, type, serializerOptions);
            storage.SetBoxed(entity, deserialized);
        }

        internal static void SerializeStorages()
        {
            for (int i = 0; i < ComponentPoolCount; i++)
            {
                if (componentStorages[i] == null) continue;
                componentStorages[i].SerializeForHotload();
                componentStorages[i] = null;
            }
        }

        private static void DeserializeStorages(Assembly gameAssembly)
        {
            List<string> componentTypes = new List<string>();
            foreach (KeyValuePair<string, SerializedComponentStorage> kvp in ComponentTypeLookup.serializedComponents)
            {
                componentTypes.Add(kvp.Value.FullTypeName);
            }
            
            foreach (string typename in componentTypes)
            {
                Type componentType = gameAssembly.GetType(typename);
                AssureStorage(componentType);
            }
        }

        private static IComponentStorage AssureStorage(Type type)
        {
            if (!ComponentTypeLookup.typeIndices.ContainsKey(type.FullName) || componentStorages[ComponentTypeLookup.typeIndices[type.FullName]] == null)
            {
                Type storageType = typeof(ComponentStorage<>).MakeGenericType(type);

                int index = (int)storageType.GetField("typeIndex", BindingFlags.Static | BindingFlags.Public).GetValue(null);

                bool hotload = ComponentTypeLookup.serializedComponents.ContainsKey(type.FullName);

                componentStorages[index] = (IComponentStorage)Activator.CreateInstance(storageType, BindingFlags.NonPublic | BindingFlags.Instance, null, new object[] { hotload }, null);
            }

            return componentStorages[ComponentTypeLookup.typeIndices[type.FullName]];
        }

        private static ComponentStorage<T> AssureStorage<T>()
        {
            int typeIndex = ComponentStorage<T>.typeIndex;

            if (typeIndex >= ComponentPoolCount)
                throw new ArgumentOutOfRangeException("Out of component pools. Oops.");

            bool hotload = ComponentTypeLookup.serializedComponents.ContainsKey(typeof(T).FullName);

            if (componentStorages[typeIndex] == null)
                componentStorages[typeIndex] = new ComponentStorage<T>(hotload);

            return (ComponentStorage<T>)componentStorages[typeIndex];
        }

        public static bool HasBuiltinComponent<T>(Entity entity) where T : BuiltinComponent
        {
            var type = typeof(T);

            if (type == typeof(WorldObject))
            {
                return WorldObject.ExistsOn(nativeRegistryPtr, entity);
            }

            throw new ArgumentException($"Type {type.FullName} is not a builtin component.");
        }

        public static bool HasComponent<T>(Entity entity)
        {
            var storage = AssureStorage<T>();

            return storage.Contains(entity);
        }

        public static bool HasComponent(Entity entity, Type t)
        {
            var storage = AssureStorage(t);

            return storage.Contains(entity);
        }

        public static void AddComponent<T>(Entity entity, T instance)
        {
            var storage = AssureStorage<T>();
            storage.Set(entity, instance);
        }

        public static void AddComponent(Entity entity, Type t, object val)
        {
            var storage = AssureStorage(t);

            storage.SetBoxed(entity, val);
        }

        public static T GetComponent<T>(Entity entity)
        {
            var type = typeof(T);

            if (type.IsAssignableTo(typeof(BuiltinComponent)))
            {
                if (type == typeof(WorldObject))
                {
                    return (T)(object)new WorldObject(nativeRegistryPtr, entity.ID);
                }
                else if (type == typeof(DynamicPhysicsActor))
                {
                    return (T)(object)new DynamicPhysicsActor(nativeRegistryPtr, entity.ID);
                }
            }
            
            var storage = AssureStorage<T>();

            return storage.Get(entity);
        }

        public static object GetComponent(Type type, Entity entity)
        {
            if (type.IsAssignableTo(typeof(BuiltinComponent)))
            {
                if (type == typeof(WorldObject))
                {
                    return new WorldObject(nativeRegistryPtr, entity.ID);
                }
            }

            var storage = AssureStorage(type);

            return storage.GetBoxed(entity);
        }

        public static void RemoveComponent<T>(Entity entity)
        {
            var storage = AssureStorage<T>();

            storage.Remove(entity);
        }

        public static void RemoveComponent(Type type, Entity entity)
        {
            var storage = AssureStorage(type);
            storage.Remove(entity);
        }

        public static Transform GetTransform(Entity entity)
        {
            Transform t = new Transform();
            unsafe
            {
                Transform* tPtr = &t;
                NativeRegistry.registry_getTransform(nativeRegistryPtr, entity.ID, (IntPtr)tPtr);
            }
            return t;
        }

        public static void SetTransform(Entity entity, Transform t)
        {
            unsafe
            {
                Transform* tPtr = &t;
                NativeRegistry.registry_setTransform(nativeRegistryPtr, entity.ID, (IntPtr)tPtr);
            }
        }

        public static bool HasName(Entity entity)
        {
            uint length = NativeRegistry.registry_getEntityNameLength(nativeRegistryPtr, entity.ID);
            return length != uint.MaxValue;
        }

        public static string GetName(Entity entity)
        {
            uint length = NativeRegistry.registry_getEntityNameLength(nativeRegistryPtr, entity.ID);

            if (length == uint.MaxValue)
                return null;

            StringBuilder sb = new StringBuilder((int)length);
            NativeRegistry.registry_getEntityName(nativeRegistryPtr, entity.ID, sb);
            return sb.ToString();
        }

        public static Entity Find(string name)
        {
            Entity result = Entity.Null;

            Each((Entity ent) =>
            {
                if (GetName(ent) == name)
                {
                    result = ent;
                }
            });

            return result;
        }

        private static Action<Entity> currentEntityFunction;

        private static void EntityCallback(uint entityId)
        {
            currentEntityFunction(new Entity(entityId));
        }

        public static void Each(Action<Entity> function)
        {
            currentEntityFunction = function;
            NativeRegistry.registry_eachTransform(nativeRegistryPtr, EntityCallback);
            currentEntityFunction = null;
        }

        public static void Destroy(Entity entity)
        {
            NativeRegistry.registry_destroy(nativeRegistryPtr, entity.ID);
        }

        public static Entity Create()
        {
            return new Entity(NativeRegistry.registry_create(nativeRegistryPtr));
        }

        public static Entity CreatePrefab(AssetID prefabId)
        {
            return new Entity(NativeRegistry.registry_createPrefab(nativeRegistryPtr, prefabId.ID));
        }

        public static ComponentStorage<T> View<T>()
        {
            return AssureStorage<T>();
        }

        public static void ShowDebugWindow()
        {
            if (ImGui.Begin("Registry Debugging"))
            {
                ImGui.Text($"Type counter: {typeCounter}");

                for (int i = 0; i < ComponentPoolCount; i++)
                {
                    if (componentStorages[i] == null) continue;
                    ImGui.Text($"Pool {i}: {componentStorages[i].Type.FullName}");
                }
                ImGui.End();
            }
        }

        internal static void UpdateThinkingComponents()
        {
            for (int i = 0; i < ComponentPoolCount; i++)
            {
                if (componentStorages[i] == null || !componentStorages[i].IsThinking) continue;
                componentStorages[i].UpdateIfThinking();
            }
        }
    }
}
