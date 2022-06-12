using System.Reflection;

namespace Amaranth;

interface ITypeConverter
{
    public string ManagedExposedType { get; }
    public string ManagedGlueType { get; }
    public string NativeGlueType { get; }

    public bool PassByRef { get; }

    public string NativeToGlue(string varAccess);
    public string GlueToNative(string varAccess);
    public string GlueToManaged(string retValRef);
}

[AttributeUsage(AttributeTargets.Class)]
sealed class TypeConverterAttribute : Attribute
{
    public string NativeTypeIdentifier => nativeTypeIdentifier;

    readonly string nativeTypeIdentifier;
    
    public TypeConverterAttribute(string nativeTypeIdentifier)
    {
        this.nativeTypeIdentifier = nativeTypeIdentifier;
    }
}

static class TypeConverters
{
    private static readonly Dictionary<string, ITypeConverter> converters = new();

    static TypeConverters()
    {
        Assembly thisAssembly = Assembly.GetExecutingAssembly();
        
        foreach (Type t in thisAssembly.GetTypes())
        {
            var attr = t.GetCustomAttribute<TypeConverterAttribute>();

            if (attr == null) continue;

            converters.Add(attr.NativeTypeIdentifier, (ITypeConverter)Activator.CreateInstance(t)!);
        }
    }

    public static ITypeConverter? GetConverterFor(string nativeType)
    {
        if (!converters.ContainsKey(nativeType)) return null;

        return converters[nativeType];
    }
}

[TypeConverter("std::string_view")]
class StdStringViewConverter : ITypeConverter
{
    public string ManagedExposedType => "string";
    public string ManagedGlueType => "IntPtr";
    public string NativeGlueType => "char*";
    public bool PassByRef => false;

    public string NativeToGlue(string varAccess)
    {
        return $"strdup(std::string({varAccess}).c_str())";
    }

    public string GlueToManaged(string retValRef)
    {
        return $"string t = Marshal.PtrToStringUTF8({retValRef})!;\nMarshal.FreeHGlobal({retValRef});\nreturn t;";
    }

    public string GlueToNative(string varAccess)
    {
        throw new NotImplementedException();
    }
}

[TypeConverter("int")]
class IntConverter : ITypeConverter
{
    public string ManagedExposedType => "int";
    public string ManagedGlueType => "int";
    public string NativeGlueType => "int";
    public bool PassByRef => false;

    public string NativeToGlue(string varAccess)
    {
        return varAccess;
    }

    public string GlueToManaged(string retValRef)
    {
        return $"return {retValRef};";
    }

    public string GlueToNative(string varAccess)
    {
        return varAccess;
    }
}

[TypeConverter("bool")]
class BoolConverter : ITypeConverter
{
    public string ManagedExposedType => "bool";
    public string ManagedGlueType => "byte";
    public string NativeGlueType => "bool";
    public bool PassByRef => false;

    public string NativeToGlue(string varAccess) => varAccess;
    public string GlueToManaged(string retValRef) => $"return (bool){retValRef};";
    public string GlueToNative(string varAccess) => varAccess;
}

[TypeConverter("std::string")]
class StdStringConverter : ITypeConverter
{
    public string ManagedExposedType => "string";
    public string ManagedGlueType => "IntPtr";
    public string NativeGlueType => "char*";
    public bool PassByRef => false;

    public string NativeToGlue(string varAccess) => $"{varAccess}.c_str()";
    public string GlueToManaged(string retValRef) => $"return Marshal.PtrToStringUTF8({retValRef})!;";
    public string GlueToNative(string varAccess) => $"std::string({varAccess})";
}

[TypeConverter("glm::vec3")]
class Vector3Converter : ITypeConverter
{
    public string ManagedExposedType => "WorldsEngine.Math.Vector3";
    public string ManagedGlueType => "WorldsEngine.Math.Vector3";
    public string NativeGlueType => "glm::vec3";
    public bool PassByRef => true;

    public string NativeToGlue(string varAccess) => $"&{varAccess}";
    public string GlueToManaged(string retValRef) => $"return {retValRef};";
    public string GlueToNative(string varAcces) => "";
}