namespace Amaranth;

class Parameter
{
    public string Type;
    public string Name;

    public Parameter(string type, string name)
    {
        Type = type;
        Name = name;
    }
}

class BindingFileParser
{
    private List<Token> tokens;
    private int tokenIdx = 0;

    public BindingFileParser(List<Token> toks)
    {
        tokens = toks;
    }

    private Token ConsumeToken(TokenType type)
    {
        tokenIdx++;
        if (tokenIdx >= tokens.Count)
        {
            throw new Exception("unexpected end of token stream");
        }

        if (tokens[tokenIdx].Type != type)
        {
            throw new Exception($"expected {type}, got {tokens[tokenIdx].Type}");
        }

        return tokens[tokenIdx];
    }

    private Token ConsumeToken(params TokenType[] types)
    {
        tokenIdx++;
        if (tokenIdx >= tokens.Count)
        {
            throw new Exception("unexpected end of token stream");
        }
        
        ExpectTokenType(tokens[tokenIdx], types);

        return tokens[tokenIdx];
    }

    private KeywordToken ConsumeKeyword(params string[] keywords)
    {
        var kt = (KeywordToken)ConsumeToken(TokenType.Keyword);

        if (!keywords.Contains(kt.Keyword))
        {
            throw new Exception($"expected {keywords}, got {kt.Keyword}");
        }

        return kt;
    }

    private void ExpectTokenType(Token token, params TokenType[] types)
    {
        bool matched = false;
        foreach (TokenType type in types)
        {
            if (tokens[tokenIdx].Type == type)
            {
                matched = true;
                break;
            }
        }

        if (!matched)
            throw new Exception($"expected {types}, got {tokens[tokenIdx].Type}");
    }

    private string ConsumeQualifiedIdentifier()
    {
        string identifier = string.Empty;
        bool prevWasSeparator = true;

        while(true)
        {
            tokenIdx++;

            var tok = tokens[tokenIdx];

            if ((prevWasSeparator && tok.Type != TokenType.Identifier) || 
                (!prevWasSeparator && tok.Type != TokenType.Period))
            {
                tokenIdx--;
                break;
            }

            if (tok.Type == TokenType.Identifier)
            {
                identifier += ((IdentifierToken)tok).Identifier;
                prevWasSeparator = false;
            }
            else
            {
                identifier += ".";
                prevWasSeparator = true;
            }
        }

        return identifier;
    }

    private List<Parameter> ConsumeParameterList()
    {
        List<Parameter> parameters = new();

        while (NextTokenType != TokenType.CloseParenthesis)
        {
            string type = ConsumeQualifiedIdentifier();

            var it = (IdentifierToken)ConsumeToken(TokenType.Identifier);
            string name = it.Identifier;

            Parameter p = new(type, name);
            parameters.Add(p);

            if (NextTokenType == TokenType.Comma)
                ConsumeToken(TokenType.Comma);
        }

        return parameters;
    }

    private TokenType NextTokenType => tokens[tokenIdx + 1].Type;
    
    private NativeBindMember ParseNativeBindMember()
    {
        ConsumeKeyword("method");
        string methodReturnType = ConsumeQualifiedIdentifier();

        var methodName = (IdentifierToken)ConsumeToken(TokenType.Identifier);

        ConsumeToken(TokenType.OpenParenthesis);

        ConsumeParameterList();

        ConsumeToken(TokenType.CloseParenthesis);
        ConsumeToken(TokenType.Arrow);

        string managedType = ConsumeQualifiedIdentifier();

        var exposeAs = (IdentifierToken)ConsumeToken(TokenType.Identifier);

        NativeBindMember member;
        if (NextTokenType == TokenType.OpenParenthesis)
        {
            // native bind method
            member = new NativeBindMethod
            {
                ManagedReturnType = managedType,
                NativeReturnType = methodReturnType
            };
            ConsumeToken(TokenType.OpenParenthesis);
            ConsumeToken(TokenType.CloseParenthesis);
        }
        else
        {
            member = new NativeBindProperty
            {
                ManagedType = managedType,
                NativeType = methodReturnType
            };
        }

        member.NativeName = methodName.Identifier;
        member.ManagedName = exposeAs.Identifier;

        ConsumeToken(TokenType.Semicolon);

        return member;
    }

    private NativeBindClass ParseNativeBindClass()
    {
        string classIdentifier = ConsumeQualifiedIdentifier();
        ConsumeToken(TokenType.OpenBrace);

        NativeBindClass nbc = new(classIdentifier);

        while (true)
        {
            if (NextTokenType == TokenType.CloseBrace) break;
            var member = ParseNativeBindMember();
            if (member is NativeBindProperty)
                nbc.BindProperties.Add((NativeBindProperty)member);
            else
                nbc.BindMethods.Add((NativeBindMethod)member);
        }

        ConsumeToken(TokenType.CloseBrace);

        return nbc;
    }

    public BindingFile Parse()
    {
        BindingFile bf = new();

        while (tokenIdx < tokens.Count)
        {
            Token currentToken = tokens[tokenIdx];

            if (currentToken.Type != TokenType.Keyword)
            {
                throw new Exception($"expected Keyword, got {currentToken.Type}");
            }

            KeywordToken kt = (KeywordToken)currentToken;

            if (kt.Keyword == "nativebindclass")
            {
                bf.NativeBindClasses.Add(ParseNativeBindClass());
            }
            else
            {
                throw new NotImplementedException();
            }

            tokenIdx++;
        }

        return bf;
    }
}