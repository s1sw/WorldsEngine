using ImGuiNET;
using JetBrains.Annotations;
using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using WorldsEngine.ComponentMeta;
using WorldsEngine.ECS;

namespace WorldsEngine
{
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
        public static extern void registry_setEntityName(IntPtr regPtr, uint entityId, string str);

        [DllImport(WorldsEngine.NativeModule)]
        public static extern void registry_destroy(IntPtr regPtr, uint entityId);

        [DllImport(WorldsEngine.NativeModule)]
        public static extern uint registry_create(IntPtr regPtr);

        [DllImport(WorldsEngine.NativeModule)]
        public static extern void registry_setSerializedEntityInfo(IntPtr serializationContext, IntPtr key, IntPtr value);

        [DllImport(WorldsEngine.NativeModule)]
        public static extern uint registry_createPrefab(IntPtr regPtr, uint assetId);

        [DllImport(WorldsEngine.NativeModule)]
        public static extern bool registry_valid(IntPtr regPtr, uint entityId);
    }

    public static class Registry
    {
        const int ComponentPoolCount = 128;

        internal static IntPtr NativePtr => nativeRegistryPtr;

        internal static IntPtr nativeRegistryPtr;
        private static readonly IComponentStorage?[] componentStorages = new IComponentStorage?[ComponentPoolCount];

        internal static int typeCounter = 0;

        private readonly static Queue<Entity> _destroyQueue = new();
        private readonly static List<IComponentStorage> _collisionHandlers = new();
        private readonly static List<IComponentStorage> _startListeners = new();

        static Registry()
        {
            GameAssemblyManager.OnAssemblyLoad += DeserializeStorages;
        }

        [UsedImplicitly]
        [SuppressMessage("CodeQuality", "IDE0051:Remove unused private members",
            Justification = "Called from native C++")]
        private static void OnNativeEntityDestroy(uint id)
        {
            for (int i = 0; i < ComponentPoolCount; i++)
            {
                var storage = componentStorages[i];
                if (storage != null && storage.Contains(new Entity(id)))
                    storage.Remove(new Entity(id));
            }
        }

        [UsedImplicitly]
        [SuppressMessage("CodeQuality", "IDE0051:Remove unused private members",
            Justification = "Called from native C++ during deserialization")]
        private static void SerializeManagedComponents(IntPtr serializationContext, uint entityId)
        {
            var entity = new Entity(entityId);
            var serializerOptions = new JsonSerializerOptions()
            {
                IncludeFields = true,
                IgnoreReadOnlyProperties = true,
            };

            for (int i = 0; i < ComponentPoolCount; i++)
            {
                var storage = componentStorages[i];
                if (storage != null && storage.Contains(entity))
                {
                    var type = storage.Type;

                    // Components defined in this assembly will be native, so we
                    // don't want to serialize them twice
                    if (type.Assembly == Assembly.GetExecutingAssembly()) continue;
                    object component = storage.GetBoxed(entity);

                    byte[] serialized = JsonSerializer.SerializeToUtf8Bytes(component, type, serializerOptions);
                    string key = type.FullName!;

                    IntPtr keyUTF8 = Marshal.StringToCoTaskMemUTF8(key);
                    GCHandle serializedHandle = GCHandle.Alloc(serialized, GCHandleType.Pinned);

                    NativeRegistry.registry_setSerializedEntityInfo(serializationContext, keyUTF8, serializedHandle.AddrOfPinnedObject());

                    serializedHandle.Free();
                    Marshal.FreeCoTaskMem(keyUTF8);
                }
            }
        }

        [UsedImplicitly]
        [SuppressMessage("CodeQuality", "IDE0051:Remove unused private members",
            Justification = "Called from native C++ during deserialization")]
        private static void DeserializeManagedComponent(IntPtr idPtr, IntPtr jsonPtr, uint entityId)
        {
            var entity = new Entity(entityId);
            var serializerOptions = new JsonSerializerOptions()
            {
                IncludeFields = true,
                IgnoreReadOnlyProperties = true
            };

            string idStr = Marshal.PtrToStringAnsi(idPtr)!;
            string jsonStr = Marshal.PtrToStringAnsi(jsonPtr)!;

            if (HotloadSerialization.CurrentGameAssembly == null)
            {
                Log.Error("Game assembly isn't loaded!");
                return;
            }

            Type? type = HotloadSerialization.CurrentGameAssembly.GetType(idStr);

            if (type == null)
            {
                Log.Warn($"Failed to find type with ID {idStr}. Trying with fuzzier search in case the namespace changed.");
                string[] split = idStr.Split(".");
                string typename = split[split.Length - 1];

                foreach (Type candidate in HotloadSerialization.CurrentGameAssembly.GetTypes())
                {
                    if (candidate.Name == typename)
                        type = candidate;
                }

                if (type == null)
                {
                    Log.Error("Still couldn't find the type.");   
                    return;
                }
            }

            IComponentStorage storage = AssureStorage(type);

            // Deserialization should never fail - this isn't untrusted JSON, it's serialized in C++
            var deserialized = JsonSerializer.Deserialize(jsonStr, type, serializerOptions)!;
            SetComponent(entity, type, deserialized);
        }

        [UsedImplicitly]
        [SuppressMessage("CodeQuality", "IDE0051:Remove unused private members",
            Justification = "Called from native C++")]
        private static void CopyManagedComponents(Entity from, Entity to)
        {
            foreach (var metadata in MetadataManager.ManagedMetadata)
            {
                if (metadata.ExistsOn(from))
                    metadata.Copy(from, to);
            }
        }

        internal static void SerializeStorages()
        {
            for (int i = 0; i < ComponentPoolCount; i++)
            {
                if (componentStorages[i] == null) continue;
                if (componentStorages[i]!.Type.Assembly == Assembly.GetExecutingAssembly()) continue;
                componentStorages[i]!.SerializeForHotload();
                componentStorages[i] = null;
            }

            _collisionHandlers.Clear();
            _startListeners.Clear();
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
                Type? componentType = gameAssembly.GetType(typename);

                if (componentType == null)
                    Log.Warn($"Component type {typename} no longer exists");
                else
                    AssureStorage(componentType);
            }
        }

        private static IComponentStorage AssureStorage(Type type)
        {
            if (!ComponentTypeLookup.typeIndices.ContainsKey(type.FullName!) || componentStorages[ComponentTypeLookup.typeIndices[type.FullName!]] == null)
            {
                Type storageType = typeof(ComponentStorage<>).MakeGenericType(type);

                int index = (int)storageType.GetField("typeIndex", BindingFlags.Static | BindingFlags.Public)!.GetValue(null)!;
                Log.Msg($"Creating storage for {type.FullName}, index {index}");

                if (index >= ComponentPoolCount)
                    throw new ArgumentOutOfRangeException("Out of component pools. Oops.");

                bool hotload = ComponentTypeLookup.serializedComponents.ContainsKey(type.FullName!);

                componentStorages[index] = (IComponentStorage)Activator.CreateInstance(storageType, BindingFlags.Public | BindingFlags.Instance, null, new object[] { hotload }, null)!;

                if (typeof(ICollisionHandler).IsAssignableFrom(type))
                    _collisionHandlers.Add(componentStorages[index]!);

                if (typeof(IStartListener).IsAssignableFrom(type))
                    _startListeners.Add(componentStorages[index]!);
            }

            return componentStorages[ComponentTypeLookup.typeIndices[type.FullName!]]!;
        }

        private static ComponentStorage<T> AssureStorage<T>()
        {
            if (typeof(T).IsArray) throw new ArgumentException("");

            int typeIndex = ComponentStorage<T>.typeIndex;

            if (typeIndex >= ComponentPoolCount)
                throw new ArgumentOutOfRangeException("Out of component pools. Oops.");

            bool hotload = ComponentTypeLookup.serializedComponents.ContainsKey(typeof(T).FullName!);

            if (componentStorages[typeIndex] == null)
            {
                Log.Msg($"Creating storage for {typeof(T).FullName}, index {typeIndex}");
                componentStorages[typeIndex] = new ComponentStorage<T>(hotload);
                if (typeof(ICollisionHandler).IsAssignableFrom(typeof(T)))
                    _collisionHandlers.Add(componentStorages[typeIndex]!);

                if (typeof(IStartListener).IsAssignableFrom(typeof(T)))
                    _startListeners.Add(componentStorages[typeIndex]!);
            }

            return (ComponentStorage<T>)componentStorages[typeIndex]!;
        }

        private static ComponentMetadata GetBuiltinComponentMetadata(Type componentType)
        {
            PropertyInfo propertyInfo = componentType.GetProperty(
                "Metadata",
                BindingFlags.Static | BindingFlags.NonPublic
            )!;

            return (ComponentMetadata)propertyInfo.GetValue(null)!;
        }

        [MustUseReturnValue]
        public static bool HasComponent<T>(Entity entity)
        {
            var type = typeof(T);
            if (type.IsAssignableTo(typeof(BuiltinComponent)))
            {
                return GetBuiltinComponentMetadata(type).ExistsOn(entity);
            }

            var storage = AssureStorage<T>();

            return storage.Contains(entity);
        }

        [MustUseReturnValue]
        public static bool HasComponent(Entity entity, Type type)
        {
            if (type.IsAssignableTo(typeof(BuiltinComponent)))
            {
                return GetBuiltinComponentMetadata(type).ExistsOn(entity);
            }

            var storage = AssureStorage(type);

            return storage.Contains(entity);
        }

        public static T AddComponent<T>(Entity entity)
        {
            var type = typeof(T);
            if (HasComponent<T>(entity)) throw new InvalidOperationException("Can't add a component that already exists");

            if (type.IsAssignableTo(typeof(BuiltinComponent)))
            {
                GetBuiltinComponentMetadata(type).Create(entity);
                return (T)GetComponent(type, entity);
            }

            var storage = AssureStorage<T>();

            T instance = Activator.CreateInstance<T>();

            storage.Set(entity, Activator.CreateInstance<T>());

            if (type.IsSubclassOf(typeof(Component)))
                ((Component)(object)instance!).Entity = entity;

            if (type.IsAssignableTo(typeof(IStartListener)))
                ((IStartListener)instance!).Start();

            return instance;
        }

        public static object AddComponent(Entity entity, Type type)
        {
            if (HasComponent(entity, type)) throw new InvalidOperationException("Can't add a component that already exists");

            if (type.IsAssignableTo(typeof(BuiltinComponent)))
            {
                GetBuiltinComponentMetadata(type).Create(entity);
                return GetComponent(type, entity);
            }

            var storage = AssureStorage(type);

            object instance = Activator.CreateInstance(type)!;

            if (type.IsSubclassOf(typeof(Component)))
                ((Component)instance!).Entity = entity;

            if (type.IsAssignableTo(typeof(IStartListener)))
                ((IStartListener)instance!).Start();

            storage.SetBoxed(entity, instance);
            return instance;
        }

        internal static void SetComponent(Entity entity, Type type, object value)
        {
            if (type.IsAssignableTo(typeof(BuiltinComponent)))
            {
                throw new InvalidOperationException();
            }

            var storage = AssureStorage(type);

            if (type.IsSubclassOf(typeof(Component)))
                ((Component)value).Entity = entity;

            storage.SetBoxed(entity, value);
        }

        public static bool TryGetComponent<T>(Entity entity, out T component)
        {
            if (!HasComponent<T>(entity))
            {
                component = default(T)!;
                return false;
            }

            component = GetComponent<T>(entity);
            return true;
        }

        public static T GetComponent<T>(Entity entity)
        {
            var type = typeof(T);
            if (entity.IsNull) throw new NullEntityException();

            var storage = AssureStorage<T>();

            if (type.IsAssignableTo(typeof(BuiltinComponent)))
            {
                if (GetBuiltinComponentMetadata(type).ExistsOn(entity))
                {
                    if (!storage.Contains(entity))
                    {
                        ConstructorInfo ci = type.GetConstructor(
                            BindingFlags.NonPublic | BindingFlags.Instance,
                            null,
                            new Type[] { typeof(IntPtr), typeof(uint) }, null)!;

                        T comp = (T)ci.Invoke(new object[] { nativeRegistryPtr, entity.ID });

                        storage.Set(entity, comp);

                        return comp;
                    }
                    else
                    {
                        return storage.Get(entity);
                    }
                }

                throw new MissingComponentException(type.Name);
            }

            if (!storage.Contains(entity))
                throw new MissingComponentException(type.Name);

            return storage.Get(entity);
        }

        public static object GetComponent(Type type, Entity entity)
        {
            if (entity.IsNull) throw new NullEntityException();

            var storage = AssureStorage(type);

            if (type.IsAssignableTo(typeof(BuiltinComponent)))
            {
                if (!storage.Contains(entity) && GetBuiltinComponentMetadata(type).ExistsOn(entity))
                {
                    if (!storage.Contains(entity))
                    {
                        ConstructorInfo ci = type.GetConstructor(
                            BindingFlags.NonPublic | BindingFlags.Instance,
                            null,
                            new Type[] { typeof(IntPtr), typeof(uint) }, null)!;

                        object comp = ci.Invoke(new object[] { nativeRegistryPtr, entity.ID });

                        storage.SetBoxed(entity, comp);

                        return comp;
                    }
                    else
                    {
                        return storage.GetBoxed(entity);
                    }
                }

                throw new MissingComponentException(type.Name);
            }

            if (!storage.Contains(entity))
                throw new MissingComponentException(type.Name);

            return storage.GetBoxed(entity);
            
        }

        public static void RemoveComponent<T>(Entity entity)
        {
            var type = typeof(T);
            bool builtin = type.IsAssignableTo(typeof(BuiltinComponent));

            if (builtin)
            {
                var meta = GetBuiltinComponentMetadata(type);
                if (!meta.ExistsOn(entity))
                    throw new ArgumentException($"Trying to remove component {type.Name} that isn't on the given entity");
                meta.Destroy(entity);
            }

            var storage = AssureStorage<T>();

            if (!builtin || storage.Contains(entity))
                storage.Remove(entity);
        }

        public static void RemoveComponent(Type type, Entity entity)
        {
            if (type.IsAssignableTo(typeof(BuiltinComponent)))
            {
                GetBuiltinComponentMetadata(type).Destroy(entity);
            }

            var storage = AssureStorage(type);
            storage.Remove(entity);
        }

        internal static bool OverrideTransformToDPAPose = false;

        [MustUseReturnValue]
        public static Transform GetTransform(Entity entity)
        {
            if (!Valid(entity)) throw new ArgumentException("Invalid entity handle");
            Transform t = new Transform();
            unsafe
            {
                Transform* tPtr = &t;
                NativeRegistry.registry_getTransform(nativeRegistryPtr, entity.ID, (IntPtr)tPtr);
            }

            if (OverrideTransformToDPAPose)
            {
                if (HasComponent<DynamicPhysicsActor>(entity))
                {
                    Transform pose = GetComponent<DynamicPhysicsActor>(entity).Pose;
                    t.Position = pose.Position;
                    t.Rotation = pose.Rotation;
                }
            }
            return t;
        }

        public static void SetTransform(Entity entity, Transform t)
        {
            if (OverrideTransformToDPAPose && TryGetComponent<DynamicPhysicsActor>(entity, out var dpa))
            {
                dpa.Pose = t;
            }

            unsafe
            {
                Transform* tPtr = &t;
                NativeRegistry.registry_setTransform(nativeRegistryPtr, entity.ID, (IntPtr)tPtr);
            }
        }

        [MustUseReturnValue]
        public static bool HasName(Entity entity)
        {
            uint length = NativeRegistry.registry_getEntityNameLength(nativeRegistryPtr, entity.ID);
            return length != uint.MaxValue;
        }

        [MustUseReturnValue]
        public static string? GetName(Entity entity)
        {
            uint length = NativeRegistry.registry_getEntityNameLength(nativeRegistryPtr, entity.ID);

            if (length == uint.MaxValue)
                return null;

            StringBuilder sb = new StringBuilder((int)length);
            NativeRegistry.registry_getEntityName(nativeRegistryPtr, entity.ID, sb);
            return sb.ToString();
        }

        public static void SetName(Entity entity, string name)
        {
            NativeRegistry.registry_setEntityName(nativeRegistryPtr, entity.ID, name);
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

        public static void Each(Action<Entity> function)
        {
            void EntityCallback(uint entityId)
            {
                function(new Entity(entityId));
            }

            NativeRegistry.registry_eachTransform(nativeRegistryPtr, EntityCallback);
        }

        public static void Destroy(Entity entity)
        {
            if (!Valid(entity))
                throw new InvalidEntityException("Tried to destroy an invalid entity");
            NativeRegistry.registry_destroy(nativeRegistryPtr, entity.ID);
            foreach (IComponentStorage? storage in componentStorages)
            {
                if (storage == null) continue;

                if (storage.Contains(entity))
                    storage.Remove(entity);
            }
        }

        public static void DestroyNext(Entity entity)
        {
            _destroyQueue.Enqueue(entity);
        }

        public static Entity Create()
        {
            return new Entity(NativeRegistry.registry_create(nativeRegistryPtr));
        }

        public static Entity CreatePrefab(AssetID prefabId)
        {
            Entity e = new(NativeRegistry.registry_createPrefab(nativeRegistryPtr, prefabId.ID));

            foreach (IComponentStorage storage in _startListeners)
            {
                if (!storage.Contains(e)) continue;
                ((IStartListener)storage.GetBoxed(e)).Start();
            }

            return e;
        }

        public static ComponentStorage<T> View<T>()
        {
            return AssureStorage<T>();
        }

        public static IComponentStorage View(Type t)
        {
            return AssureStorage(t);
        }

        public static bool Valid(Entity entity)
        {
            return NativeRegistry.registry_valid(NativePtr, entity.ID);
        }

        public static void ShowDebugWindow()
        {
            if (ImGui.Begin("Registry Debugging"))
            {
                ImGui.Text($"Type counter: {typeCounter}");

                for (int i = 0; i < ComponentPoolCount; i++)
                {
                    if (componentStorages[i] == null) continue;
                    ImGui.Text($"Pool {i}: {componentStorages[i]!.Type.FullName}");
                }
                ImGui.End();
            }
        }

        internal static void RunSimulateOnComponents()
        {
            for (int i = 0; i < ComponentPoolCount; i++)
            {
                if (componentStorages[i] == null || !componentStorages[i]!.IsThinking) continue;
                componentStorages[i]!.RunSimulate();
            }
        }

        internal static void RunUpdateOnComponents()
        {
            for (int i = 0; i < ComponentPoolCount; i++)
            {
                if (componentStorages[i] == null || !componentStorages[i]!.IsUpdateable) continue;
                componentStorages[i]!.RunUpdate();
            }
        }

        internal static void OnSceneStart()
        {
            for (int i = 0; i < ComponentPoolCount; i++)
            {
                if (componentStorages[i] == null) continue;

                var componentStorage = componentStorages[i]!;
                if (!typeof(IStartListener).IsAssignableFrom(componentStorage.Type)) continue;

                foreach (Entity e in componentStorages[i]!)
                {
                    object comp = componentStorage.GetBoxed(e);

                    try
                    {
                        ((IStartListener)comp).Start();
                    }
                    catch (Exception exception)
                    {
                        Logger.LogError($"Caught exception: {exception}");
                    }
                }
            }
        }

        internal static void ClearDestroyQueue()
        {
            while (_destroyQueue.TryDequeue(out Entity ent))
            {
                if (Valid(ent))
                    Destroy(ent);
            }
        }

        internal static void HandleCollision(uint entityId, ref PhysicsContactInfo contactInfo)
        {
            Entity entity = new(entityId);

            foreach (IComponentStorage storage in _collisionHandlers)
            {
                if (!storage.Contains(entity)) continue;
                var handler = (ICollisionHandler)storage.GetBoxed(entity);
                handler.OnCollision(ref contactInfo);
            }
        }

        internal static List<Type> GetTypesWithAttribute(Type attributeType)
        {
            List<Type> t = new();
            foreach (IComponentStorage? storage in componentStorages)
            {
                if (storage == null) continue;
                if (storage.Type.GetCustomAttribute(attributeType) != null)
                {
                    t.Add(storage.Type);
                }
            }

            return t;
        }
    }
}
