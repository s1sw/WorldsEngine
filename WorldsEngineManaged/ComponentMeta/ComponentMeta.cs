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
        private class TransformEditContext
        {
            public bool CurrentlyUsing = false;
            public object? Component;
            public string? FieldName;
            public Entity? Entity;

            public void StartUsing(object component, string fieldName)
            {
                CurrentlyUsing = true;
                Component = component;
                FieldName = fieldName;

                Entity = Registry.Create();
            }

            public void StopUsing()
            {
                if (Entity == null) return;
                Registry.Destroy(Entity.Value);
                Entity = null;
                Component = null;
                FieldName = null;
                CurrentlyUsing = false;
            }
        }

        private readonly Type type;
        private readonly FieldInfo[] fieldInfos;
        private readonly string friendlyName = string.Empty;
        private static readonly TransformEditContext transformCtx = new();

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

        private unsafe void EditField(FieldInfo fieldInfo, object instance, Entity entity)
        {
            string fieldName = fieldInfo.Name;

            var nameAttribute = fieldInfo.GetCustomAttribute<EditorFriendlyNameAttribute>();

            if (nameAttribute != null)
                fieldName = nameAttribute.FriendlyName!;

            // Since the type object is dynamic and typeof technically isn't constant,
            // we're forced to use if/else if here ;-;
            if (fieldInfo.FieldType == typeof(int))
            {
                int val = (int)fieldInfo.GetValue(instance)!;
                ImGui.DragInt(fieldName, ref val);
                fieldInfo.SetValue(instance, val);
            }
            else if (fieldInfo.FieldType == typeof(uint))
            {
                uint val = (uint)fieldInfo.GetValue(instance)!;
                ImGui.DragScalar(fieldName, ImGuiDataType.U32, (IntPtr)(&val), 1.0f);
                fieldInfo.SetValue(instance, val);
            }
            else if (fieldInfo.FieldType == typeof(float))
            {
                float val = (float)fieldInfo.GetValue(instance)!;
                ImGui.DragFloat(fieldName, ref val);
                fieldInfo.SetValue(instance, val);
            }
            else if (fieldInfo.FieldType == typeof(double))
            {
                double val = (double)fieldInfo.GetValue(instance)!;
                ImGui.DragScalar(fieldName, ImGuiDataType.Double, (IntPtr)(&val), 1.0f);
                fieldInfo.SetValue(instance, val);
            }
            else if (fieldInfo.FieldType == typeof(Vector3))
            {
                Vector3 val = (Vector3)fieldInfo.GetValue(instance)!;
                ImGui.DragFloat3(fieldName, ref val);
                fieldInfo.SetValue(instance, val);
            }
            else if (fieldInfo.FieldType == typeof(Quaternion))
            {
                Quaternion val = (Quaternion)fieldInfo.GetValue(instance)!;
                Vector4 v4 = (Vector4)val;

                if (ImGui.DragFloat4(fieldName, ref v4))
                    fieldInfo.SetValue(instance, (Quaternion)v4);
            }
            else if (fieldInfo.FieldType == typeof(bool))
            {
                bool val = (bool)fieldInfo.GetValue(instance)!;
                ImGui.Checkbox(fieldName, ref val);
                fieldInfo.SetValue(instance, val);
            }
            else if (fieldInfo.FieldType == typeof(string))
            {
                string str = (string)fieldInfo.GetValue(instance)!;

                if (str == null)
                {
                    str = string.Empty;
                    fieldInfo.SetValue(instance, str);
                }

                // TODO: find a better way of handling this
                if (ImGui.InputText(fieldName, ref str, (uint)(str.Length + 50)))
                {
                    fieldInfo.SetValue(instance, str);
                }
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
            else if (fieldInfo.FieldType == typeof(Transform))
            {
                if (fieldInfo.GetCustomAttribute<EditRelativeTransformAttribute>() != null)
                {
                    if (!transformCtx.CurrentlyUsing)
                    {
                        ImGui.Text(fieldName);
                        ImGui.SameLine();
                        if (ImGui.Button("Change"))
                        {
                            transformCtx.StartUsing(instance, fieldName);
                            Transform currentValue = (Transform)fieldInfo.GetValue(instance)!;
                            if (!currentValue.Rotation.Valid) currentValue.Rotation = Quaternion.Identity;
                            currentValue = currentValue.TransformBy(Registry.GetTransform(entity));
                            Registry.SetTransform(transformCtx.Entity!.Value, currentValue);
                        }
                    }
                    else
                    {
                        Editor.Editor.OverrideHandle(transformCtx.Entity!.Value);
                        var transform = Registry.GetTransform(transformCtx.Entity!.Value);
                        transform = transform.TransformByInverse(Registry.GetTransform(entity));
                        fieldInfo.SetValue(instance, transform);

                        ImGui.Text(fieldName);
                        ImGui.SameLine();

                        if (ImGui.Button("Done"))
                        {
                            transformCtx.StopUsing();
                        }
                    }
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

                    int removeIndex = -1;
                    for (int i = 0; i < l.Count; i++)
                    {
                        object? classInstance = l[i];

                        if (ImGui.TreeNode($"Element {i}"))
                        {
                            if (ImGui.Button("-"))
                            {
                                removeIndex = i;
                            }

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
                                    EditField(fieldInfo1, classInstance, entity);
                                }
                            }

                            ImGui.TreePop();
                        }
                    }

                    if (removeIndex >= 0)
                        l.RemoveAt(removeIndex);
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
                                EditField(fieldInfo1, classInstance, entity);
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

            ImGui.PushStyleVar(ImGuiStyleVar.FrameBorderSize, 1.0f);
            if (ImGui.CollapsingHeader(EditorName))
            {
                ImGui.PopStyleVar();
                if (ImGui.Button("Remove"))
                {
                    Registry.RemoveComponent(type, entity);
                    return;
                }

                object component = Registry.GetComponent(type, entity);

                foreach (FieldInfo fieldInfo in fieldInfos!)
                {
                    EditField(fieldInfo, component, entity);
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
