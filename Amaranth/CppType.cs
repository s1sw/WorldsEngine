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

class ExposedField
{
    public string ExposedAs;
    public string NativeFieldName;
    public string NativeType;

    public ExposedField(string exposedAs, string nativeFieldName, string nativeType)
    {
        ExposedAs = exposedAs;
        NativeFieldName = nativeFieldName;
        NativeType = nativeType;
    }

    public string GetBridgeGetter(string typeId)
    {
        return GetBridgeBase(typeId) + "_get";
    }

    public string GetBridgeSetter(string typeId)
    {
        return GetBridgeBase(typeId) + "_set";
    }

    private string GetBridgeBase(string typeId)
    {
        return typeId.ToString().Replace("::", "__") + "_" + NativeFieldName;
    }
}

class CppType
{
    public string Identifier;

    public readonly List<ExposedProperty> ExposedProperties = new();
    public readonly List<ExposedField> ExposedFields = new();

    public bool IsComponent = false;

    public CppType(string qi)
    {
        Identifier = qi;
    }
}