#include <frontend/parser/parser.h>
#include <debug.h>

namespace FE
{
    using type = YaccParser::symbol_type;
    using kind = YaccParser::symbol_kind;

    void Parser::reportError(const location& loc, const std::string& message)
    {
        ++lexicalErrorCount_;
        _parser.error(loc, message);
    }

    std::vector<Token> Parser::parseTokens_impl()
    {
        std::vector<Token> tokens;
        while (true)
        {
            type token = _scanner.nextToken();
            if (token.kind() == kind::S_END) break;

            Token result;
            result.token_name    = token.name();
            result.line_number   = token.location.begin.line;
            result.column_number = token.location.begin.column - 1;
            result.lexeme.assign(_scanner.YYText(), static_cast<size_t>(_scanner.YYLeng()));

            switch (token.kind())
            {
                case kind::S_INT_CONST:
                    result.ival = token.value.as<int>();
                    result.type = Token::TokenType::T_INT;
                    break;
                case kind::S_LL_CONST:
                    result.lval = token.value.as<long long>();
                    result.type = Token::TokenType::T_LL;
                    break;
                case kind::S_FLOAT_CONST:
                    result.fval = token.value.as<float>();
                    result.type = Token::TokenType::T_FLOAT;
                    break;
                case kind::S_ERR_TOKEN:
                    // EOF errors (e.g. an open comment) have no current YYText().
                    result.lexeme = token.value.as<std::string>();
                    result.sval   = result.lexeme;
                    result.type   = Token::TokenType::T_STRING;
                    break;
                case kind::S_IDENT:
                case kind::S_SLASH_COMMENT:
                case kind::S_STR_CONST:
                    result.sval = token.value.as<std::string>();
                    result.type = Token::TokenType::T_STRING;
                    break;
                default: result.type = Token::TokenType::T_NONE; break;
            }

            tokens.push_back(result);
        }

        return tokens;
    }

    AST::Root* Parser::parseAST_impl()
    {
        if (_parser.parse() != 0 || hasLexicalErrors())
        {
            delete ast;
            ast = nullptr;
        }
        return ast;
    }
}  // namespace FE
