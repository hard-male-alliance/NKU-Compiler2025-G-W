#include <middleend/module/ir_instruction.h>
#include <debug.h>
#include <numeric>
#include <sstream>

namespace ME
{
    std::string LoadInst::toString() const
    {
        std::stringstream ss;
        ss << res << " = load " << dt << ", ptr " << ptr << getComment();
        return ss.str();
    }

    std::string StoreInst::toString() const
    {
        std::stringstream ss;
        ss << "store " << dt << " " << val << ", ptr " << ptr << getComment();
        return ss.str();
    }

    std::string ArithmeticInst::toString() const
    {
        std::stringstream ss;
        ss << res << " = " << opcode << " " << dt << " " << lhs << ", " << rhs << getComment();
        return ss.str();
    }

    std::string IcmpInst::toString() const
    {
        std::stringstream ss;
        ss << res << " = icmp " << cond << " " << dt << " " << lhs << ", " << rhs << getComment();
        return ss.str();
    }

    std::string FcmpInst::toString() const
    {
        std::stringstream ss;
        ss << res << " = fcmp " << cond << " " << dt << " " << lhs << ", " << rhs << getComment();
        return ss.str();
    }

    std::string AllocaInst::toString() const
    {
        std::stringstream ss;
        ss << res << " = alloca ";
        if (dims.empty())
            ss << dt;
        else
        {
            for (int dim : dims) ss << "[" << dim << " x ";
            ss << dt << std::string(dims.size(), ']');
        }
        if (elementCount) ss << ", i32 " << elementCount;
        ss << getComment();
        return ss.str();
    }

    std::string BrCondInst::toString() const
    {
        std::stringstream ss;
        ss << "br i1 " << cond << ", label " << trueTar << ", label " << falseTar << getComment();
        return ss.str();
    }

    std::string BrUncondInst::toString() const
    {
        std::stringstream ss;
        ss << "br label " << target << getComment();
        return ss.str();
    }

    static void initArrayGlb(std::ostream& out, DataType type, const FE::AST::VarAttr& value, size_t depth,
        size_t start, size_t length, size_t explicitCount)
    {
        if (depth == value.arrayDims.size())
        {
            out << type << " ";
            if (start >= explicitCount)
                out << "zeroinitializer";
            else if (type == DataType::F32)
                out << "0x" << std::hex << FLOAT_TO_DOUBLE_BITS(value.initList[start].getFloat()) << std::dec;
            else
                out << value.initList[start].getInt();
            return;
        }

        for (size_t i = depth; i < value.arrayDims.size(); ++i) out << "[" << value.arrayDims[i] << " x ";
        out << type << std::string(value.arrayDims.size() - depth, ']');
        if (start >= explicitCount || length == 0)
        {
            out << " zeroinitializer";
            return;
        }
        out << " [";
        const int    dim    = value.arrayDims[depth];
        const size_t stride = length / dim;
        for (int i = 0; i < dim; ++i)
        {
            if (i) out << ", ";
            initArrayGlb(out, type, value, depth + 1, start + i * stride, stride, explicitCount);
        }
        out << "]";
    }

    std::string GlbVarDeclInst::toString() const
    {
        std::stringstream ss;
        ss << "@" << name << " = global ";
        if (initList.arrayDims.empty())
        {
            ss << dt << " ";
            if (init)
                ss << init;
            else
                ss << "zeroinitializer";
        }
        else
        {
            size_t step = 1;
            for (int dim : initList.arrayDims) step *= dim;
            size_t explicitCount = initList.initList.size();
            while (explicitCount > 0)
            {
                const auto& tail = initList.initList[explicitCount - 1];
                const auto  bits = dt == DataType::F32 ? FLOAT_TO_INT_BITS(tail.getFloat()) : tail.getInt();
                if (bits != 0) break;
                --explicitCount;
            }
            initArrayGlb(ss, dt, initList, 0, 0, step, explicitCount);
        }
        ss << getComment();
        return ss.str();
    }

    std::string CallInst::toString() const
    {
        std::stringstream ss;
        if (retType != DataType::VOID) ss << res << " = ";
        ss << "call " << retType << " @" << funcName << "(";

        for (auto it = args.begin(); it != args.end(); ++it)
        {
            ss << it->first << " " << it->second;
            if (std::next(it) != args.end()) ss << ", ";
        }
        ss << ")" << getComment();
        return ss.str();
    }

    std::string RetInst::toString() const
    {
        std::stringstream ss;
        ss << "ret " << rt;
        if (res) ss << " " << res;
        ss << getComment();
        return ss.str();
    }

    std::string FuncDeclInst::toString() const
    {
        std::stringstream ss;
        ss << "declare " << retType << " @" << funcName << "(";
        for (auto it = argTypes.begin(); it != argTypes.end(); ++it)
        {
            ss << *it;
            if (std::next(it) != argTypes.end()) ss << ", ";
        }
        if (isVarArg) ss << ", ...";
        ss << ")" << getComment();
        return ss.str();
    }

    std::string FuncDefInst::toString() const
    {
        std::stringstream ss;
        ss << "define " << retType << " @" << funcName << "(";

        for (auto it = argRegs.begin(); it != argRegs.end(); ++it)
        {
            ss << it->first << " " << it->second;
            if (std::next(it) != argRegs.end()) ss << ", ";
        }
        ss << ")" << getComment();
        return ss.str();
    }

    std::string GEPInst::toString() const
    {
        std::stringstream ss;
        ss << res << " = getelementptr ";
        if (dims.empty())
            ss << dt;
        else
        {
            for (int dim : dims) ss << "[" << dim << " x ";
            ss << dt << std::string(dims.size(), ']');
        }

        ss << ", ptr " << basePtr;
        for (auto& idx : idxs) ss << ", " << idxType << " " << idx;
        ss << getComment();
        return ss.str();
    }

    std::string SI2FPInst::toString() const
    {
        std::stringstream ss;
        ss << dest << " = sitofp i32 " << src << " to float" << getComment();
        return ss.str();
    }

    std::string FP2SIInst::toString() const
    {
        std::stringstream ss;
        ss << dest << " = fptosi float " << src << " to i32" << getComment();
        return ss.str();
    }

    std::string ZextInst::toString() const
    {
        std::stringstream ss;
        ss << dest << " = zext " << from << " " << src << " to " << to << getComment();
        return ss.str();
    }

    std::string PhiInst::toString() const
    {
        std::stringstream ss;
        ss << res << " = phi " << dt << " ";

        for (auto it = incomingVals.begin(); it != incomingVals.end(); ++it)
        {
            ss << "[ " << it->second << ", " << it->first << " ]";
            if (std::next(it) != incomingVals.end()) ss << ", ";
        }
        ss << getComment();
        return ss.str();
    }
}  // namespace ME
