using System.IO;
using System.Text;
using Amaranth;

string file = File.ReadAllText("ExampleBinding.amth");

BindingFileLexer bfl = new(file);
var tokens = bfl.Lex();

BindingFileParser bfp = new(tokens);
BindingFile bindingFile = bfp.Parse();

foreach (NativeBindClass nbc in bindingFile.NativeBindClasses)
{
    Console.WriteLine($"nbc {nbc.Name}");
    Console.WriteLine("bind methods:");
    foreach (NativeBindMethod method in nbc.BindMethods)
    {
        Console.WriteLine($"{method.NativeReturnType} {method.NativeName} -> {method.ManagedName}");
    }
    
    Console.WriteLine("bind props:");
    foreach (NativeBindProperty property in nbc.BindProperties)
    {
        Console.WriteLine($"{property.NativeType} {property.NativeName} -> {property.ManagedType} {property.ManagedName}");
    }

    StringBuilder cppBinding = new();
    foreach (NativeBindMethod method in nbc.BindMethods)
    {
        
    }
}

static class Engine
{
    public const string NativeModule = "lol";
}