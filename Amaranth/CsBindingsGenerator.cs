using System.Text;

namespace Amaranth;

class CsCppBindingsGenerator
{
    private CppType cppType;

    public CsCppBindingsGenerator(CppType type)
    {
        cppType = type;
    }

    public string GenerateBindings()
    {
        StringBuilder sb = new();
        string className = IdentifierUtil.GetNamePart(cppType.Identifier);
        sb.AppendLine($"class {className}");
        sb.AppendLine("{");
        sb.AppendLine("private IntPtr nativeInstance;");

        sb.Append($"public {className}(IntPtr instance)");
        sb.AppendLine("{");
        sb.AppendLine("nativeInstance = instance;");
        sb.AppendLine("}");
        foreach (ExposedProperty ep in cppType.ExposedProperties)
        {
            ITypeConverter? converter = TypeConverters.GetConverterFor(ep.NativeType);
            if (converter == null) throw new InvalidOperationException("oops");

            sb.AppendLine("[DllImport(Engine.NativeModule)]");
            sb.AppendLine($"private static extern {converter.ManagedGlueType} {ep.GetNativeMethodName(cppType.Identifier)}(IntPtr inst);");

            sb.AppendLine($"public {converter.ManagedExposedType} {ep.ExposedAs}");
            sb.AppendLine("{");
            sb.AppendLine("get");
            sb.AppendLine("{");
            sb.AppendLine($"{converter.ManagedGlueType} tmp = {ep.GetNativeMethodName(cppType.Identifier)}(nativeInstance);");
            sb.AppendLine(converter.GetManagedGlueCode("tmp"));
            sb.AppendLine("}");
            sb.AppendLine("}");
        }
        sb.Append("}");

        return sb.ToString();
    }
}