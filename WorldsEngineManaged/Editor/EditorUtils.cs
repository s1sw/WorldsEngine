using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using ImGuiNET;
using WorldsEngine.Math;

namespace WorldsEngine.Editor;

public static class EditorUtils
{
    private static readonly TransformEditContext transformCtx = new();
    private static string? transformCtxFieldName;
    
    public static void EnumDropdown<T>(string name, ref T eVal) where T : Enum
    {
        string[] names = typeof(T).GetEnumNames();
        int val = Convert.ToInt32(eVal);

        if (ImGui.Combo(name, ref val, names, names.Length))
        {
            eVal = (T)Enum.ToObject(typeof(T), val);
        }
    }

    public static unsafe void EditField(FieldInfo fieldInfo, object instance, Entity entity)
    {
        string fieldName = fieldInfo.Name;

        var nameAttribute = fieldInfo.GetCustomAttribute<EditorFriendlyNameAttribute>();

        if (nameAttribute != null)
            fieldName = nameAttribute.FriendlyName!;
        
        ImGui.PushID(fieldName);

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
                    if (ImGui.Button($"Change##{fieldInfo.Name}"))
                    {
                        Transform currentValue = (Transform)fieldInfo.GetValue(instance)!;
                        if (!currentValue.Rotation.Valid) currentValue.Rotation = Quaternion.Identity;
                        currentValue = currentValue.TransformBy(Registry.GetTransform(entity));
                        transformCtx.StartUsing(currentValue);
                        transformCtxFieldName = fieldName;
                    }
                }
                else if (transformCtxFieldName == fieldName)
                {
                    Editor.OverrideHandle(transformCtx.Entity!.Value);
                    var transform = Registry.GetTransform(transformCtx.Entity!.Value);
                    transform = transform.TransformByInverse(Registry.GetTransform(entity));
                    fieldInfo.SetValue(instance, transform);

                    ImGui.Text(fieldName);
                    ImGui.SameLine();

                    if (ImGui.Button("Done"))
                    {
                        transformCtx.StopUsing();
                        transformCtxFieldName = null;
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
        
        ImGui.PopID();
    }
}