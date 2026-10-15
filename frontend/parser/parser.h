#ifndef __FRONTEND_PARSER_PARSER_H__
#define __FRONTEND_PARSER_PARSER_H__

#include <frontend/iparser.h>
#include <frontend/parser/scanner.h>
#include <frontend/parser/yacc.h>

namespace FE
{
    class Parser : public iParser<Parser>
    {
        friend iParser<Parser>;

      private:
        Scanner    _scanner;
        YaccParser _parser;
        /// Lexical diagnostics are separate from unfinished grammar/semantic checks.
        size_t lexicalErrorCount_ = 0;

      public:
        AST::Root* ast;

      public:
        Parser(std::istream* inStream, std::ostream* outStream)
            : iParser<Parser>(inStream, outStream), _scanner(*this), _parser(_scanner, *this), ast(nullptr)
        {
            _scanner.switch_streams(inStream, outStream);
        }
        ~Parser() {}

        /// Record a lexical failure and print its span through Bison's diagnostic hook.
        void reportError(const location& loc, const std::string& message);
        /// Whether scanning encountered an error; callers must not report success.
        bool hasLexicalErrors() const { return lexicalErrorCount_ != 0; }

      private:
        std::vector<Token> parseTokens_impl();
        AST::Root*         parseAST_impl();
    };
}  // namespace FE

#endif  // __FRONTEND_PARSER_PARSER_H__
