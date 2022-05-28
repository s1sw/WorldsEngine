namespace Amaranth;

class ExposedProperty
{
    public string ExposedAs;
    public string NativeMethodName;
    public QualifiedIdentifier NativeType;

    public ExposedProperty(string exposedAs, string nativeMethodName, QualifiedIdentifier nativeType)
    {
        ExposedAs = exposedAs;
        NativeMethodName = nativeMethodName;
        NativeType = nativeType;
    }

    public string GetNativeMethodName(QualifiedIdentifier typeId)
    {
        return typeId.ToString().Replace("::", "__") + "_" + NativeMethodName;
    }
}

class CppType
{
    public QualifiedIdentifier Identifier;

    public readonly List<ExposedProperty> ExposedProperties = new();

    public CppType(QualifiedIdentifier qi)
    {
        Identifier = qi;
    }
}