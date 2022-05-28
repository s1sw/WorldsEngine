using System.IO;
using Amaranth;

string file = File.ReadAllText("ExampleBinding.amth");

BindingFileLexer bfl = new(file);
var tokens = bfl.Lex();

BindingFileParser bfp = new(tokens);
BindingFile bindingFile = bfp.Parse();

foreach (CppType cppType in bindingFile.CppTypes)
{
    Console.WriteLine($"CppType: {cppType.Identifier.NamePart} (namespace {cppType.Identifier.NamespacePart})");

    foreach (var e in cppType.ExposedProperties)
    {
        Console.WriteLine($"Exposes {e.NativeMethodName} as {e.ExposedAs}");
    }

    CppBindingsGenerator bg = new(cppType);
    File.WriteAllText("cppbindings.cpp", bg.GenerateBindings());
    CsBindingsGenerator csbg = new(cppType);
    File.WriteAllText("csbindings.cs", csbg.GenerateBindings());
}


static class Engine
{
    public const string NativeModule = "lol";
}