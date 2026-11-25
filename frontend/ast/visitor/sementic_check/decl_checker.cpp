#include <frontend/ast/visitor/sementic_check/ast_checker.h>
#include <algorithm>
#include <climits>
#include <numeric>

namespace FE::AST
{
    bool ASTChecker::visit(Initializer& node)
    {
        bool ok   = check(node.init_val);
        node.attr = node.init_val->attr;
        return ok;
    }

    bool ASTChecker::visit(InitializerList& node)
    {
        bool ok = true;
        for (auto* init : *node.init_list) ok &= check(init);
        return ok;
    }

    bool ASTChecker::dimensions(
        const std::vector<ExprNode*>* exprs, std::vector<int>& shape, bool parameter, bool tensor)
    {
        shape.clear();
        if (!exprs) return true;
        int count = 1;
        for (auto* expr : *exprs)
        {
            auto* literal = dynamic_cast<LiteralExpr*>(expr);
            if (parameter && shape.empty() && literal && literal->literal.type == intType &&
                literal->literal.getInt() == -1)
            {
                shape.push_back(-1);
                continue;
            }
            if (!check(expr)) return false;
            const auto& value = expr->attr.val;
            if (!value.isConstexpr || (value.value.type != intType && value.value.type != llType) ||
                value.getLL() < (tensor ? 1 : 0))
                return error(*expr,
                    tensor ? "tensor dimension must be a positive integer constant"
                           : "array dimension must be a nonnegative integer constant");
            if (value.getLL() > INT_MAX / count) return error(*expr, "array or tensor has too many elements");
            shape.push_back(static_cast<int>(value.getLL()));
            count *= std::max(1, shape.back());
        }
        return true;
    }

    bool ASTChecker::flatten(InitDecl& init, const std::vector<int>& spans, size_t level, int offset,
        std::vector<std::pair<int, ExprNode*>>& values, bool tensor)
    {
        if (init.singleInit)
        {
            values.emplace_back(offset, static_cast<Initializer&>(init).init_val);
            return check(&init);
        }
        int  used = 0;
        bool ok   = true;
        for (auto* child : *static_cast<InitializerList&>(init).init_list)
        {
            if (used == spans[level]) return error(*child, "too many initializers for this subobject");
            size_t next = spans.size() - 1;
            if (!child->singleInit && level < next)
            {
                next = level + 1;
                while (used % spans[next] != 0) ++next;
            }
            if (tensor && !child->singleInit && next == spans.size() - 1)
                return error(*child, "tensor initializer braces do not match the shape");
            ok &= flatten(*child, spans, next, offset + used, values, tensor);
            used += spans[next];
        }
        return ok;
    }

    bool ASTChecker::visit(ParamDeclarator& node)
    {
        node.binding = VarAttr(node.type, false, symTable.getScopeDepth());
        if (node.type == voidType) return error(node, "parameter cannot have void type");
        if (symTable.containsLocal(node.entry)) return error(node, "duplicate parameter " + node.entry->getName());
        if (!dimensions(node.dims, node.binding.arrayDims, true, isTensor(node.type))) return false;
        auto& dims = node.binding.arrayDims;
        if (isTensor(node.type))
        {
            if (dims.empty() || dims.front() != -1)
                return error(node, "tensor parameter must have an empty first dimension []");
            node.binding.extentParameter = parameterIndex;
        }
        if (!dims.empty()) dims.front() = -1;  // The legacy sized array-parameter spelling still decays.
        node.attr.val.value.type = !isTensor(node.type) && node.dims ? TypeFactory::getPtrType(node.type) : node.type;
        symTable.addSymbol(node.entry, node.binding);
        return true;
    }

    bool ASTChecker::visit(VarDeclarator& node)
    {
        auto& name = static_cast<LeftValExpr&>(*node.lval);
        auto& attr = node.binding;
        attr       = VarAttr(currentDeclaration->type, currentDeclaration->isConstDecl, symTable.getScopeDepth());
        node.initializers.clear();
        const bool global = symTable.isGlobalScope();
        if (symTable.containsLocal(name.entry) || (global && funcDecls.find(name.entry) != funcDecls.end()))
            return error(name, "redefinition of " + name.entry->getName());
        const bool tensor = isTensor(attr.type);
        if (!dimensions(name.indices, attr.arrayDims, false, tensor)) return false;
        if (tensor && attr.arrayDims.empty()) return error(name, "tensor declaration requires dimensions");
        name.attr.val.value.type = attr.type;
        name.shape               = attr.arrayDims;
        if (!node.init)
        {
            if (attr.isConstDecl) return error(name, "constant requires an initializer");
        }
        else if (!attr.arrayDims.empty() && node.init->singleInit)
        {
            auto* expr = static_cast<Initializer&>(*node.init).init_val;
            if (!check(expr)) return false;
            if (!tensor || expr->attr.val.value.type != attr.type || !compatibleShape(attr.arrayDims, expr->shape))
                return error(*expr, "initializer must have the same tensor element type and shape");
            if (global || attr.isConstDecl)
            {
                int count = std::accumulate(attr.arrayDims.begin(), attr.arrayDims.end(), 1, std::multiplies<int>());
                for (int i = 0; i < count; ++i)
                {
                    const auto value = tensorConstant(*expr, i);
                    if (!value) return error(*expr, "initializer must be a constant tensor");
                    attr.initList.push_back(*value);
                }
            }
        }
        else
        {
            std::vector<int> spans(attr.arrayDims.size() + 1, 1);
            for (size_t i = attr.arrayDims.size(); i > 0; --i) spans[i - 1] = spans[i] * attr.arrayDims[i - 1];
            if (!flatten(*node.init, spans, 0, 0, node.initializers, tensor)) return false;
            Type* target = elementType(attr.type);
            for (auto [offset, expr] : node.initializers)
            {
                const auto& value = expr->attr.val;
                if (!isNumeric(value.value.type)) return error(*expr, "initializer must be a numeric scalar");
                if (global || attr.isConstDecl)
                {
                    if (!value.isConstexpr) return error(*expr, "initializer must be a constant expression");
                    if (target == intType && value.value.type == floatType)
                    {
                        const double scalar = value.getFloat();
                        if (!(scalar >= INT_MIN && scalar < static_cast<double>(INT_MAX) + 1))
                            return error(*expr, "constant initializer is outside the int range");
                    }
                    VarValue zero = target == floatType ? VarValue(0.0f) : VarValue(0);
                    attr.initList.resize(static_cast<size_t>(offset) + 1, zero);
                    attr.initList[offset] = target == floatType ? VarValue(value.getFloat()) : VarValue(value.getInt());
                }
            }
        }
        symTable.addSymbol(name.entry, attr);
        if (global) glbSymbols.emplace(name.entry, attr);
        return true;
    }

    bool ASTChecker::visit(VarDeclaration& node)
    {
        if (node.type == voidType) return error(node, "variable cannot have void type");
        currentDeclaration = &node;
        bool ok            = true;
        for (auto* decl : *node.decls) ok &= check(decl);
        currentDeclaration = nullptr;
        return ok;
    }
}  // namespace FE::AST
