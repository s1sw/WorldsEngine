namespace Amaranth;

class ExposedProperty
{
    public string ExposedAs;
    public string NativeMethodName;
    public string NativeType;

    public ExposedProperty(string exposedAs, string nativeMethodName, string nativeType)
    {
        ExposedAs = exposedAs;
        NativeMethodName = nativeMethodName;
        NativeType = nativeType;
    }

    public string GetNativeMethodName(string typeId)
    {
        return typeId.ToString().Replace("::", "__") + "_" + NativeMethodName;
    }
}

class CppType
{
    public string Identifier;

    public readonly List<ExposedProperty> ExposedProperties = new();

    public CppType(string qi)
    {
        Identifier = qi;
    }
}