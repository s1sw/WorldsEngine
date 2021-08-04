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

    static class HotloadSerialization
    {
        const BindingFlags SerializedFieldBindingFlags =
              BindingFlags.NonPublic
            | BindingFlags.Public
            | BindingFlags.FlattenHierarchy
            | BindingFlags.Instance;

        public static Assembly CurrentGameAssembly;

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
    }
}
