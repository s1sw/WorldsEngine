namespace Amaranth;

class QualifiedIdentifier
{
    public string NamespacePart = string.Empty;
    public string NamePart = string.Empty;

    public override string ToString()
    {
        if (string.IsNullOrEmpty(NamespacePart))
            return NamePart;
        else
            return NamespacePart + "::" + NamePart;
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

    private QualifiedIdentifier ConsumeQualifiedIdentifier()
    {
        QualifiedIdentifier qi = new();
        bool prevWasSeparator = true;

        while(true)
        {
            tokenIdx++;

            var tok = tokens[tokenIdx];

            if ((prevWasSeparator && tok.Type != TokenType.Identifier) || 
                (!prevWasSeparator && tok.Type != TokenType.NamespaceSeparator))
            {
                tokenIdx--;
                break;
            }

            if (tok.Type == TokenType.Identifier)
            {
                qi.NamePart = ((IdentifierToken)tok).Identifier;
                prevWasSeparator = false;
            }
            else
            {
                if (qi.NamespacePart.Length == 0)
                {
                    qi.NamespacePart = qi.NamePart;
                }
                else
                {
                    qi.NamespacePart += "::" + qi.NamePart;
                    qi.NamePart = string.Empty;
                }
                prevWasSeparator = true;
            }
        }

        return qi;
    }

    private TokenType NextTokenType => tokens[tokenIdx + 1].Type;

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
            case TokenType.CppType:
                QualifiedIdentifier classIdentifier = ConsumeQualifiedIdentifier();
                ConsumeToken(TokenType.OpenBrace);

                CppType cppType = new(classIdentifier);

                while (true)
                {
                    if (NextTokenType == TokenType.CloseBrace) break;
                    ConsumeToken(TokenType.Method);
                    QualifiedIdentifier returnType = ConsumeQualifiedIdentifier();

                    var methodName = (IdentifierToken)ConsumeToken(TokenType.Identifier);

                    ConsumeToken(TokenType.OpenParenthesis);
                    ConsumeToken(TokenType.CloseParenthesis);
                    ConsumeToken(TokenType.Arrow);
                    ConsumeToken(TokenType.Property);

                    var exposeAs = (IdentifierToken)ConsumeToken(TokenType.Identifier);

                    ConsumeToken(TokenType.Semicolon);

                    ExposedProperty ep = new(exposeAs.Identifier, methodName.Identifier, returnType);
                    cppType.ExposedProperties.Add(ep);
                }

                ConsumeToken(TokenType.CloseBrace);
                bf.CppTypes.Add(cppType);
                break;
            default:
                throw new Exception($"Didn't expect a {currentToken.Type}!");
            }
            tokenIdx++;
        }

        return bf;
    }
}