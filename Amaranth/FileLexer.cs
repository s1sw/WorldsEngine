using System.Text.RegularExpressions;

namespace Amaranth;

public enum TokenType
{
    Include,
    CppType,
    CppComponent,
    CsType,
    StringLiteral,
    Identifier,
    OpenParenthesis,
    CloseParenthesis,
    OpenBrace,
    CloseBrace,
    Comma,
    Arrow,
    Semicolon,
    NamespaceSeparator,
    Method,
    StaticMethod,
    Property,
    Function,
    Period,
    Field
}

public class Token
{
    public readonly TokenType Type;

    public Token(TokenType type)
    {
        Type = type;
    }

    public override string ToString()
    {
        return $"Token {Type}";
    }
}

public class StringLiteralToken : Token
{
    public readonly string Content;

    public StringLiteralToken(string content) : base(TokenType.StringLiteral)
    {
        Content = content;
    }

    public override string ToString()
    {
        return $"String Literal Token \"{Content}\"";
    }
}

public class IdentifierToken : Token
{
    public readonly string Identifier;

    public IdentifierToken(string identifier) : base(TokenType.Identifier)
    {
        Identifier = identifier;
    }

    public override string ToString()
    {
        return $"Identifier Token \"{Identifier}\"";
    }
}

public class TokenDef
{
    public readonly Regex Regex;
    public readonly TokenType Type;

    public TokenDef(string regexStr, TokenType type)
    {
        Regex = new Regex(regexStr, RegexOptions.Compiled);
        Type = type;
    }
}

public class BindingFileLexer
{
    private static TokenDef[] tokenDefs = 
    {
        new TokenDef("^Include", TokenType.Include),
        new TokenDef("^CppType", TokenType.CppType),
        new TokenDef("^CppComponent", TokenType.CppComponent),
        new TokenDef("^CsType", TokenType.CsType),
        new TokenDef("^\\(", TokenType.OpenParenthesis),
        new TokenDef("^\\)", TokenType.CloseParenthesis),
        new TokenDef("^{", TokenType.OpenBrace),
        new TokenDef("^}", TokenType.CloseBrace),
        new TokenDef("^,", TokenType.Comma),
        new TokenDef("^->", TokenType.Arrow),
        new TokenDef("^;", TokenType.Semicolon),
        new TokenDef("^::", TokenType.NamespaceSeparator),
        new TokenDef("^method", TokenType.Method),
        new TokenDef("^staticmethod", TokenType.StaticMethod),
        new TokenDef("^property", TokenType.Property),
        new TokenDef("^function", TokenType.Function),
        new TokenDef("^\\.", TokenType.Period),
        new TokenDef("^field", TokenType.Field)
    };

    private string lexingString;
    private static Regex stringLiteralRegex = new("^\\\"(.*)\\\"", RegexOptions.Compiled);
    private static Regex identifierRegex = new("^([A-z]+)", RegexOptions.Compiled);
    private static Regex whitespaceRegex = new("^(?://.+\\n|\\s+)", RegexOptions.Compiled);

    public BindingFileLexer(string targetStr)
    {
        lexingString = targetStr;
    }

    public List<Token> Lex()
    {
        List<Token> tokens = new();

        string str = lexingString;
        while (str.Length > 0)
        {
            bool matched = false;
            foreach (TokenDef td in tokenDefs)
            {
                var match = td.Regex.Match(str);

                if (!match.Success) continue;

                tokens.Add(new Token(td.Type));
                str = str.Substring(match.Length);
                matched = true;
                break;
            }

            if (matched) continue;

            var slMatch = stringLiteralRegex.Match(str);
            
            if (slMatch.Success)
            {
                StringLiteralToken slt = new(slMatch.Captures[0].Value.Replace("\"", ""));
                tokens.Add(slt);
                str = str.Substring(slMatch.Length);
                continue;
            }

            var identifierMatch = identifierRegex.Match(str);

            if (identifierMatch.Success)
            {
                IdentifierToken it = new(identifierMatch.Captures[0].Value);
                tokens.Add(it);
                str = str.Substring(identifierMatch.Length);
                continue;
            }

            var whitespaceMatch = whitespaceRegex.Match(str);

            if (whitespaceMatch.Success)
            {
                str = str.Substring(whitespaceMatch.Length);
                continue;
            }

            throw new InvalidDataException($"Dunno: {str}");
        }

        return tokens;
    }
}