%{
    #include <frontend/parser/yacc.h>
    #include <frontend/parser/scanner.h>
    #include <frontend/parser/parser.h>
    #include <charconv>
    #include <limits>
    #include <string>
    #include <string_view>

    using namespace FE;

    // Token payloads must represent SysY's fixed-width numeric domains exactly.
    static_assert(std::numeric_limits<int>::digits == 31, "SysY requires 32-bit int tokens");
    static_assert(std::numeric_limits<long long>::digits == 63, "Wide token transport requires 64 bits");
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559 &&
                  std::numeric_limits<float>::digits == 24,
                  "SysY requires IEEE-754 binary32 floats");

    // Every rule starts a new span; newline/tab actions adjust its end explicitly.
    #define YY_USER_ACTION loc_.step(); loc_.columns(yyleng);
    #define RETT(x) return YaccParser::make_##x(loc_);
    #define LEXEME std::string_view(yytext, static_cast<size_t>(yyleng))
%}

%option c++ noyywrap nodefault
%option yyclass="Scanner"
%option prefix="Yacc_"
%x BLOCK_COMMENT

DIGIT       [0-9]
HEX         [0-9a-fA-F]
NEWLINE     (\r\n|\r|\n)
DEC_EXP     [eE][+-]?{DIGIT}+
BIN_EXP     [pP][+-]?{DIGIT}+
DEC_FRAC    ({DIGIT}*\.{DIGIT}+|{DIGIT}+\.)
HEX_FRAC    ({HEX}*\.{HEX}+|{HEX}+\.)
DEC_FLOAT   ({DEC_FRAC}{DEC_EXP}?|{DIGIT}+{DEC_EXP})
HEX_FLOAT   0[xX]({HEX_FRAC}|{HEX}+){BIN_EXP}
INTEGER     (0[xX]{HEX}+|0[0-7]*|[1-9]{DIGIT}*)
IDENTIFIER  [A-Za-z_][A-Za-z0-9_]*
STRING      \"([^\"\\\r\n]|\\[^\r\n])*\"
OPEN_STRING \"([^\"\\\r\n]|\\[^\r\n])*\\?

%%

{NEWLINE}              { loc_.lines(1); }
[ \f\v]+               { /* Whitespace separates tokens. */ }
\t                     { advanceTab(); }

"//"[^\r\n]*           { /* Leave the line ending to the newline rule. */ }
"/*"                   { commentStart_ = loc_; BEGIN(BLOCK_COMMENT); }
<BLOCK_COMMENT>"*/"    { BEGIN(INITIAL); }
<BLOCK_COMMENT>[^*\r\n\t]+ { /* Bulk-skip ordinary comment text. */ }
<BLOCK_COMMENT>"*"     { /* The first following star/slash pair closes the comment. */ }
<BLOCK_COMMENT>\t      { advanceTab(); }
<BLOCK_COMMENT>{NEWLINE} { loc_.lines(1); }
<BLOCK_COMMENT><<EOF>> {
    BEGIN(INITIAL);
    return errorToken(commentStart_, "/*", "Unterminated block comment");
}

"int"                  { RETT(INT) }
"float"                { RETT(FLOAT) }
"void"                 { RETT(VOID) }
"const"                { RETT(CONST) }
"if"                   { RETT(IF) }
"else"                 { RETT(ELSE) }
"while"                { RETT(WHILE) }
"continue"             { RETT(CONTINUE) }
"break"                { RETT(BREAK) }
"return"               { RETT(RETURN) }

    /* Preserve the pre-existing framework keyword extensions, without adding grammar. */
"for"                  { RETT(FOR) }
"switch"               { RETT(SWITCH) }
"case"                 { RETT(CASE) }
"goto"                 { RETT(GOTO) }
"do"                   { RETT(DO) }

{IDENTIFIER}           { return YaccParser::make_IDENT(std::string(LEXEME), loc_); }
{HEX_FLOAT}            { return floatToken(LEXEME, true); }
{DEC_FLOAT}            { return floatToken(LEXEME, false); }
{INTEGER}              { return integerToken(LEXEME); }

    /* Longest match rejects bad bases, suffixes, exponents and joined numeric text.
       Keep exponent signs specific to their base: 0x1e+2 is an integer plus 2. */
({DIGIT}|\.{DIGIT})[A-Za-z0-9_.]* {
    return errorToken(loc_, LEXEME, "Malformed numeric literal");
}
({DIGIT}+(\.{DIGIT}*)?|\.{DIGIT}+)[eE][+-]?[A-Za-z0-9_.]* {
    return errorToken(loc_, LEXEME, "Malformed decimal exponent");
}
0[xX][A-Za-z0-9_.]*[pP][+-]?[A-Za-z0-9_.]* {
    return errorToken(loc_, LEXEME, "Malformed hexadecimal exponent");
}

