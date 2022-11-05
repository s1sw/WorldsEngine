using System;
using System.Collections;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Runtime.Serialization;

namespace WorldsEngine.NativeInterop;

enum JsonType : byte
{
    Null,
    Object,
    Array,
    String,
    Boolean,
    Number_integer,
    Number_unsigned,
    Number_float,
    Binary,
    Discarded
}


public static class NmJson
{
    [DllImport(Engine.NativeModule, CharSet = CharSet.Ansi)]
    private static extern bool nmjson_contains(IntPtr nativePtr, string key);
    
    [DllImport(Engine.NativeModule, CharSet = CharSet.Ansi)]
    private static extern IntPtr nmjson_get(IntPtr nativePtr, string key);
    
    [DllImport(Engine.NativeModule, CharSet = CharSet.Ansi)]
    private static extern int nmjson_getAsInt(IntPtr nativePtr);
    
    [DllImport(Engine.NativeModule, CharSet = CharSet.Ansi)]
    private static extern uint nmjson_getAsUint(IntPtr nativePtr);
    
    [DllImport(Engine.NativeModule, CharSet = CharSet.Ansi)]
    private static extern float nmjson_getAsFloat(IntPtr nativePtr);
    
    [DllImport(Engine.NativeModule, CharSet = CharSet.Ansi)]
    private static extern float nmjson_getAsDouble(IntPtr nativePtr);
    
    [DllImport(Engine.NativeModule, CharSet = CharSet.Ansi)]
    private static extern IntPtr nmjson_getAsString(IntPtr nativePtr);

    [DllImport(Engine.NativeModule)]
    private static extern JsonType nmjson_getType(IntPtr nativePtr);

    [DllImport(Engine.NativeModule)]
    private static extern int nmjson_getCount(IntPtr nativePtr);
    
    [DllImport(Engine.NativeModule)]
    private static extern IntPtr nmjson_getArrayElement(IntPtr nativePtr, int idx);
    
    internal struct NativeJsonValue
    {
        public JsonType Type => nmjson_getType(nativePtr);
        public int IntValue => nmjson_getAsInt(nativePtr);
        public uint UintValue => nmjson_getAsUint(nativePtr);
        public float FloatValue => nmjson_getAsFloat(nativePtr);
        public double DoubleValue => nmjson_getAsDouble(nativePtr);

        public string StringValue => Marshal.PtrToStringUTF8(nmjson_getAsString(nativePtr))!;

        public bool IsNumber => Type == JsonType.Number_float || Type == JsonType.Number_integer ||
                                Type == JsonType.Number_unsigned;

        public int Count => nmjson_getCount(nativePtr);
        
        private IntPtr nativePtr;

        public NativeJsonValue(IntPtr nativePtr)
        {
            this.nativePtr = nativePtr;
        }

        public bool Contains(string key)
        {
            return nmjson_contains(nativePtr, key);
        }

        public NativeJsonValue this[string key] => new NativeJsonValue(nmjson_get(nativePtr, key));
        public NativeJsonValue this[int idx] => new NativeJsonValue(nmjson_getArrayElement(nativePtr, idx));
    }
    
    public static T Deserialize<T>(IntPtr nativePtr) => (T)Deserialize(typeof(T), new NativeJsonValue(nativePtr));

    public static object Deserialize(Type t, IntPtr nativePtr) => Deserialize(t, new NativeJsonValue(nativePtr));

    private static object Deserialize(Type t, NativeJsonValue jval)
    {
        if (jval.Type == JsonType.Null) return null;
        
        if (t == typeof(int))
        {
            if (!jval.IsNumber)
                throw new SerializationException($"Expected number, got {jval.Type}");
            return jval.IntValue;
        }
        else if (t == typeof(float))
        {
            if (!jval.IsNumber)
                throw new SerializationException($"Expected number, got {jval.Type}");
            return jval.FloatValue;
        }
        else if (t == typeof(double))
        {
            if (!jval.IsNumber)
                throw new SerializationException($"Expected number, got {jval.Type}");
            return jval.DoubleValue;
        }
        else if (t == typeof(bool))
        {
            if (jval.Type != JsonType.Boolean)
                throw new SerializationException($"Expected boolean, got {jval.Type}");
            return jval.IntValue == 1;
        }
        else if (t == typeof(string))
        {
            if (jval.Type != JsonType.String)
                throw new SerializationException($"Expected string, got {jval.Type}");
            return jval.StringValue;
        }
        else if (t.IsEnum)
        {
            if (!jval.IsNumber)
                throw new SerializationException($"Expected Number_unsigned for enum, got {jval.Type}");
            return jval.IntValue;
        }
        else if (t.IsGenericType && t.GetGenericTypeDefinition() == typeof(System.Collections.Generic.List<>))
        {
            if (jval.Type != JsonType.Array)
                throw new SerializationException($"Expected array, got {jval.Type}");

            Type elementType = t.GetGenericArguments()[0];
            int count = jval.Count;
            IList? list = (IList?)Activator.CreateInstance(t, count);

            if (list == null)
                throw new InvalidOperationException("Failed to create list");
            
            for (int i = 0; i < count; i++)
            {
                list.Add(Deserialize(elementType, jval[i]));
            }

            return list;
        }
        else if (t.IsArray)
        {
            if (jval.Type != JsonType.Array)
                throw new SerializationException($"Expected array, got {jval.Type}");

            Type elementType = t.GetElementType()!;
            int count = jval.Count;
            var array = Array.CreateInstance(elementType, count);

            for (int i = 0; i < count; i++)
            {
                array.SetValue(Deserialize(elementType, jval[i]), i);
            }

            return array;
        }
        
        if (jval.Type != JsonType.Object)
            throw new SerializationException($"Expected object, got {jval.Type}");

        object? o = Activator.CreateInstance(t);
        if (o == null)
        {
            Log.Warn($"Deserializing type {t} using uninitialized object");
            o = RuntimeHelpers.GetUninitializedObject(t);
        }

        var fields = t.GetFields(BindingFlags.Public | BindingFlags.Instance);

        foreach (var fieldInfo in fields)
        {
            if (fieldInfo.GetCustomAttribute<NonSerializedAttribute>() != null)
            {
                Log.Msg($"Ignoring {fieldInfo.Name}");
                continue;
            }
            
            if (jval.Contains(fieldInfo.Name))
            {
                fieldInfo.SetValue(o, Deserialize(fieldInfo.FieldType, jval[fieldInfo.Name]));
            }
        }

        return o;
    }
}