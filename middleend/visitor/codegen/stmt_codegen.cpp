#include <middleend/visitor/codegen/ast_codegen.h>

namespace ME
{
    using namespace FE::AST;

    void ASTCodeGen::visit(ExprStmt& node, Module* module)
    {
        if (!node.expr) return;
        startExpression();
        eval(*node.expr, module);
        finishExpression();
    }

    void ASTCodeGen::visit(FuncDeclStmt& node, Module* module)
    {
        const bool tensor = isTensor(node.retType);
        auto       dt     = tensor ? DataType::VOID : type(node.retType);
        function          = new Function(new FuncDefInst(dt, node.entry->getName()));
        module->functions.push_back(function);
        sourceFunction = &node;
        trapBlock      = nullptr;
        entry = block = newBlock();
        locals.emplace_back();
        if (tensor)
        {
            returnBuffer = {reg(), node.returnShape.front() > 0 ? imm(node.returnShape.front()) : nullptr};
            function->funcDef->argRegs.emplace_back(DataType::PTR, returnBuffer.data);
        }
        if (node.params)
        {
            for (auto* param : *node.params) generate(param, module);
            if (tensor && !returnBuffer.first)
                returnBuffer.first = lookup((*node.params)[node.returnExtentParameter]->entry).value.first;
        }
        auto* body = newBlock();
        jump(body);
        block = body;
        generate(node.body, module);
        locals.pop_back();
        if (!terminated())
        {
            if (tensor) zeroFill(returnBuffer.data, elements(node.returnShape, returnBuffer.first));
            auto* zero = dt == DataType::F32 ? static_cast<Operand*>(getImmeF32Operand(0.0f)) : imm(0);
            emit(new RetInst(dt, dt == DataType::VOID ? nullptr : zero));
        }
    }

    void ASTCodeGen::visit(VarDeclStmt& node, Module* module) { generate(node.decl, module); }

    void ASTCodeGen::visit(BlockStmt& node, Module* module)
    {
        locals.emplace_back();
        for (auto* stmt : *node.stmts)
        {
            if (terminated()) break;
            generate(stmt, module);
        }
        locals.pop_back();
    }

    void ASTCodeGen::visit(ReturnStmt& node, Module* module)
    {
        auto* returnType = sourceFunction->retType;
        if (!node.retExpr)
        {
            emit(new RetInst(DataType::VOID));
            return;
        }
        startExpression();
        if (isTensor(returnType))
        {
            copy(returnBuffer, eval(*node.retExpr, module), sourceFunction->returnShape);
            finishExpression();
            emit(new RetInst(DataType::VOID));
            return;
        }
        auto  dt    = type(returnType);
        auto* value = evalScalar(*node.retExpr, dt, module);
        finishExpression();
        emit(new RetInst(dt, value));
    }

    void ASTCodeGen::visit(IfStmt& node, Module* module)
    {
        startExpression();
        auto* cond = evalScalar(*node.cond, DataType::I1, module);
        finishExpression();
        auto* yes = newBlock();
        auto* no  = node.elseStmt ? newBlock() : nullptr;
        auto* end = newBlock();
        branch(cond, yes, no ? no : end);

        block = yes;
        generate(node.thenStmt, module);
        jump(end);
        if (no)
        {
            block = no;
            generate(node.elseStmt, module);
            jump(end);
        }
        block = end;
    }

    void ASTCodeGen::visit(WhileStmt& node, Module* module)
    {
        auto* test = newBlock();
        auto* body = newBlock();
        auto* end  = newBlock();
        jump(test);
        block = test;
        startExpression();
        auto* cond = evalScalar(*node.cond, DataType::I1, module);
        finishExpression();
        branch(cond, body, end);

        block = body;
        loops.push_back({test, end});
        generate(node.body, module);
        loops.pop_back();
        jump(test);
        block = end;
    }

    void ASTCodeGen::visit(BreakStmt&, Module*) { jump(loops.back().end); }
    void ASTCodeGen::visit(ContinueStmt&, Module*) { jump(loops.back().next); }

    void ASTCodeGen::visit(ForStmt& node, Module* module)
    {
        locals.emplace_back();
        generate(node.init, module);
        auto* test = newBlock();
        auto* body = newBlock();
        auto* step = newBlock();
        auto* end  = newBlock();
        jump(test);
        block = test;
        if (node.cond)
        {
            startExpression();
            auto* cond = evalScalar(*node.cond, DataType::I1, module);
            finishExpression();
            branch(cond, body, end);
        }
        else
            jump(body);

        block = body;
        loops.push_back({step, end});
        generate(node.body, module);
        loops.pop_back();
        jump(step);

        block = step;
        if (node.step)
        {
            startExpression();
            eval(*node.step, module);
            finishExpression();
        }
        jump(test);
        block = end;
        locals.pop_back();
    }
}  // namespace ME
