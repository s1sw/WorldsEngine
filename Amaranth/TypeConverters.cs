using System.Reflection;

namespace Amaranth;

interface ITypeConverter
{
    public string ManagedExposedType { get; }
    public string ManagedGlueType { get; }

    public string NativeGlueType { get; }

    public string GetNativeGlueCode(string varAccess);
    public string GetManagedGlueCode(string retValRef);
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

    public string GetNativeGlueCode(string varAccess)
    {
        return $"strdup(std::string({varAccess}).c_str())";
    }

    public string GetManagedGlueCode(string retValRef)
    {
        return $"string t = Marshal.PtrToStringUTF8({retValRef})!;\nMarshal.FreeHGlobal({retValRef});\nreturn t;";
    }
}

[TypeConverter("int")]
class IntConverter : ITypeConverter
{
    public string ManagedExposedType => "int";
    public string ManagedGlueType => "int";
    public string NativeGlueType => "int";

    public string GetNativeGlueCode(string varAccess)
    {
        return varAccess;
    }

    public string GetManagedGlueCode(string retValRef)
    {
        return $"return {retValRef};";
    }
}

[TypeConverter("std::string")]
class StdStringConverter : ITypeConverter
{
    public string ManagedExposedType => "string";
    public string ManagedGlueType => "IntPtr";
    public string NativeGlueType => "char*";

    public string GetNativeGlueCode(string varAccess)
    {
        return $"{varAccess}.c_str()";
    }

    public string GetManagedGlueCode(string retValRef)
    {
        return $"return Marshal.PtrToStringUTF8({retValRef})!;";
    }
}