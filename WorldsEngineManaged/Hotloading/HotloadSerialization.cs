using System;
using System.Reflection;
using System.Collections.Generic;
using System.Runtime.Serialization;

namespace WorldsEngine
{
    struct SerializedField
    {
        public string FieldName;
        public object Value;
        public bool IsValueSerialized;
    }

    struct SerializedType
    {
        public bool IsNull;
        public string FullName;
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

        public static Assembly CurrentGameAssembly;

        static HotloadSerialization()
        {
            GameAssemblyManager.OnAssemblyLoad += (_) => DeserializeStatics();
            GameAssemblyManager.OnAssemblyUnload += SerializeStatics;
        }

        public static object Deserialize(SerializedType serializedType)
        {
            if (serializedType.IsNull) return null;

            // Find the type
            Type type = CurrentGameAssembly.GetType(serializedType.FullName, true);
            object instance = FormatterServices.GetUninitializedObject(type);

            // Restore field values
            FieldInfo[] fields = type.GetFields(SerializedFieldBindingFlags);
            foreach (var field in fields)
            {
                if (serializedType.Fields.ContainsKey(field.Name))
                {
                    var serializedField = serializedType.Fields[field.Name];

                    if (serializedField.IsValueSerialized)
                    {
                        field.SetValue(instance, Deserialize((SerializedType)serializedField.Value));
                    }
                    else
                    {
                        field.SetValue(instance, serializedField.Value);
                    }
                }
            }

            return instance;
        }

        public static SerializedType Serialize(object obj)
        {
            if (obj == null)
                return new SerializedType { IsNull = true };

            Type type = obj.GetType();
            var serialized = new SerializedType
            {
                FullName = type.FullName,
                Fields = new Dictionary<string, SerializedField>()
            };

            FieldInfo[] fields = type.GetFields(SerializedFieldBindingFlags);
            foreach (var field in fields)
            {
                var serializedField = new SerializedField
                {
                    FieldName = field.Name
                };

                if (field.FieldType.Assembly == CurrentGameAssembly)
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
            serializedStaticTypes.Clear();

            foreach (var type in CurrentGameAssembly.GetTypes())
            {
                var fields = type.GetFields(BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic);

                if (fields.Length == 0) continue;

                SerializedStaticType serializedStaticType = new()
                {
                    FullName = type.FullName,
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
                        field.SetValue(null, Deserialize((SerializedType)serializedField.Value));
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