{STRING}               { return stringToken(LEXEME); }
{OPEN_STRING}          { return errorToken(loc_, LEXEME, "Unterminated string literal"); }

"+"                    { RETT(PLUS) }
"-"                    { RETT(MINUS) }
"*"                    { RETT(STAR) }
"/"                    { RETT(SLASH) }
"%"                    { RETT(MOD) }
"="                    { RETT(ASSIGN) }
"=="                   { RETT(EQ) }
"!="                   { RETT(NE) }
"<"                    { RETT(LT) }
">"                    { RETT(GT) }
"<="                   { RETT(LE) }
">="                   { RETT(GE) }
"!"                    { RETT(NOT) }
"&&"                   { RETT(AND) }
"||"                   { RETT(OR) }
";"                    { RETT(SEMICOLON) }
","                    { RETT(COMMA) }
"("                    { RETT(LPAREN) }
")"                    { RETT(RPAREN) }
"["                    { RETT(LBRACKET) }
"]"                    { RETT(RBRACKET) }
"{"                    { RETT(LBRACE) }
"}"                    { RETT(RBRACE) }

.                      { return errorToken(loc_, LEXEME, "Unexpected character"); }
<<EOF>>                { loc_.step(); RETT(END) }

%%

void Scanner::advanceTab()
{
    constexpr int tabWidth = 4;
    loc_.end.column = ((loc_.begin.column - 1) / tabWidth + 1) * tabWidth + 1;
}

YaccParser::symbol_type Scanner::errorToken(
    const location& where, std::string_view text, const std::string& message)
{
    _parser.reportError(where, message);
    return YaccParser::make_ERR_TOKEN(std::string(text), where);
}

YaccParser::symbol_type Scanner::integerToken(std::string_view text)
{
    int base = 10;
    std::string_view digits = text;
    if (text.size() > 1 && text[0] == '0')
    {
        base = 8;
        if (text[1] == 'x' || text[1] == 'X')
        {
            base = 16;
            digits.remove_prefix(2);
        }
    }

    long long value = 0;
    const auto result = std::from_chars(digits.data(), digits.data() + digits.size(), value, base);
    if (result.ec != std::errc{} || result.ptr != digits.data() + digits.size())
        return errorToken(loc_, text, "Integer literal exceeds signed 64-bit token range");

    // Wide tokens transport magnitudes such as 2147483648 for a later unary minus.
    // They do not add a 64-bit source type or perform semantic narrowing here.
    if (value > std::numeric_limits<int>::max()) return YaccParser::make_LL_CONST(value, loc_);
    return YaccParser::make_INT_CONST(static_cast<int>(value), loc_);
}

YaccParser::symbol_type Scanner::floatToken(std::string_view text, bool hexadecimal)
{
    std::string_view digits = text;
    if (hexadecimal) digits.remove_prefix(2); // from_chars(hex) does not consume 0x.
    float value = 0.0f;
    const auto format = hexadecimal ? std::chars_format::hex : std::chars_format::general;
    const auto result = std::from_chars(digits.data(), digits.data() + digits.size(), value, format);
    if (result.ec != std::errc{} || result.ptr != digits.data() + digits.size())
        return errorToken(loc_, text, "Floating literal is outside the representable float range");
    return YaccParser::make_FLOAT_CONST(value, loc_);
}

YaccParser::symbol_type Scanner::stringToken(std::string_view text)
{
    const auto isHex = [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    };
    const std::string_view value = text.substr(1, text.size() - 2);
    if (value.find('\0') != std::string_view::npos)
        return errorToken(loc_, text, "NUL byte in string literal");
    for (size_t i = 0; i < value.size(); ++i)
    {
        if (value[i] != '\\') continue;
        const char escaped = value[++i]; // The complete-string rule guarantees a following byte.
        if (std::string_view("abfnrtv\\'\"?").find(escaped) != std::string_view::npos) continue;
        if (escaped >= '0' && escaped <= '7')
        {
            for (int count = 1; count < 3 && i + 1 < value.size() && value[i + 1] >= '0' && value[i + 1] <= '7'; ++count)
                ++i;
            continue;
        }
        if (escaped != 'x' || i + 1 == value.size() || !isHex(value[i + 1]))
            return errorToken(loc_, text, "Invalid string escape sequence");
        while (i + 1 < value.size() && isHex(value[i + 1])) ++i;
    }
    return YaccParser::make_STR_CONST(std::string(value), loc_);
}
