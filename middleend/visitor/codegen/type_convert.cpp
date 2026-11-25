#include <middleend/visitor/codegen/ast_codegen.h>

namespace ME
{
    using namespace FE::AST;

    DataType ASTCodeGen::type(Type* t)
    {
        if (t->getTypeGroup() != TypeGroup::BASIC) return DataType::PTR;
        switch (t->getBaseType())
        {
            case Type_t::VOID: return DataType::VOID;
            case Type_t::BOOL: return DataType::I1;
            case Type_t::LL:
            case Type_t::INT: return DataType::I32;
            case Type_t::FLOAT: return DataType::F32;
            default: ERROR("Unknown AST type");
        }
    }

    Operand* ASTCodeGen::constant(const VarValue& value)
    {
        return value.type == floatType ? static_cast<Operand*>(getImmeF32Operand(value.getFloat()))
                                       : imm(value.getInt());
    }

    Operand* ASTCodeGen::compare(ICmpOp op, Operand* lhs, Operand* rhs)
    {
        auto* out = reg();
        emit(new IcmpInst(DataType::I32, op, lhs, rhs, out));
        return out;
    }

    Operand* ASTCodeGen::compare(FCmpOp op, Operand* lhs, Operand* rhs)
    {
        auto* out = reg();
        emit(new FcmpInst(DataType::F32, op, lhs, rhs, out));
        return out;
    }

    Operand* ASTCodeGen::cast(Operand* operand, DataType src, DataType dest)
    {
        if (src == dest) return operand;
        if (dest == DataType::I1)
            return src == DataType::F32 ? compare(FCmpOp::UNE, operand, getImmeF32Operand(0))
                                        : compare(ICmpOp::NE, operand, imm(0));

        if (src == DataType::I1)
        {
            if (operand->getType() != OperandType::IMMEI32)
            {
                auto* out = reg();
                emit(new ZextInst(src, DataType::I32, operand, out));
                operand = out;
            }
            src = DataType::I32;
            if (dest == src) return operand;
        }

        auto* out = reg();
        if (src == DataType::F32)
            emit(new FP2SIInst(operand, out));
        else
            emit(new SI2FPInst(operand, out));
        return out;
    }

    Operand* ASTCodeGen::arithmetic(Operator op, DataType dt, Operand* lhs, Operand* rhs)
    {
        auto* out = reg();
        emit(new ArithmeticInst(op, dt, lhs, rhs, out));
        return out;
    }

    Operand* ASTCodeGen::negate(DataType dt, Operand* value)
    {
        if (dt == DataType::F32) return arithmetic(Operator::FSUB, dt, getImmeF32Operand(-0.0f), value);
        return arithmetic(Operator::SUB, dt, imm(0), value);
    }

    Operand* ASTCodeGen::scalarOp(FE::AST::Operator op, DataType dt, Operand* a, Operand* b)
    {
        using A       = FE::AST::Operator;
        const bool fp = dt == DataType::F32;
        switch (op)
        {
            case A::ADD: return arithmetic(fp ? Operator::FADD : Operator::ADD, dt, a, b);
            case A::SUB: return arithmetic(fp ? Operator::FSUB : Operator::SUB, dt, a, b);
            case A::MUL: return arithmetic(fp ? Operator::FMUL : Operator::MUL, dt, a, b);
            case A::DIV: return arithmetic(fp ? Operator::FDIV : Operator::DIV, dt, a, b);
            case A::MOD: return arithmetic(Operator::MOD, dt, a, b);
            case A::EQ: return fp ? compare(FCmpOp::OEQ, a, b) : compare(ICmpOp::EQ, a, b);
            case A::NEQ: return fp ? compare(FCmpOp::UNE, a, b) : compare(ICmpOp::NE, a, b);
            case A::LT: return fp ? compare(FCmpOp::OLT, a, b) : compare(ICmpOp::SLT, a, b);
            case A::LE: return fp ? compare(FCmpOp::OLE, a, b) : compare(ICmpOp::SLE, a, b);
            case A::GT: return fp ? compare(FCmpOp::OGT, a, b) : compare(ICmpOp::SGT, a, b);
            case A::GE: return fp ? compare(FCmpOp::OGE, a, b) : compare(ICmpOp::SGE, a, b);
            default: ERROR("Unsupported binary operator");
        }
    }
}  // namespace ME
