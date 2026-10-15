#include <frontend/parser/parser.h>
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>

/** Verify independent scanner cursors, error flags and exact binary32 rounding. */
int main()
{
    std::istringstream firstInput("\talpha\r\n/* one\n two */123");
    std::istringstream secondInput("float 0");
    std::ostringstream diagnostics;
    FE::Parser         first(&firstInput, &diagnostics);
    FE::Parser         second(&secondInput, &diagnostics);
    const auto         firstTokens  = first.parseTokens();
    const auto         secondTokens = second.parseTokens();
    if (firstTokens.size() != 2 || firstTokens[0].line_number != 1 || firstTokens[0].column_number != 4 ||
        firstTokens[1].line_number != 3 || firstTokens[1].column_number != 7)
        return 1;
    if (secondTokens.size() != 2 || secondTokens[0].line_number != 1 || secondTokens[0].column_number != 0) return 2;

    std::istringstream badInput("/* open");
    FE::Parser         bad(&badInput, &diagnostics);
    const auto         badTokens = bad.parseTokens();
    if (!bad.hasLexicalErrors() || badTokens.size() != 1 || badTokens[0].lexeme != "/*" || first.hasLexicalErrors() ||
        second.hasLexicalErrors())
        return 3;

    // This decimal lies just above a binary32 midpoint. Parsing to double first
    // and then narrowing can round it incorrectly to 0.5 rather than its successor.
    std::istringstream numericInput("0.5000000298023224 0x1p-149 0x1.fffffep127");
    FE::Parser         numeric(&numericInput, &diagnostics);
    const auto         numbers = numeric.parseTokens();
    if (numeric.hasLexicalErrors() || numbers.size() != 3) return 4;
    for (const auto& token : numbers)
        if (token.type != FE::Token::TokenType::T_FLOAT) return 5;
    if (numbers[0].fval != std::nextafter(0.5f, 1.0f) || numbers[1].fval != std::numeric_limits<float>::denorm_min() ||
        numbers[2].fval != std::numeric_limits<float>::max())
        return 5;
    std::cout << "Independent scanner state and exact binary32 conversion passed\n";
    return 0;
}
