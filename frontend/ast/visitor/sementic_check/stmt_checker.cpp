#include <frontend/ast/visitor/sementic_check/ast_checker.h>

namespace FE::AST
{
    bool ASTChecker::condition(ExprNode* node)
    {
        return check(node) &&
               (!node || isNumeric(node->attr.val.value.type) || error(*node, "condition must be a numeric scalar"));
    }

    bool ASTChecker::visit(ExprStmt& node) { return check(node.expr); }

    bool ASTChecker::visit(FuncDeclStmt& node)
    {
        if (symTable.containsLocal(node.entry) || !funcDecls.emplace(node.entry, &node).second)
            return error(node, "redefinition of " + node.entry->getName());
        bool ok = true;
        if (node.entry == Entry::getEntry("main") &&
            (node.retType != intType || (node.params && !node.params->empty())))
            ok = error(node, "main must have signature int main()");
        currentFunction = &node;
        node.returnShape.clear();
        node.returnExtentParameter = -1;
        recursiveTensorCall        = false;
        inferReturnShape           = true;
        symTable.enterScope();
        if (node.params)
        {
            parameterIndex = 0;
            for (auto* param : *node.params)
            {
                ok &= check(param);
                ++parameterIndex;
            }
        }
        const size_t beforeBody = errors.size();
        bool         bodyOK     = check(node.body);
        if (recursiveTensorCall && !node.returnShape.empty())
        {
            // Recheck calls against the settled signature, not a provisional extent.
            inferReturnShape = false;
            errors.resize(beforeBody);
            bodyOK = check(node.body);
        }
        symTable.exitScope();
        currentFunction = nullptr;
        if (isTensor(node.retType) && node.returnShape.empty())
            ok &= error(node, "cannot infer tensor return shape from any return expression");
        return ok && bodyOK;
    }

    bool ASTChecker::visit(VarDeclStmt& node) { return check(node.decl); }

    bool ASTChecker::visit(BlockStmt& node)
    {
        symTable.enterScope();
        bool ok = true;
        for (auto* stmt : *node.stmts) ok &= check(stmt);
        symTable.exitScope();
        return ok;
    }

    bool ASTChecker::visit(ReturnStmt& node)
    {
        Type* target = currentFunction->retType;
        if (!node.retExpr) return target == voidType || error(node, "return requires a value");
        if (!check(node.retExpr)) return false;
        Type* source = node.retExpr->attr.val.value.type;
        if (!isTensor(target))
            return (isNumeric(target) && isNumeric(source)) || error(node, "incompatible return type");
        if (source != target) return error(node, "incompatible tensor return element type");
        auto&       result = currentFunction->returnShape;
        const auto& shape  = node.retExpr->shape;
        if (!result.empty() && !compatibleShape(result, shape)) return error(node, "incompatible tensor return shape");
        if (inferReturnShape && (result.empty() || (result.front() < 0 && shape.front() > 0)))
        {
            result                                 = shape;
            currentFunction->returnExtentParameter = node.retExpr->extentParameter;
        }
        return true;
    }

    bool ASTChecker::visit(WhileStmt& node)
    {
        bool ok = condition(node.cond);
        ++loopDepth;
        ok &= check(node.body);
        --loopDepth;
        return ok;
    }

    bool ASTChecker::visit(IfStmt& node)
    {
        bool ok = condition(node.cond);
        ok &= check(node.thenStmt);
        ok &= check(node.elseStmt);
        return ok;
    }

    bool ASTChecker::visit(BreakStmt& node) { return loopDepth || error(node, "break outside loop"); }
    bool ASTChecker::visit(ContinueStmt& node) { return loopDepth || error(node, "continue outside loop"); }

    bool ASTChecker::visit(ForStmt& node)
    {
        symTable.enterScope();
        bool ok = check(node.init);
        ok &= condition(node.cond);
        ok &= check(node.step);
        ++loopDepth;
        ok &= check(node.body);
        --loopDepth;
        symTable.exitScope();
        return ok;
    }
}  // namespace FE::AST
