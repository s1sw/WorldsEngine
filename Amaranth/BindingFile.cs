namespace Amaranth;

class NativeBindMember
{
    public string NativeName;
    public string ManagedName;
}

class NativeBindProperty : NativeBindMember
{
    public string NativeType;
    public string ManagedType;
}

class NativeBindMethod : NativeBindMember
{
    public string NativeReturnType;
    public string ManagedReturnType;
}

class NativeBindClass
{
    public readonly string Name;
    public readonly List<NativeBindProperty> BindProperties = new();
    public readonly List<NativeBindMethod> BindMethods = new();

    public NativeBindClass(string Name)
    {
        this.Name = Name;
    }
}

class BindingFile
{
    public readonly List<string> Includes = new();
    public readonly List<NativeBindClass> NativeBindClasses = new();
}