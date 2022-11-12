namespace Amaranth;

public enum TypeLanguage
{
    Amaranth,
    Cpp,
    CSharp
}

/// <summary>
/// Defines a type in Amaranth.
/// </summary>
public class AType
{
    public readonly TypeLanguage Language;
    public readonly string Namespace;
    public readonly string Name;

    public override int GetHashCode()
    {
        return $"{Language}:{Namespace}.{Name}".GetHashCode();
    }
}