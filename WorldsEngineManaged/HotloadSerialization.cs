using System;
using System.Reflection;
using System.Collections.Generic;

namespace WorldsEngine
{
    struct SerializedField
    {
        public string FieldName;
        public object Value;
        public string FullTypeName;
        public Type Type;
    }

    struct SerializedType
    {
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

        public static object Deserialize(Assembly assembly, SerializedType serializedType, object[] constructorArgs)
        {
            // Find the type
            var type = assembly.GetType(serializedType.FullName, true);
            var instance = Activator.CreateInstance(type, constructorArgs);

            // Restore field values
            var fields = type.GetFields(SerializedFieldBindingFlags);
            foreach (var field in fields)
            {
                if (serializedType.Fields.ContainsKey(field.Name))
                    field.SetValue(instance, serializedType.Fields[field.Name].Value);
            }

            return instance;
        }

        public static SerializedType Serialize(object obj)
        {
            var type = obj.GetType();
            var serialized = new SerializedType();

            serialized.FullName = type.FullName;
            serialized.Fields = new Dictionary<string, SerializedField>();

            var fields = type.GetFields(SerializedFieldBindingFlags);
            foreach (var field in fields)
            {
                var serializedField = new SerializedField();
                serializedField.FieldName = field.Name;
                serializedField.Type = field.FieldType;
                serializedField.Value = field.GetValue(obj);

                serialized.Fields.Add(field.Name, serializedField);
            }

            return serialized;
        }
    }
}
