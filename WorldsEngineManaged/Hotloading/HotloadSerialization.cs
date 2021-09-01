using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.Serialization;

namespace WorldsEngine
{
    class SerializedType
    {
        public bool IsGeneric;
        public Type? NonGameType;
        public SerializedType? GenericDefinitionType;
        public string? FullName;
        public SerializedType[]? GenericParameters;

        public SerializedType(Type type)
        {
            if (type.Assembly != HotloadSerialization.CurrentGameAssembly)
            {
                NonGameType = type;
            }

            if (type.IsGenericType && !type.IsGenericTypeDefinition)
            {
                NonGameType = type.GetGenericTypeDefinition();
                GenericParameters = new SerializedType[type.GenericTypeArguments.Length];

                for (int i = 0; i < GenericParameters.Length; i++)
                {
                    GenericParameters[i] = new SerializedType(type.GenericTypeArguments[i]);
                }

                IsGeneric = true;
                GenericDefinitionType = new SerializedType(type.GetGenericTypeDefinition());
            }
            else
            {
                FullName = type.FullName;
                GenericParameters = null;
                IsGeneric = false;
            }
        }

        public Type Deserialize(Assembly gameAssembly)
        {
            if (IsGeneric)
            {
                Type[] genericTypes = new Type[GenericParameters!.Length];

                for (int i = 0; i < GenericParameters!.Length; i++)
                {
                    genericTypes[i] = GenericParameters[i].Deserialize(gameAssembly);
                }

                return GenericDefinitionType!.Deserialize(gameAssembly).MakeGenericType(genericTypes);
            }

            return NonGameType ?? gameAssembly.GetType(FullName!)!;
        }
    }

    struct SerializedField
    {
        public string FieldName;
        public object? Value;
        public bool IsValueSerialized;
    }

    struct ArrayInfo
    {
        public int Length;
        public SerializedObject[] Items;
    }

    struct SerializedObject
    {
        public bool IsNull;
        public ArrayInfo? ArrayInfo;
        public SerializedType SerializedType;
        public Dictionary<string, SerializedField> Fields;
    }

    struct SerializedStaticType
    {
        public string FullName;
        public List<SerializedField> Fields;
    }

    static class HotloadSerialization
    {
        const BindingFlags SerializedFieldBindingFlags =
              BindingFlags.NonPublic
            | BindingFlags.Public
            | BindingFlags.FlattenHierarchy
            | BindingFlags.Instance;

        public static Assembly? CurrentGameAssembly;

        static HotloadSerialization()
        {
            GameAssemblyManager.OnAssemblyLoad += (_) => DeserializeStatics();
            GameAssemblyManager.OnAssemblyUnload += SerializeStatics;
        }

        private static bool HasGenericParameterFromGameAssembly(Type genericType)
        {
            for (int i = 0; i < genericType.GenericTypeArguments.Length; i++)
            {
                Type argument = genericType.GenericTypeArguments[i];

                if (argument.Assembly == CurrentGameAssembly)
                    return true;
            }

            return false;
        }

        public static object? Deserialize(SerializedObject serializedObject)
        {
            if (CurrentGameAssembly == null) throw new InvalidOperationException();
            if (serializedObject.IsNull) return null;

            // Find the type
            Type type = serializedObject.SerializedType.Deserialize(CurrentGameAssembly);

            if (type.IsArray)
            {
                Array arr = Array.CreateInstance(type.GetElementType()!, serializedObject.ArrayInfo!.Value.Length);

                for (int i = 0; i < arr.Length; i++)
                {
                    arr.SetValue(Deserialize(serializedObject.ArrayInfo.Value.Items[i]), i);
                }

                return arr;
            }

            object instance = FormatterServices.GetUninitializedObject(type);

            // Restore field values
            FieldInfo[] fields = type.GetFields(SerializedFieldBindingFlags);
            foreach (var field in fields)
            {
                if (serializedObject.Fields.ContainsKey(field.Name))
                {
                    var serializedField = serializedObject.Fields[field.Name];

                    if (serializedField.IsValueSerialized)
                    {
                        field.SetValue(instance, Deserialize((SerializedObject)serializedField.Value!));
                    }
                    else
                    {
                        field.SetValue(instance, serializedField.Value);
                    }
                }
            }

            return instance;
        }

        public static SerializedObject Serialize(object? obj)
        {
            if (obj == null)
                return new SerializedObject { IsNull = true };

            Type type = obj.GetType();
            var serialized = new SerializedObject
            {
                SerializedType = new SerializedType(type),
                Fields = new Dictionary<string, SerializedField>()
            };

            if (type.IsArray)
            {
                Array arr = (Array)obj;
                serialized.ArrayInfo = new ArrayInfo()
                {
                    Length = arr.Length,
                    Items = new SerializedObject[arr.Length]
                };

                for (int i = 0; i < arr.Length; i++)
                {
                    serialized.ArrayInfo.Value.Items[i] = Serialize(arr.GetValue(i));
                }

                return serialized;
            }

            FieldInfo[] fields = type.GetFields(SerializedFieldBindingFlags);
            foreach (var field in fields)
            {
                var serializedField = new SerializedField
                {
                    FieldName = field.Name
                };

                if (field.FieldType.Assembly == CurrentGameAssembly || HasGenericParameterFromGameAssembly(field.FieldType))
                {
                    serializedField.IsValueSerialized = true;
                    serializedField.Value = Serialize(field.GetValue(obj));
                }
                else
                {
                    serializedField.Value = field.GetValue(obj);
                }

                serialized.Fields.Add(field.Name, serializedField);
            }

            return serialized;
        }

        private static List<SerializedStaticType> serializedStaticTypes = new();

        public static void SerializeStatics()
        {
            if (CurrentGameAssembly == null) throw new InvalidOperationException();

            serializedStaticTypes.Clear();

            foreach (var type in CurrentGameAssembly.GetTypes())
            {
                var fields = type.GetFields(BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic);

                if (fields.Length == 0) continue;

                SerializedStaticType serializedStaticType = new()
                {
                    FullName = type.FullName!,
                    Fields = new List<SerializedField>()
                };

                foreach (var field in fields)
                {
                    if (field.IsLiteral && !field.IsInitOnly) continue;

                    SerializedField serializedField = new();
                    serializedField.FieldName = field.Name;

                    if (field.FieldType.Assembly == CurrentGameAssembly)
                    {
                        serializedField.IsValueSerialized = true;
                        serializedField.Value = Serialize(field.GetValue(null));
                    }
                    else
                    {
                        serializedField.Value = field.GetValue(null);
                    }

                    serializedStaticType.Fields.Add(serializedField);
                }

                serializedStaticTypes.Add(serializedStaticType);
            }
        }

        public static void DeserializeStatics()
        {
            if (CurrentGameAssembly == null) throw new InvalidOperationException();

            foreach (var serializedType in serializedStaticTypes)
            {
                var type = CurrentGameAssembly.GetType(serializedType.FullName);

                if (type == null) continue;

                // Force initialise static fields so they don't overwrite our serialised values
                System.Runtime.CompilerServices.RuntimeHelpers.RunClassConstructor(type.TypeHandle);

                foreach (var serializedField in serializedType.Fields)
                {
                    var field = type.GetField(serializedField.FieldName, BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static);

                    if (field == null) continue;

                    if (serializedField.IsValueSerialized)
                    {
                        field.SetValue(null, Deserialize((SerializedObject)serializedField.Value!));
                    }
                    else
                    {
                        field.SetValue(null, serializedField.Value);
                    }
                }
            }

            serializedStaticTypes.Clear();
        }
    }
}
