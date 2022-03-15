using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Collections;
using System.Reflection;
using WorldsEngine.Math;
using WorldsEngine.Editor;
using ImGuiNET;

namespace WorldsEngine.ComponentMeta
{
    internal class NativeMetadataAPI
    {
        [DllImport(WorldsEngine.NativeModule)]
        internal static extern int componentmeta_getDataCount();

        [DllImport(WorldsEngine.NativeModule)]
        internal static extern IntPtr componentmeta_getName(int index);

        [DllImport(WorldsEngine.NativeModule)]
        internal static extern void componentmeta_editIfNecessary(IntPtr registry, uint entity, int index);

        [DllImport(WorldsEngine.NativeModule)]
        internal static extern void componentmeta_create(IntPtr registry, uint entity, int index);

        [DllImport(WorldsEngine.NativeModule)]
        internal static extern void componentmeta_destroy(IntPtr registry, uint entity, int index);

        [DllImport(WorldsEngine.NativeModule)]
        internal static extern void componentmeta_clone(IntPtr registry, uint from, uint to, int index);

        [DllImport(WorldsEngine.NativeModule)]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool componentmeta_hasComponent(IntPtr registry, uint entity, int index);

        [DllImport(WorldsEngine.NativeModule)]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool componentmeta_allowInspectorAdd(int index);
    }

    public abstract class ComponentMetadata
    {
        public abstract string Name { get; }
        public abstract string EditorName { get; }
        public abstract bool AllowInspectorAdd { get; }

        public abstract void Create(Entity entity);
        public abstract void Destroy(Entity entity);
        public abstract void Copy(Entity from, Entity to);
        public abstract void EditIfExists(Entity entity);
        public abstract bool ExistsOn(Entity entity);
    }

    internal class NativeComponentMetadata : ComponentMetadata
    {
        internal int Index;

        public override string Name => Marshal.PtrToStringUTF8(NativeMetadataAPI.componentmeta_getName(Index))!;
        public override string EditorName => Name;
        public override bool AllowInspectorAdd => NativeMetadataAPI.componentmeta_allowInspectorAdd(Index);

        public override void Create(Entity entity)
        {
            NativeMetadataAPI.componentmeta_create(Registry.NativePtr, entity.ID, Index);
        }

        public override void Destroy(Entity entity)
        {
            NativeMetadataAPI.componentmeta_destroy(Registry.NativePtr, entity.ID, Index);
        }

        public override void Copy(Entity from, Entity to)
        {
            NativeMetadataAPI.componentmeta_clone(Registry.NativePtr, from.ID, to.ID, Index);
        }

        public override void EditIfExists(Entity entity)
        {
            NativeMetadataAPI.componentmeta_editIfNecessary(Registry.NativePtr, entity.ID, Index);
        }

        public override bool ExistsOn(Entity entity)
        {
            return NativeMetadataAPI.componentmeta_hasComponent(Registry.NativePtr, entity.ID, Index);
        }
    }

    internal class ManagedComponentMetadata : ComponentMetadata
    {
        private readonly Type type;
        private readonly FieldInfo[] fieldInfos;
        private readonly string friendlyName = string.Empty;

        public override string Name => type.Name;
        public override string EditorName => friendlyName;
        public override bool AllowInspectorAdd => true;

        public ManagedComponentMetadata(Type type)
        {
            this.type = type;
            friendlyName = Name;

            var nameAttribute = type.GetCustomAttribute<EditorFriendlyNameAttribute>();

            if (nameAttribute != null)
                friendlyName = nameAttribute.FriendlyName;

            var iconAttribute = type.GetCustomAttribute<EditorIconAttribute>();

            if (iconAttribute != null)
                friendlyName = iconAttribute.Icon + " " + friendlyName;

            fieldInfos = type.GetFields(BindingFlags.Public | BindingFlags.Instance)
                .Where(m => !m.IsNotSerialized).ToArray();
        }

