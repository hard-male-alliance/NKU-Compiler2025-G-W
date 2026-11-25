#include <middleend/visitor/codegen/ast_codegen.h>

namespace ME
{
    using namespace FE::AST;

    const ASTCodeGen::Storage& ASTCodeGen::lookup(Entry* name) const
    {
        for (auto scope = locals.rbegin(); scope != locals.rend(); ++scope)
        {
            auto it = scope->find(name);
            if (it != scope->end()) return it->second;
        }
        return globalStorage.at(name);
    }

    Operand* ASTCodeGen::gep(DataType dt, Operand* ptr, Operand* index)
    {
        auto* out = reg();
        emit(new GEPInst(dt, DataType::I32, ptr, out, {}, {index}));
        return out;
    }

    Operand* ASTCodeGen::load(DataType dt, Operand* ptr)
    {
        auto* out = reg();
        emit(new LoadInst(dt, ptr, out));
        return out;
    }

    void ASTCodeGen::store(DataType dt, Operand* value, Operand* ptr) { emit(new StoreInst(dt, value, ptr)); }

    void ASTCodeGen::visit(Initializer& node, Module* module) { result = eval(*node.init_val, module); }

    void ASTCodeGen::visit(InitializerList& node, Module* module)
    {
        for (auto* init : *node.init_list) generate(init, module);
    }

    void ASTCodeGen::visit(ParamDeclarator& node, Module*)
    {
        auto& args      = function->funcDef->argRegs;
        auto  dt        = type(elementType(node.type));
        bool  aggregate = !node.binding.arrayDims.empty();
        auto* arg       = reg();
        args.emplace_back(aggregate ? DataType::PTR : dt, arg);
        Value value{arg};
        if (isTensor(node.type))
        {
            value.first = reg();
            args.emplace_back(DataType::I32, value.first);
        }
        if (!aggregate)
        {
            value.data = allocate(dt);
            store(dt, arg, value.data);
        }
        locals.back().emplace(node.entry, Storage{dt, node.binding.arrayDims, value});
    }

    void ASTCodeGen::visit(VarDeclarator& node, Module* module)
    {
        auto&       name      = static_cast<LeftValExpr&>(*node.lval);
        const auto& dims      = node.binding.arrayDims;
        bool        aggregate = !dims.empty();
        auto        dt        = type(elementType(node.binding.type));
        auto*       count     = imm(span(dims, 0));
        Value       dest{
            allocate(dt, aggregate ? count : nullptr), isTensor(node.binding.type) ? imm(dims.front()) : nullptr};

        if (node.init && aggregate && node.init->singleInit)
        {
            startExpression();
            copy(dest, eval(*static_cast<Initializer&>(*node.init).init_val, module), dims);
            finishExpression();
        }
        else if (node.init)
        {
            if (aggregate && node.initializers.size() != static_cast<size_t>(span(dims, 0))) zeroFill(dest.data, count);
            if (!aggregate && node.initializers.empty())
                store(dt, dt == DataType::F32 ? static_cast<Operand*>(getImmeF32Operand(0.0f)) : imm(0), dest.data);
            for (auto [offset, expr] : node.initializers)
            {
                startExpression();
                auto* val = evalScalar(*expr, dt, module);
                store(dt, val, aggregate ? gep(dt, dest.data, imm(offset)) : dest.data);
                finishExpression();
            }
        }
        locals.back().emplace(name.entry, Storage{dt, dims, dest});
    }

    void ASTCodeGen::visit(VarDeclaration& node, Module* module)
    {
        for (auto* v : *node.decls) generate(v, module);
    }
}  // namespace ME
