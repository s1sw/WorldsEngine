using System.Text;

namespace Amaranth;

class CsBindingsGenerator
{
    private CppType cppType;

    public CsBindingsGenerator(CppType type)
    {
        cppType = type;
    }

    public string GenerateBindings()
    {
        StringBuilder sb = new();
        sb.Append("using System.Runtime.InteropServices;\n");
        sb.Append("class ");
        sb.Append(cppType.Identifier.NamePart);
        sb.Append("\n{\n");
        sb.Append("private IntPtr nativeInstance;\n");
        foreach (ExposedProperty ep in cppType.ExposedProperties)
        {
            ITypeConverter? converter = TypeConverters.GetConverterFor(ep.NativeType);
            if (converter == null) throw new InvalidOperationException("oops");

            sb.Append("[DllImport(Engine.NativeModule)]\n");
            sb.Append("private static extern ");
            sb.Append(converter.ManagedGlueType);
            sb.Append(" ");
            sb.Append(ep.GetNativeMethodName(cppType.Identifier));
            sb.Append("(IntPtr inst);\n");

            sb.Append("public ");
            sb.Append(converter.ManagedExposedType);
            sb.Append(" ");
            sb.Append(ep.ExposedAs);
            sb.Append("\n{\nget\n{\n");
            sb.Append(converter.ManagedGlueType);
            sb.Append(" tmp = ");
            sb.Append(ep.GetNativeMethodName(cppType.Identifier));
            sb.Append("(nativeInstance);\n");
            sb.Append(converter.GetManagedGlueCode("tmp"));
            sb.Append("\n}\n}\n");
        }
        sb.Append("}");

        return sb.ToString();
    }
}