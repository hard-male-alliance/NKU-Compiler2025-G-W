#ifndef __FRONTEND_PARSER_SCANNER_H__
#define __FRONTEND_PARSER_SCANNER_H__

#ifndef yyFlexLexerOnce
#undef yyFlexLexer
#define yyFlexLexer Yacc_FlexLexer
#include <FlexLexer.h>
#endif

#undef YY_DECL
#define YY_DECL FE::YaccParser::symbol_type FE::Scanner::nextToken()

#include <frontend/parser/yacc.h>
#include <string_view>

namespace FE
{
    class Parser;

    class Scanner : public yyFlexLexer
    {
      private:
        /// Parser receiving lexical diagnostics; it outlives this scanner.
        Parser& _parser;
        /// Per-input cursor; Bison uses one-based columns internally.
        location loc_;
        /// Opening delimiter used to locate an unterminated block comment.
        location commentStart_;

        /// Advance to the next four-column tab stop, including inside comments.
        void advanceTab();
        /// Convert an unsigned source spelling without overflowing signed arithmetic.
        YaccParser::symbol_type integerToken(std::string_view text);
        /// Convert a suffix-free decimal/hexadecimal literal directly to binary32.
        YaccParser::symbol_type floatToken(std::string_view text, bool hexadecimal);
        /// Validate C escapes while retaining the raw, unquoted runtime string payload.
        YaccParser::symbol_type stringToken(std::string_view text);
        /// Emit one diagnostic and return an error token without echoing source bytes.
        YaccParser::symbol_type errorToken(const location& where, std::string_view text, const std::string& message);

      public:
        Scanner(Parser& parser) : _parser(parser) {}
        virtual ~Scanner() {}

        virtual YaccParser::symbol_type nextToken();
    };
}  // namespace FE

#endif  // __FRONTEND_PARSER_SCANNER_H__
