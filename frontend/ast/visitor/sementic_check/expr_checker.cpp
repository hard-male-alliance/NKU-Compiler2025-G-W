#include <frontend/ast/visitor/sementic_check/ast_checker.h>
#include <limits>

namespace FE::AST
{
    bool ASTChecker::visit(LeftValExpr& node)
    {
        const auto* symbol = symTable.getSymbol(node.entry);
        if (!symbol) return error(node, "undeclared variable " + node.entry->getName());
        const auto& shape   = symbol->arrayDims;
        size_t      indexed = node.indices ? node.indices->size() : 0;
        if (indexed > shape.size() || (isTensor(symbol->type) && indexed && indexed != shape.size()))
            return error(node, "incorrect number of subscripts for " + node.entry->getName());
        node.writable = !symbol->isConstDecl;
        node.shape.assign(shape.begin() + indexed, shape.end());
        node.extentParameter = indexed == 0 ? symbol->extentParameter : -1;
        node.attr.val        = {};
        auto& value          = node.attr.val;
        if (node.shape.empty())
            value.value.type = elementType(symbol->type);
        else if (isTensor(symbol->type))
            value.value.type = symbol->type;
        else
            value.value.type = TypeFactory::getPtrType(symbol->type);

        bool   fold   = symbol->isConstDecl && node.shape.empty();
        size_t offset = 0;
        for (size_t i = 0; i < indexed; ++i)
        {
            auto* index = (*node.indices)[i];
            if (!check(index)) return false;
            const auto& sub = index->attr.val;
            Type*       t   = sub.value.type;
            if (t != intType && t != llType && t != boolType) return error(*index, "subscript must be integral");
            fold = fold && sub.isConstexpr && sub.getLL() >= 0 && sub.getLL() < shape[i];
            if (fold) offset = offset * static_cast<size_t>(shape[i]) + sub.getInt();
        }
        if (fold)
        {
            value.isConstexpr = true;
            if (offset < symbol->initList.size())
                value.value = symbol->initList[offset];
            else
                value.value = value.value.type == floatType ? VarValue(0.0f) : VarValue(0);
        }
        return true;
    }

    bool ASTChecker::visit(LiteralExpr& node)
    {
        node.attr.val = ExprValue(node.literal, true);
        return true;
    }

    bool ASTChecker::visit(UnaryExpr& node)
    {
        if (!check(node.expr)) return false;
        Type* type = node.expr->attr.val.value.type;
        if (isTensor(type) && node.op != Operator::NOT)
        {
            node.attr.val.value.type = type;
            node.shape               = node.expr->shape;
            node.extentParameter     = node.expr->extentParameter;
            return true;
        }
        if (!isNumeric(type)) return error(node, "invalid unary operand for " + toString(node.op));
        node.attr.val = typeInfer(node.expr->attr.val, node.op);
        return true;
    }

    std::optional<VarValue> ASTChecker::tensorConstant(ExprNode& expr, int index)
    {
        if (!isTensor(expr.attr.val.value.type))
        {
            if (expr.attr.val.isConstexpr) return expr.attr.val.value;
            return std::nullopt;
        }
        if (auto* name = dynamic_cast<LeftValExpr*>(&expr))
        {
            const auto* var = symTable.getSymbol(name->entry);
            if (!var->isConstDecl) return std::nullopt;
            if (static_cast<size_t>(index) < var->initList.size()) return var->initList[index];
            return elementType(var->type) == floatType ? VarValue(0.0f) : VarValue(0);
        }
        if (auto* unary = dynamic_cast<UnaryExpr*>(&expr))
        {
            auto value = tensorConstant(*unary->expr, index);
            if (!value) return std::nullopt;
            auto folded = typeInfer(ExprValue(*value, true), unary->op);
            return folded.value;
        }
        auto* bin = dynamic_cast<BinaryExpr*>(&expr);
        if (!bin || bin->op == Operator::ASSIGN) return std::nullopt;
        auto combine = [&](Operator op, int lhsIndex, int rhsIndex) -> std::optional<VarValue> {
            auto a = tensorConstant(*bin->lhs, lhsIndex);
            auto b = tensorConstant(*bin->rhs, rhsIndex);
            if (!a || !b) return std::nullopt;
            bool failed = false;
            auto folded = typeInfer(ExprValue(*a, true), ExprValue(*b, true), op, *bin, failed);
            return failed ? std::nullopt : std::optional<VarValue>(folded.value);
        };
        if (bin->op != Operator::MATMUL) return combine(bin->op, index, index);
        const int inner = bin->lhs->shape[1];
        const int cols  = bin->shape[1];
        VarValue  total = elementType(expr.attr.val.value.type) == floatType ? VarValue(0.0f) : VarValue(0);
        for (int k = 0; k < inner; ++k)
        {
            auto product = combine(Operator::MUL, (index / cols) * inner + k, k * cols + index % cols);
            if (!product) return std::nullopt;
            bool failed = false;
            total = typeInfer(ExprValue(total, true), ExprValue(*product, true), Operator::ADD, *bin, failed).value;
        }
        return total;
    }

