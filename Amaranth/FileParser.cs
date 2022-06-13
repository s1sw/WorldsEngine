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

        return tokens[tokenIdx];
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
                (!prevWasSeparator && tok.Type != TokenType.NamespaceSeparator && tok.Type != TokenType.Period))
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
                identifier += tok.Type == TokenType.NamespaceSeparator ? "::" : ".";
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
    
    private void ParseCppTypeMethod(CppType cppType)
    {
        ConsumeToken(TokenType.Method);
        string returnType = ConsumeQualifiedIdentifier();

        var methodName = (IdentifierToken)ConsumeToken(TokenType.Identifier);

        ConsumeToken(TokenType.OpenParenthesis);

        ConsumeParameterList();

        ConsumeToken(TokenType.CloseParenthesis);
        ConsumeToken(TokenType.Arrow);
        ConsumeToken(TokenType.Property);

        var exposeAs = (IdentifierToken)ConsumeToken(TokenType.Identifier);

        ConsumeToken(TokenType.Semicolon);

        ExposedProperty ep = new(exposeAs.Identifier, methodName.Identifier, returnType);
        cppType.ExposedProperties.Add(ep);
    }

    private void ParseCppTypeField(CppType cppType)
    {
        ConsumeToken(TokenType.Field);
        string returnType = ConsumeQualifiedIdentifier();

        var fieldName = (IdentifierToken)ConsumeToken(TokenType.Identifier);

        ConsumeToken(TokenType.Arrow);
        ConsumeToken(TokenType.Property);

        var exposeAs = (IdentifierToken)ConsumeToken(TokenType.Identifier);

        ConsumeToken(TokenType.Semicolon);

        ExposedField ef = new(exposeAs.Identifier, fieldName.Identifier, returnType);
        cppType.ExposedFields.Add(ef);
    }

    private CppType ParseCppType(bool isComponent)
    {
        string classIdentifier = ConsumeQualifiedIdentifier();
        ConsumeToken(TokenType.OpenBrace);

        CppType cppType = new(classIdentifier);

        cppType.IsComponent = isComponent;

        while (true)
        {
            if (NextTokenType == TokenType.CloseBrace) break;
            
            switch (NextTokenType)
            {
            case TokenType.Method:
                ParseCppTypeMethod(cppType);
                break;
            case TokenType.Field:
                ParseCppTypeField(cppType);
                break;
            }
        }

        ConsumeToken(TokenType.CloseBrace);

        return cppType;
    }

    private void ParseCsType()
    {
        string classIdentifier = ConsumeQualifiedIdentifier();
        ConsumeToken(TokenType.OpenBrace);

        while (true)
        {
            if (NextTokenType == TokenType.CloseBrace) break;

            ConsumeToken(TokenType.Method, TokenType.StaticMethod);

            ConsumeToken(TokenType.Identifier);

            ConsumeToken(TokenType.OpenParenthesis);

            ConsumeParameterList();

            ConsumeToken(TokenType.CloseParenthesis);

            ConsumeToken(TokenType.Arrow);
            ConsumeToken(TokenType.Property, TokenType.Function);

            ConsumeQualifiedIdentifier();
            ConsumeToken(TokenType.Semicolon);
        }

        ConsumeToken(TokenType.CloseBrace);
    }

    public BindingFile Parse()
    {
        BindingFile bf = new();

        while (tokenIdx < tokens.Count)
        {
            Token currentToken = tokens[tokenIdx];

            switch (currentToken.Type)
            {
            case TokenType.Include:
                var includeLoc = (StringLiteralToken)ConsumeToken(TokenType.StringLiteral);
                ConsumeToken(TokenType.Semicolon);
                bf.Includes.Add(includeLoc.Content);
                break;
            case TokenType.CppComponent:
                bf.CppTypes.Add(ParseCppType(true));
                break;
            case TokenType.CppType:
                bf.CppTypes.Add(ParseCppType(false));
                break;
            case TokenType.CsType:
                ParseCsType();
                break;
            default:
                throw new Exception($"Didn't expect a {currentToken.Type}!");
            }
            tokenIdx++;
        }

        return bf;
    }
}