#include <middleend/visitor/codegen/ast_codegen.h>

#include <climits>
#include <numeric>

namespace ME
{
    using namespace FE::AST;

    int ASTCodeGen::span(const Shape& shape, size_t start)
    { return std::accumulate(shape.begin() + start, shape.end(), 1, std::multiplies<int>()); }

    Operand* ASTCodeGen::elements(const Shape& shape, Operand* first)
    {
        if (shape.front() > 0) return imm(span(shape, 0));
        auto stride = span(shape, 1);
        if (stride == 1) return first;
        require(compare(ICmpOp::SLE, first, imm(INT_MAX / stride)));
        return arithmetic(Operator::MUL, DataType::I32, first, imm(stride));
    }

    Operand* ASTCodeGen::byteSize(Operand* count)
    {
        auto* extended = reg();
        emit(new ZextInst(DataType::I32, DataType::I64, count, extended));
        return arithmetic(Operator::MUL, DataType::I64, extended, imm(4));
    }

    void ASTCodeGen::zeroFill(Operand* dest, Operand* count)
    {
        needsZero = true;
        using D   = DataType;
        emit(new CallInst(D::VOID,
            "llvm.memset.p0.i64",
            {{D::PTR, dest}, {D::I8, imm(0)}, {D::I64, byteSize(count)}, {D::I1, imm(0)}}));
    }

    void ASTCodeGen::sameExtent(Operand* a, Operand* b)
    {
        if (a != b) require(compare(ICmpOp::EQ, a, b));
    }

    void ASTCodeGen::copy(Value dest, Value src, const Shape& shape)
    {
        sameExtent(dest.first, src.first);
        if (dest.data == src.data) return;
        needsCopy = true;
        using D   = DataType;
        emit(new CallInst(D::VOID,
            "llvm.memmove.p0.p0.i64",
            {{D::PTR, dest.data},
                {D::PTR, src.data},
                {D::I64, byteSize(elements(shape, dest.first))},
                {D::I1, imm(0)}}));
    }

    void ASTCodeGen::forElements(Operand* count, const std::function<void(Operand*)>& body)
    {
        auto* before = block;
        auto* test   = newBlock();
        auto* work   = newBlock();
        auto* end    = newBlock();
        jump(test);

        block       = test;
        auto* index = reg();
        auto* phi   = new PhiInst(DataType::I32, index);
        phi->addIncoming(imm(0), getLabelOperand(before->blockId));
        emit(phi);
        branch(compare(ICmpOp::SLT, index, count), work, end);

        block = work;
        body(index);
        auto* next = arithmetic(Operator::ADD, DataType::I32, index, imm(1));
        phi->addIncoming(next, getLabelOperand(block->blockId));
        jump(test);
        block = end;
    }

    ASTCodeGen::Value ASTCodeGen::tensorUnary(UnaryExpr& node, Value source)
    {
        auto  dt    = type(elementType(node.attr.val.value.type));
        auto* count = elements(node.shape, source.first);
        Value out{allocate(dt, count), source.first};
        if (node.op == FE::AST::Operator::ADD)
        {
            copy(out, source, node.shape);
            return out;
        }
        forElements(
            count, [&](Operand* i) { store(dt, negate(dt, load(dt, gep(dt, source.data, i))), gep(dt, out.data, i)); });
        return out;
    }

    ASTCodeGen::Value ASTCodeGen::tensorBinary(BinaryExpr& node, Value lhs, Value rhs)
    {
        auto dt = type(elementType(node.attr.val.value.type));
        if (node.op == FE::AST::Operator::MATMUL) return matmul(dt, lhs, rhs, node.lhs->shape[1], node.shape[1]);
        if (lhs.first && rhs.first) sameExtent(lhs.first, rhs.first);
        auto* first = node.shape.front() > 0 ? imm(node.shape.front()) : lhs.first ? lhs.first : rhs.first;
        auto* count = elements(node.shape, first);
        Value out{allocate(dt, count), first};
        forElements(count, [&](Operand* i) {
            auto* a = lhs.first ? load(dt, gep(dt, lhs.data, i)) : lhs.data;
            auto* b = rhs.first ? load(dt, gep(dt, rhs.data, i)) : rhs.data;
            store(dt, scalarOp(node.op, dt, a, b), gep(dt, out.data, i));
        });
        return out;
    }

    ASTCodeGen::Value ASTCodeGen::matmul(DataType dt, Value lhs, Value rhs, int inner, int columns)
    {
        sameExtent(imm(inner), rhs.first);
        Shape shape{lhs.first->getType() == OperandType::IMMEI32 ? static_cast<ImmeI32Operand*>(lhs.first)->value : -1,
            columns};
        auto* count = elements(shape, lhs.first);
        Value out{allocate(dt, count), lhs.first};
        auto* sum  = allocate(dt);
        auto* zero = dt == DataType::F32 ? static_cast<Operand*>(getImmeF32Operand(0.0f)) : imm(0);
        forElements(count, [&](Operand* index) {
            auto* row      = arithmetic(Operator::DIV, DataType::I32, index, imm(columns));
            auto* col      = arithmetic(Operator::MOD, DataType::I32, index, imm(columns));
            auto* rowStart = arithmetic(Operator::MUL, DataType::I32, row, imm(inner));
            store(dt, zero, sum);
            forElements(imm(inner), [&](Operand* k) {
                auto* ai = arithmetic(Operator::ADD, DataType::I32, rowStart, k);
                auto* bi = arithmetic(
                    Operator::ADD, DataType::I32, arithmetic(Operator::MUL, DataType::I32, k, imm(columns)), col);
                auto* a       = load(dt, gep(dt, lhs.data, ai));
                auto* b       = load(dt, gep(dt, rhs.data, bi));
                auto* product = scalarOp(FE::AST::Operator::MUL, dt, a, b);
                store(dt, scalarOp(FE::AST::Operator::ADD, dt, load(dt, sum), product), sum);
            });
            store(dt, load(dt, sum), gep(dt, out.data, index));
        });
        return out;
    }
}  // namespace ME
