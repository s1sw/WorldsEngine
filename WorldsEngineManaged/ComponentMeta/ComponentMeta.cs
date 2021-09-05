using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Collections;
using System.Reflection;
using WorldsEngine.Math;
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
        private string friendlyName = string.Empty;

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

        private void EditField(FieldInfo fieldInfo, object instance)
        {
            string fieldName = fieldInfo.Name;

            var nameAttribute = fieldInfo.GetCustomAttribute<EditorFriendlyNameAttribute>();

            if (nameAttribute != null)
                fieldName = nameAttribute.FriendlyName!;

            if (fieldInfo.FieldType == typeof(int))
            {
                int val = (int)fieldInfo.GetValue(instance)!;
                ImGui.DragInt(fieldName, ref val);
                fieldInfo.SetValue(instance, val);
            }
            else if (fieldInfo.FieldType == typeof(float))
            {
                float val = (float)fieldInfo.GetValue(instance)!;
                ImGui.DragFloat(fieldName, ref val);
                fieldInfo.SetValue(instance, val);
            }
            else if (fieldInfo.FieldType == typeof(Vector3))
            {
                Vector3 val = (Vector3)fieldInfo.GetValue(instance)!;
                ImGui.DragFloat3(fieldName, ref val);
                fieldInfo.SetValue(instance, val);
            }
            else if (fieldInfo.FieldType == typeof(bool))
            {
                bool val = (bool)fieldInfo.GetValue(instance)!;
                ImGui.Checkbox(fieldName, ref val);
                fieldInfo.SetValue(instance, val);
            }
            else if (fieldInfo.FieldType.IsEnum)
            {
                string[] names = fieldInfo.FieldType.GetEnumNames();
                int val = (int)fieldInfo.GetValue(instance)!;

                if (ImGui.Combo(fieldName, ref val, names, names.Length))
                {
                    fieldInfo.SetValue(instance, val);
                }
            }

            if (fieldInfo.GetCustomAttribute<EditableClassAttribute>() != null)
            {
                Type type = fieldInfo.FieldType;

                if (type.IsConstructedGenericType && type.GetGenericTypeDefinition() == typeof(List<>))
                {
                    IList? l = (IList?)fieldInfo.GetValue(instance);

                    if (l == null) return;

                    Type objType = type.GetGenericArguments()[0];
                    ImGui.Text($"List of {objType.Name}");
                    ImGui.Text($"{l.Count} elements");

                    if (ImGui.Button("+"))
                    {
                        l.Add(Activator.CreateInstance(objType));
                    }

                    for (int i = 0; i < l.Count; i++)
                    {
                        object? classInstance = l[i];

                        if (ImGui.TreeNode($"Element {i}"))
                        {
                            if (classInstance == null)
                            {
                                ImGui.Text("Null");
                                continue;
                            }
                            else
                            {
                                FieldInfo[] classFieldInfo = objType.GetFields(BindingFlags.Public | BindingFlags.Instance)
                                    .Where(m => !m.IsNotSerialized).ToArray();

                                foreach (FieldInfo fieldInfo1 in classFieldInfo)
                                {
                                    EditField(fieldInfo1, classInstance);
                                }
                            }

                            ImGui.TreePop();
                        }
                    }
                }
                else
                {
                    object? classInstance = fieldInfo.GetValue(instance);

                    if (ImGui.TreeNode(fieldName))
                    {
                        if (classInstance == null)
                        {
                            ImGui.Text("Null");
                        }
                        else
                        {
                            FieldInfo[] classFieldInfo = type.GetFields(BindingFlags.Public | BindingFlags.Instance)
                            .Where(m => !m.IsNotSerialized).ToArray();

                            foreach (FieldInfo fieldInfo1 in classFieldInfo)
                            {
                                EditField(fieldInfo1, classInstance);
                            }
                        }

                        ImGui.TreePop();
                    }
                }
            }
        }

        public override void EditIfExists(Entity entity)
        {
            if (!Registry.HasComponent(entity, type)) return;

            if (ImGui.CollapsingHeader(EditorName))
            {
                if (ImGui.Button("Remove"))
                {
                    Registry.RemoveComponent(type, entity);
                    return;
                }

                object component = Registry.GetComponent(type, entity);

                foreach (FieldInfo fieldInfo in fieldInfos!)
                {
                    EditField(fieldInfo, component);
                }
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
