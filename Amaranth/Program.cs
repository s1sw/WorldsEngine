using System.IO;
using System.Text;
using Amaranth;

string file = File.ReadAllText("ExampleBinding.amth");

BindingFileLexer bfl = new(file);
var tokens = bfl.Lex();

BindingFileParser bfp = new(tokens);
BindingFile bindingFile = bfp.Parse();

StringBuilder cppFile = new();
StringBuilder csFile = new();

cppFile.AppendLine("#include \"Export.hpp\"");
foreach (string include in bindingFile.Includes)
{
    cppFile.AppendLine($"#include <{include}>");
}

cppFile.AppendLine();

csFile.AppendLine("using System;");
csFile.AppendLine("using System.Runtime.InteropServices;");
csFile.AppendLine("namespace WorldsEngine.Editor;");

foreach (CppType cppType in bindingFile.CppTypes)
{
    Console.WriteLine($"CppType: {cppType.Identifier}");

    foreach (var e in cppType.ExposedProperties)
    {
        Console.WriteLine($"Exposes {e.NativeMethodName} as {e.ExposedAs}");
    }

    CppBindingsGenerator bg = new(cppType);
    bg.GenerateBindings(cppFile);
    CsCppBindingsGenerator csbg = new(cppType);
    csFile.Append(csbg.GenerateBindings());
}

File.WriteAllText("cppbindings.cpp", cppFile.ToString());
File.WriteAllText("csbindings.cs", csFile.ToString());

static class Engine
{
    public const string NativeModule = "lol";
}