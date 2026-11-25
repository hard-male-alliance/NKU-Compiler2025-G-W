#include <middleend/visitor/codegen/ast_codegen.h>

namespace ME
{
    using namespace FE::AST;

    void ASTCodeGen::visit(Root& node, Module* module)
    {
        for (const auto& [name, declaration] : functions)
        {
            if (declaration->body) continue;
            std::vector<DataType> params;
            if (declaration->params)
                for (auto* param : *declaration->params) params.push_back(type(param->attr.val.value.type));
            module->funcDecls.push_back(new FuncDeclInst(type(declaration->retType), name->getName(), params));
        }
        for (const auto& [name, binding] : globals)
        {
            DataType dt  = type(elementType(binding.type));
            auto*    ptr = getGlobalOperand(name->getName());
            globalStorage.emplace(name,
                Storage{
                    dt, binding.arrayDims, {ptr, isTensor(binding.type) ? imm(binding.arrayDims.front()) : nullptr}});
            if (binding.arrayDims.empty())
            {
                auto* init = binding.initList.empty() ? nullptr : constant(binding.initList.front());
                module->globalVars.push_back(new GlbVarDeclInst(dt, name->getName(), init));
            }
            else
                module->globalVars.push_back(new GlbVarDeclInst(dt, name->getName(), binding));
        }
        for (auto* stmt : *node.getStmts())
            if (!stmt->isVarDeclStmt()) generate(stmt, module);
        registerIntrinsics(module);
    }

    void ASTCodeGen::registerIntrinsics(Module* module) const
    {
        using D = DataType;
        if (needsZero)
            module->funcDecls.push_back(
                new FuncDeclInst(D::VOID, "llvm.memset.p0.i64", {D::PTR, D::I8, D::I64, D::I1}));
        if (needsCopy)
            module->funcDecls.push_back(
                new FuncDeclInst(D::VOID, "llvm.memmove.p0.p0.i64", {D::PTR, D::PTR, D::I64, D::I1}));
        if (needsTrap) module->funcDecls.push_back(new FuncDeclInst(D::VOID, "llvm.trap"));
        if (needsStackIntrinsics)
        {
            module->funcDecls.push_back(new FuncDeclInst(D::PTR, "llvm.stacksave"));
            module->funcDecls.push_back(new FuncDeclInst(D::VOID, "llvm.stackrestore", {D::PTR}));
        }
    }

    void ASTCodeGen::startExpression()
    {
        ASSERT(!expressionStart);
        expressionStart  = block;
        expressionOffset = block->insts.size();
    }

    void ASTCodeGen::finishExpression()
    {
        if (savedStack) emit(new CallInst(DataType::VOID, "llvm.stackrestore", {{DataType::PTR, savedStack}}));
        savedStack      = nullptr;
        expressionStart = nullptr;
    }

    Operand* ASTCodeGen::allocate(DataType type, Operand* count)
    {
        auto* ptr = reg();
        if (!count || count->getType() == OperandType::IMMEI32)
        {
            std::vector<int> dims;
            if (count) dims.push_back(static_cast<ImmeI32Operand*>(count)->value);
            entry->insertFront(new AllocaInst(type, ptr, dims));
            return ptr;
        }
        if (!savedStack)
        {
            ASSERT(expressionStart);
            needsStackIntrinsics = true;
            savedStack           = reg();
            // A save in a short-circuit RHS would not dominate the restoration.
            auto& insts = expressionStart->insts;
            insts.insert(insts.begin() + expressionOffset, new CallInst(DataType::PTR, "llvm.stacksave", savedStack));
        }
        auto* alloc         = new AllocaInst(type, ptr);
        alloc->elementCount = count;
        emit(alloc);
        return ptr;
    }

    void ASTCodeGen::jump(Block* target)
    {
        if (!terminated()) emit(new BrUncondInst(getLabelOperand(target->blockId)));
    }

    void ASTCodeGen::branch(Operand* test, Block* yes, Block* no)
    { emit(new BrCondInst(test, getLabelOperand(yes->blockId), getLabelOperand(no->blockId))); }

    void ASTCodeGen::require(Operand* test)
    {
        if (!trapBlock)
        {
            needsTrap = true;
            trapBlock = newBlock();
            trapBlock->insert(new CallInst(DataType::VOID, "llvm.trap"));
            trapBlock->insert(new BrUncondInst(getLabelOperand(trapBlock->blockId)));
        }
        auto* pass = newBlock();
        branch(test, pass, trapBlock);
        block = pass;
    }
}  // namespace ME
