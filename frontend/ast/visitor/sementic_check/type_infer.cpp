#include <frontend/ast/visitor/sementic_check/ast_checker.h>
#include <cstdint>

namespace FE::AST
{
    namespace
    {
        ExprValue typed(Type* type)
        {
            ExprValue result;
            result.value.type = type;
            return result;
        }

        template <class T>
        ExprValue compare(T a, T b, Operator op)
        {
            switch (op)
            {
                case Operator::EQ: return {VarValue(a == b), true};
                case Operator::NEQ: return {VarValue(a != b), true};
                case Operator::GT: return {VarValue(a > b), true};
                case Operator::GE: return {VarValue(a >= b), true};
                case Operator::LT: return {VarValue(a < b), true};
                case Operator::LE: return {VarValue(a <= b), true};
                default: return {};
            }
        }

        bool comparison(Operator op)
        {
            return op == Operator::EQ || op == Operator::NEQ || op == Operator::GT || op == Operator::GE ||
                   op == Operator::LT || op == Operator::LE;
        }
    }

    ExprValue ASTChecker::typeInfer(const ExprValue& operand, Operator op)
    {
        if (op == Operator::NOT)
            return operand.isConstexpr ? ExprValue(VarValue(!operand.getBool()), true) : typed(boolType);
        const bool fp = operand.value.type == floatType;
        if (!operand.isConstexpr) return typed(fp ? floatType : intType);
        if (fp) return {op == Operator::SUB ? -operand.getFloat() : operand.getFloat(), true};
        const auto v = static_cast<uint32_t>(operand.getInt());
        return {static_cast<int>(op == Operator::SUB ? 0u - v : v), true};
    }

    ExprValue ASTChecker::typeInfer(
        const ExprValue& lhs, const ExprValue& rhs, Operator op, const Node& node, bool& failed)
    {
        if (op == Operator::AND || op == Operator::OR)
        {
            if (!lhs.isConstexpr) return typed(boolType);
            bool left = lhs.getBool();
            if (left == (op == Operator::OR)) return {VarValue(left), true};
            return rhs.isConstexpr ? ExprValue(VarValue(rhs.getBool()), true) : typed(boolType);
        }

        bool fp = lhs.value.type == floatType || rhs.value.type == floatType;
        if (!fp && (op == Operator::DIV || op == Operator::MOD) && rhs.isConstexpr && rhs.getInt() == 0)
        {
            error(node, toString(op) + " operation with zero divisor");
            failed = true;
            return {};
        }

        if (!lhs.isConstexpr || !rhs.isConstexpr) return typed(comparison(op) ? boolType : fp ? floatType : intType);
        if (fp)
        {
            const float a = lhs.getFloat(), b = rhs.getFloat();
            switch (op)
            {
                case Operator::ADD: return {a + b, true};
                case Operator::SUB: return {a - b, true};
                case Operator::MUL: return {a * b, true};
                case Operator::DIV: return {a / b, true};
                default: return compare(a, b, op);
            }
        }

        const int  a = lhs.getInt(), b = rhs.getInt();
        const auto ua = static_cast<uint32_t>(a), ub = static_cast<uint32_t>(b);
        switch (op)
        {
            case Operator::ADD: return {static_cast<int>(ua + ub), true};
            case Operator::SUB: return {static_cast<int>(ua - ub), true};
            case Operator::MUL: return {static_cast<int>(ua * ub), true};
            case Operator::DIV: return {static_cast<int>(static_cast<int64_t>(a) / b), true};
            case Operator::MOD: return {static_cast<int>(static_cast<int64_t>(a) % b), true};
            default: return compare(a, b, op);
        }
    }
}  // namespace FE::AST