    bool ASTChecker::tensorBinary(BinaryExpr& node, Type* lhs, Type* rhs)
    {
        bool        lt = isTensor(lhs), rt = isTensor(rhs);
        Type*       type       = lt ? lhs : rhs;
        const auto& a          = node.lhs->shape;
        const auto& b          = node.rhs->shape;
        auto        scalarBase = [](Type* t) { return t == llType ? intType : t; };
        if ((lt ? elementType(lhs) : scalarBase(lhs)) != (rt ? elementType(rhs) : scalarBase(rhs)))
            return error(node, "tensor operands require the same element type");
        switch (node.op)
        {
            case Operator::MATMUL:
                if (!lt || !rt || a.size() != 2 || b.size() != 2 || (b[0] > 0 && a[1] != b[0]))
                    return error(node, "@ requires MxN and NxP tensors");
                if (a[0] > 0 && a[0] > std::numeric_limits<int>::max() / b[1])
                    return error(node, "matrix product has too many elements");
                node.shape           = {a[0], b[1]};
                node.extentParameter = node.lhs->extentParameter;
                break;
            case Operator::MOD:
                if (elementType(type) != intType) return error(node, "tensor % requires integer elements");
                [[fallthrough]];
            case Operator::DIV:
                if (!rt && elementType(type) == intType && node.rhs->attr.val.isConstexpr &&
                    node.rhs->attr.val.getInt() == 0)
                    return error(node, "tensor operation with zero divisor");
                [[fallthrough]];
            case Operator::ADD:
            case Operator::SUB:
            case Operator::MUL:
                if (lt && rt && !compatibleShape(a, b)) return error(node, "tensor operand shapes do not match");
                // A known first extent also constrains a dynamic operand at this operation.
                if (!lt || (rt && a.front() < 0))
                {
                    node.shape           = b;
                    node.extentParameter = node.rhs->extentParameter;
                }
                else
                {
                    node.shape           = a;
                    node.extentParameter = node.lhs->extentParameter;
                }
                break;
            default: return error(node, "operator " + toString(node.op) + " does not accept tensors");
        }
        node.attr.val.value.type = type;
        return true;
    }

    bool ASTChecker::visit(BinaryExpr& node)
    {
        bool ok = check(node.lhs);
        ok &= check(node.rhs);
        if (!ok) return false;
        Type* lhs = node.lhs->attr.val.value.type;
        Type* rhs = node.rhs->attr.val.value.type;
        if (node.op == Operator::ASSIGN)
        {
            auto* dest = dynamic_cast<LeftValExpr*>(node.lhs);
            if (!dest || !dest->writable) return error(node, "left operand of assignment is not writable");
            if (isTensor(lhs) && lhs == rhs && compatibleShape(dest->shape, node.rhs->shape))
            {
                node.shape           = dest->shape;
                node.extentParameter = dest->extentParameter;
            }
            else if (!isNumeric(lhs) || !isNumeric(rhs))
                return error(node, "assignment requires scalars or identical tensor types and shapes");
            node.attr.val.value.type = lhs;
            return true;
        }
        if (isTensor(lhs) || isTensor(rhs)) return tensorBinary(node, lhs, rhs);
        if (!isNumeric(lhs) || !isNumeric(rhs)) return error(node, "operator requires numeric operands");
        if (node.op == Operator::MATMUL) return error(node, "@ requires two tensors");
        if (node.op == Operator::MOD && (lhs == floatType || rhs == floatType))
            return error(node, "% requires integers");
        bool failed   = false;
        node.attr.val = typeInfer(node.lhs->attr.val, node.rhs->attr.val, node.op, node, failed);
        return !failed;
    }

    bool ASTChecker::visit(CallExpr& node)
    {
        auto found = funcDecls.find(node.func);
        if (found == funcDecls.end() || symTable.getSymbol(node.func))
            return error(node, "undeclared function " + node.func->getName());
        const auto* fn    = found->second;
        size_t      count = node.args ? node.args->size() : 0;
        if (count != (fn->params ? fn->params->size() : 0))
            return error(node, "incorrect argument count for " + node.func->getName());
        bool ok = true;
        for (size_t i = 0; i < count; ++i)
        {
            auto* arg   = (*node.args)[i];
            auto* param = (*fn->params)[i];
            if (!check(arg))
            {
                ok = false;
                continue;
            }
            Type* source   = arg->attr.val.value.type;
            Type* expected = param->attr.val.value.type;
            bool  match    = false;
            if (isTensor(expected))
                match = source == expected && compatibleShape(param->binding.arrayDims, arg->shape);
            else if (expected->getTypeGroup() == TypeGroup::POINTER)
            {
                const auto& dims = param->binding.arrayDims;
                // SysY I/O takes a flat buffer; the course fixtures pass entire multidimensional arrays.
                match = source == expected && (dims.empty() || compatibleShape(dims, arg->shape));
            }
            else
                match = isNumeric(expected) && isNumeric(source);
            if (!match) ok &= error(*arg, "argument " + std::to_string(i + 1) + " has incompatible type or shape");
        }
        node.attr.val.value.type = fn->retType;
        node.shape               = fn->returnShape;
        node.extentParameter     = -1;
        if (!isTensor(fn->retType)) return ok;
        if (fn == currentFunction) recursiveTensorCall = true;
        if (node.shape.empty()) return error(node, "cannot infer recursive tensor return shape");
        if (node.shape.front() < 0 && ok)
        {
            const auto* actual   = (*node.args)[fn->returnExtentParameter];
            node.shape.front()   = actual->shape.front();
            node.extentParameter = actual->extentParameter;
            if (node.shape.front() > 0)
            {
                int capacity = std::numeric_limits<int>::max() / node.shape.front();
                for (size_t i = 1; i < node.shape.size(); ++i)
                {
                    if (node.shape[i] > capacity) return error(node, "tensor result has too many elements");
                    capacity /= node.shape[i];
                }
            }
        }
        return ok;
    }

    bool ASTChecker::visit(CommaExpr& node)
    {
        bool ok = true;
        for (auto* expr : *node.exprs) ok &= check(expr);
        node.attr.val             = node.exprs->back()->attr.val;
        node.attr.val.isConstexpr = false;
        node.shape                = node.exprs->back()->shape;
        node.extentParameter      = node.exprs->back()->extentParameter;
        return ok;
    }
}  // namespace FE::AST
