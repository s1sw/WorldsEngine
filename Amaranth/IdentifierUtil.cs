namespace Amaranth;

static class IdentifierUtil
{
    public static string GetNamePart(string identifier)
    {
        bool isCpp = identifier.Contains("::");
        int idx = identifier.LastIndexOf(isCpp ? "::" : ".");
        idx += isCpp ? 2 : 1;

        return identifier.Substring(idx);
    }
}