#include <middleend/visitor/codegen/ast_codegen.h>

namespace ME
{
    using namespace FE::AST;

    ASTCodeGen::Value ASTCodeGen::eval(ExprNode& node, Module* module)
    {
        if (node.attr.val.isConstexpr) return {constant(node.attr.val.value)};
        apply(*this, node, module);
        return result;
    }

    Operand* ASTCodeGen::evalScalar(ExprNode& node, DataType dest, Module* module)
    {
        auto value = eval(node, module);
        return cast(value.data, exprType(node), dest);
    }

    ASTCodeGen::Value ASTCodeGen::address(LeftValExpr& node, Module* module)
    {
        const auto& slot = lookup(node.entry);
        if (!node.indices) return slot.value;

        Operand*    offset = nullptr;
        const auto& shape  = slot.shape;
        for (size_t i = 0; i < node.indices->size(); ++i)
        {
            auto* idx = evalScalar(*(*node.indices)[i], DataType::I32, module);
            offset    = offset ? arithmetic(Operator::ADD,
                                     DataType::I32,
                                     arithmetic(Operator::MUL, DataType::I32, offset, imm(shape[i])),
                                     idx)
                               : idx;
        }
        int remaining = span(shape, node.indices->size());
        if (remaining != 1) offset = arithmetic(Operator::MUL, DataType::I32, offset, imm(remaining));
        return {gep(slot.element, slot.value.data, offset)};
    }

    void ASTCodeGen::visit(LeftValExpr& node, Module* module)
    {
        auto value = address(node, module);
        if (node.shape.empty()) value.data = load(exprType(node), value.data);
        result = value;
    }

    void ASTCodeGen::visit(LiteralExpr& node, Module*) { result = {constant(node.literal)}; }

    void ASTCodeGen::visit(UnaryExpr& node, Module* module)
    {
        using A    = FE::AST::Operator;
        auto value = eval(*node.expr, module);
        if (isTensor(node.attr.val.value.type))
        {
            result = tensorUnary(node, value);
            return;
        }
        auto dt = exprType(*node.expr);
        if (node.op == A::NOT)
            result = {dt == DataType::F32 ? compare(FCmpOp::OEQ, value.data, getImmeF32Operand(0))
                                          : compare(ICmpOp::EQ, cast(value.data, dt, DataType::I32), imm(0))};
        else
        {
            auto  out     = exprType(node);
            auto* operand = cast(value.data, dt, out);
            result        = {node.op == A::ADD ? operand : negate(out, operand)};
        }
    }

    ASTCodeGen::Value ASTCodeGen::logical(BinaryExpr& node, Module* module)
    {
        bool  isAnd   = node.op == FE::AST::Operator::AND;
        auto* left    = evalScalar(*node.lhs, DataType::I1, module);
        auto* leftEnd = block;
        auto* rhs     = newBlock();
        auto* join    = newBlock();
        branch(left, isAnd ? rhs : join, isAnd ? join : rhs);

        block          = rhs;
        auto* right    = evalScalar(*node.rhs, DataType::I1, module);
        auto* rightEnd = block;
        jump(join);

        block       = join;
        auto* value = reg();
        auto* phi   = new PhiInst(DataType::I1, value);
        phi->addIncoming(imm(!isAnd), getLabelOperand(leftEnd->blockId));
        phi->addIncoming(right, getLabelOperand(rightEnd->blockId));
        emit(phi);
        return {value};
    }

    ASTCodeGen::Value ASTCodeGen::assign(BinaryExpr& node, Module* module)
    {
        auto dest  = address(static_cast<LeftValExpr&>(*node.lhs), module);
        auto value = eval(*node.rhs, module);
        if (isTensor(node.attr.val.value.type))
        {
            copy(dest, value, node.shape);
            return dest;
        }
        auto  dt     = exprType(node);
        auto* scalar = cast(value.data, exprType(*node.rhs), dt);
        store(dt, scalar, dest.data);
        return {scalar};
    }

    void ASTCodeGen::visit(BinaryExpr& node, Module* module)
    {
        using A = FE::AST::Operator;
        if (node.op == A::ASSIGN)
        {
            result = assign(node, module);
            return;
        }
        if (node.op == A::AND || node.op == A::OR)
        {
            result = logical(node, module);
            return;
        }

        auto lhs = eval(*node.lhs, module);
        auto rhs = eval(*node.rhs, module);
        if (isTensor(node.attr.val.value.type))
        {
            result = tensorBinary(node, lhs, rhs);
            return;
        }
        auto  lt = exprType(*node.lhs);
        auto  rt = exprType(*node.rhs);
        auto  dt = lt == DataType::F32 || rt == DataType::F32 ? DataType::F32 : DataType::I32;
        auto* a  = cast(lhs.data, lt, dt);
        auto* b  = cast(rhs.data, rt, dt);
        result   = {scalarOp(node.op, dt, a, b)};
    }

    void ASTCodeGen::visit(CallExpr& node, Module* module)
    {
        const auto&       decl = *functions.at(node.func);
        CallInst::argList args;
        Value             out;
        auto              dt             = type(decl.retType);
        Operand*          inferredExtent = nullptr;
        if (node.args)
        {
            for (size_t i = 0; i < node.args->size(); ++i)
            {
                auto&       actual = *(*node.args)[i];
                const auto* formal = (*decl.params)[i];
                auto*       target = formal->attr.val.value.type;
                auto        value  = eval(actual, module);
                args.emplace_back(type(target), cast(value.data, exprType(actual), type(target)));
                if (isTensor(target)) args.emplace_back(DataType::I32, value.first);
                if (decl.returnExtentParameter == static_cast<int>(i)) inferredExtent = value.first;
            }
        }
        if (isTensor(decl.retType))
        {
            dt        = DataType::VOID;
            out.first = node.shape.front() > 0 ? imm(node.shape.front()) : inferredExtent;
            out.data  = allocate(type(elementType(decl.retType)), elements(node.shape, out.first));
            args.insert(args.begin(), {DataType::PTR, out.data});
        }
        if (dt != DataType::VOID) out.data = reg();
        emit(new CallInst(dt, node.func->getName(), args, dt == DataType::VOID ? nullptr : out.data));
        result = out;
    }

    void ASTCodeGen::visit(CommaExpr& node, Module* module)
    {
        for (auto* expr : *node.exprs) result = eval(*expr, module);
    }
}  // namespace ME
