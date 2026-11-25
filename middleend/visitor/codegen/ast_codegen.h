#ifndef __MIDDLEEND_VISITOR_CODEGEN_AST_CODEGEN_H__
#define __MIDDLEEND_VISITOR_CODEGEN_AST_CODEGEN_H__

#include <frontend/ast/ast.h>
#include <frontend/ast/decl.h>
#include <frontend/ast/expr.h>
#include <frontend/ast/stmt.h>
#include <middleend/module/ir_module.h>
#include <functional>

namespace ME
{
    class ASTCodeGen : public FE::AST::Visitor_t<void, Module*>
    {
        using Entry = FE::AST::Entry;
        using Shape = std::vector<int>;

        struct Value
        {
            Operand* data  = nullptr;
            Operand* first = nullptr;
        };

        struct Storage
        {
            DataType element;
            Shape    shape;
            Value    value;
        };

        struct Loop
        {
            Block* next;
            Block* end;
        };

        const std::map<Entry*, FE::AST::VarAttr>&       globals;
        const std::map<Entry*, FE::AST::FuncDeclStmt*>& functions;
        std::map<Entry*, Storage>                       globalStorage;
        std::vector<std::map<Entry*, Storage>>          locals;
        std::vector<Loop>                               loops;
        Function*                                       function       = nullptr;
        Block*                                          entry          = nullptr;
        Block*                                          block          = nullptr;
        Block*                                          trapBlock      = nullptr;
        const FE::AST::FuncDeclStmt*                    sourceFunction = nullptr;
        Value                                           returnBuffer;
        Value                                           result;

        Block*   expressionStart      = nullptr;
        size_t   expressionOffset     = 0;
        Operand* savedStack           = nullptr;
        bool     needsStackIntrinsics = false;
        bool     needsTrap            = false;
        bool     needsCopy            = false;
        bool     needsZero            = false;

      public:
        ASTCodeGen(const std::map<Entry*, FE::AST::VarAttr>& globals,
            const std::map<Entry*, FE::AST::FuncDeclStmt*>&  functions)
            : globals(globals), functions(functions)
        {}

      private:
        void visit(FE::AST::Root&, Module*) override;
        void visit(FE::AST::Initializer&, Module*) override;
        void visit(FE::AST::InitializerList&, Module*) override;
        void visit(FE::AST::VarDeclarator&, Module*) override;
        void visit(FE::AST::ParamDeclarator&, Module*) override;
        void visit(FE::AST::VarDeclaration&, Module*) override;
        void visit(FE::AST::LeftValExpr&, Module*) override;
        void visit(FE::AST::LiteralExpr&, Module*) override;
        void visit(FE::AST::UnaryExpr&, Module*) override;
        void visit(FE::AST::BinaryExpr&, Module*) override;
        void visit(FE::AST::CallExpr&, Module*) override;
        void visit(FE::AST::CommaExpr&, Module*) override;
        void visit(FE::AST::ExprStmt&, Module*) override;
        void visit(FE::AST::FuncDeclStmt&, Module*) override;
        void visit(FE::AST::VarDeclStmt&, Module*) override;
        void visit(FE::AST::BlockStmt&, Module*) override;
        void visit(FE::AST::ReturnStmt&, Module*) override;
        void visit(FE::AST::WhileStmt&, Module*) override;
        void visit(FE::AST::IfStmt&, Module*) override;
        void visit(FE::AST::BreakStmt&, Module*) override;
        void visit(FE::AST::ContinueStmt&, Module*) override;
        void visit(FE::AST::ForStmt&, Module*) override;

        void generate(FE::AST::Node* node, Module* module)
        {
            if (node) apply(*this, *node, module);
        }
        Operand* reg() { return getRegOperand(function->getNewRegId()); }
        void     emit(Instruction* inst) { block->insertBack(inst); }
        Block*   newBlock() { return function->createBlock(); }
        bool     terminated() const { return !block->insts.empty() && block->insts.back()->isTerminator(); }
        void     jump(Block* target);
        void     branch(Operand* test, Block* yes, Block* no);
        void     require(Operand* test);
        void     registerIntrinsics(Module* module) const;

        void           startExpression();
        void           finishExpression();
        Value          eval(FE::AST::ExprNode& node, Module* module);
        Operand*       evalScalar(FE::AST::ExprNode& node, DataType dest, Module* module);
        Operand*       cast(Operand* operand, DataType source, DataType dest);
        Value          address(FE::AST::LeftValExpr& node, Module* module);
        const Storage& lookup(Entry* name) const;
        Operand*       gep(DataType type, Operand* address, Operand* index);
        Operand*       load(DataType type, Operand* address);
        void           store(DataType type, Operand* value, Operand* address);
        Operand*       allocate(DataType type, Operand* count = nullptr);
        Operand*       arithmetic(Operator op, DataType type, Operand* lhs, Operand* rhs);
        Operand*       scalarOp(FE::AST::Operator op, DataType type, Operand* lhs, Operand* rhs);
        Operand*       negate(DataType type, Operand* value);
        Operand*       compare(ICmpOp op, Operand* lhs, Operand* rhs);
        Operand*       compare(FCmpOp op, Operand* lhs, Operand* rhs);
        Value          logical(FE::AST::BinaryExpr& node, Module* module);
        Value          assign(FE::AST::BinaryExpr& node, Module* module);

        static DataType type(FE::AST::Type* t);
        static DataType exprType(const FE::AST::ExprNode& node) { return type(node.attr.val.value.type); }
        static Operand* imm(int value) { return getImmeI32Operand(value); }
        static Operand* constant(const FE::AST::VarValue& value);
        static int      span(const Shape& shape, size_t start);
        Operand*        elements(const Shape& shape, Operand* first);
        Operand*        byteSize(Operand* count);
        void            sameExtent(Operand* lhs, Operand* rhs);
        void            copy(Value dest, Value src, const Shape& shape);
        void            zeroFill(Operand* dest, Operand* count);
        void            forElements(Operand* count, const std::function<void(Operand*)>& body);
        Value           tensorUnary(FE::AST::UnaryExpr& node, Value source);
        Value           tensorBinary(FE::AST::BinaryExpr& node, Value lhs, Value rhs);
        Value           matmul(DataType type, Value lhs, Value rhs, int inner, int columns);
    };
}  // namespace ME
#endif
