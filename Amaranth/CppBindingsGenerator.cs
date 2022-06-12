using System.Text;

namespace Amaranth;

class CppBindingsGenerator
{
    private CppType cppType;

    public CppBindingsGenerator(CppType type)
    {
        cppType = type;
    }

    public void GenerateBindings(StringBuilder sb)
    {
        foreach (ExposedProperty ep in cppType.ExposedProperties)
        {
            ITypeConverter? converter = TypeConverters.GetConverterFor(ep.NativeType);
            if (converter == null) throw new InvalidOperationException("oops");

            sb.Append("EXPORT ");
            sb.Append(converter.NativeGlueType);
            sb.Append(" ");

            string methodName = ep.GetNativeMethodName(cppType.Identifier);
            sb.Append(methodName);
            sb.Append("(");
            
            string varAccess;
            if (cppType.IsComponent)
            {
                sb.Append("entt::registry* reg, entt::entity entity) {\n");
                varAccess = $"(reg->get<{cppType.Identifier}>(entity)).{ep.NativeMethodName}()";
            }
            else
            {
                sb.Append(cppType.Identifier.ToString());
                sb.Append("* inst) {\n");
                varAccess = $"(inst->{ep.NativeMethodName}())";
            }

            sb.Append("    return ");
            sb.Append(converter.NativeToGlue(varAccess));
            sb.Append(";\n");
            sb.Append("}\n\n");
        }

        foreach (ExposedField ef in cppType.ExposedFields)
        {
            ITypeConverter? converter = TypeConverters.GetConverterFor(ef.NativeType);
            if (converter == null) throw new InvalidOperationException("oops");

            // Get
            {
                sb.Append("EXPORT ");
                sb.Append(converter.NativeGlueType);
                sb.Append(" ");

                string methodName = ef.GetBridgeGetter(cppType.Identifier);
                sb.Append(methodName);
                sb.Append("(");
                
                string varAccess;
                if (cppType.IsComponent)
                {
                    sb.Append("entt::registry* reg, entt::entity entity) {\n");
                    varAccess = $"(reg->get<{cppType.Identifier}>(entity)).{ef.NativeFieldName}";
                }
                else
                {
                    sb.Append(cppType.Identifier.ToString());
                    sb.Append("* inst) {\n");
                    varAccess = $"(inst->{ef.NativeFieldName})";
                }

                sb.Append("    return ");
                sb.Append(converter.NativeToGlue(varAccess));
                sb.Append(";\n");
                sb.Append("}\n\n");
            }

            // Set
            {
                sb.Append("EXPORT void ");

                string methodName = ef.GetBridgeSetter(cppType.Identifier);
                sb.Append(methodName);
                sb.Append("(");
                
                string varAccess;
                if (cppType.IsComponent)
                {
                    sb.Append("entt::registry* reg, entt::entity entity) {\n");
                    varAccess = $"(reg->get<{cppType.Identifier}>(entity)).{ef.NativeFieldName}";
                }
                else
                {
                    sb.Append(cppType.Identifier.ToString());
                    sb.Append("* inst) {\n");
                    varAccess = $"(inst->{ef.NativeFieldName})";
                }

                sb.Append("    return ");
                sb.Append(converter.NativeToGlue(varAccess));
                sb.Append(";\n");
                sb.Append("}\n\n");
            }
        }
    }
}

class CppCsBindingsGenerator
{
    private CsType csType;

    public CppCsBindingsGenerator(CsType type)
    {
        csType = type;
    }

    public void GenerateBindings(StringBuilder sb)
    {
    }
}