        public override void Create(Entity entity)
        {
            Registry.AddComponent(entity, type);
        }

        public override void Destroy(Entity entity)
        {
            Registry.RemoveComponent(type, entity);
        }

        public override void Copy(Entity from, Entity to)
        {
            Registry.SetComponent(to, type, Registry.GetComponent(type, from));
        }
        
        private static Dictionary<Type, IComponentEditor> _componentEditorCache = new();

        public override void EditIfExists(Entity entity)
        {
            if (!Registry.HasComponent(entity, type)) return;

            ImGui.PushStyleVar(ImGuiStyleVar.FrameBorderSize, 1.0f);
            if (ImGui.CollapsingHeader(EditorName))
            {
                ImGui.PopStyleVar();
                if (ImGui.Button("Remove"))
                {
                    Registry.RemoveComponent(type, entity);
                    return;
                }
                
                CustomEditorAttribute? customEditorAttribute = type.GetCustomAttribute<CustomEditorAttribute>();
                
                if (customEditorAttribute != null)
                {
                    if (!_componentEditorCache.ContainsKey(customEditorAttribute.EditorType))
                    {
                        IComponentEditor ed = (IComponentEditor)Activator.CreateInstance(customEditorAttribute.EditorType)!;
                        _componentEditorCache.Add(customEditorAttribute.EditorType, ed);
                    }
                    _componentEditorCache[customEditorAttribute.EditorType].EditEntity(entity);
                }
                else
                {
                    object component = Registry.GetComponent(type, entity);

                    foreach (FieldInfo fieldInfo in fieldInfos!)
                    {
                        EditorUtils.EditField(fieldInfo, component, entity);
                    }
                }
            }
            else
            {
                ImGui.PopStyleVar();
            }
        }

        public override bool ExistsOn(Entity entity)
        {
            return Registry.HasComponent(entity, type);
        }
    }

    public static class MetadataManager
    {
        public static IReadOnlyCollection<ComponentMetadata> Metadata => metadata;
        public static IReadOnlyCollection<ComponentMetadata> ManagedMetadata => managedMetadata;

        private static readonly List<ComponentMetadata> metadata = new();
        private static readonly List<NativeComponentMetadata> nativeMetadata = new();
        private static readonly List<ManagedComponentMetadata> managedMetadata = new();

        internal static ComponentMetadata? FindNativeMetadata(string name)
        {
            return metadata.Where(metadata => metadata.Name == name).FirstOrDefault();
        }

        internal static void Initialise()
        {
            for (int i = 0; i < NativeMetadataAPI.componentmeta_getDataCount(); i++)
            {
                NativeComponentMetadata componentMetadata = new NativeComponentMetadata()
                {
                    Index = i
                };

                nativeMetadata.Add(componentMetadata);
            }

            metadata.AddRange(nativeMetadata);

            GameAssemblyManager.OnAssemblyLoad += RegisterComponentsForAssembly;
            GameAssemblyManager.OnAssemblyUnload += ClearManagedMetadata;
        }

        private static void ClearManagedMetadata()
        {
            managedMetadata.Clear();
            metadata.Clear();
        }

        private static void RegisterComponentsForAssembly(Assembly assembly)
        {
            metadata.Clear();
            managedMetadata.Clear();

            foreach (Type type in assembly.GetTypes())
            {
                if (type.GetCustomAttributes(typeof(ComponentAttribute), true).Length > 0)
                {
                    managedMetadata.Add(new ManagedComponentMetadata(type));
                }
            }

            Logger.Log($"Registered {managedMetadata.Count} components in {assembly.FullName}");

            metadata.AddRange(nativeMetadata);
            metadata.AddRange(managedMetadata);
        }

        public static void EditEntity(Entity entity)
        {
            foreach (ComponentMetadata metadata in metadata)
            {
                metadata.EditIfExists(entity);
            }
        }
    }
}
