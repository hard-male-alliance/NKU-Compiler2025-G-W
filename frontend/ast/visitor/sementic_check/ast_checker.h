#ifndef __FRONTEND_AST_VISITOR_SEMENTIC_CHECK_AST_CHECKER_H__
#define __FRONTEND_AST_VISITOR_SEMENTIC_CHECK_AST_CHECKER_H__

#include <frontend/ast/ast_visitor.h>
#include <frontend/ast/ast.h>
#include <frontend/ast/decl.h>
#include <frontend/ast/expr.h>
#include <frontend/ast/stmt.h>
#include <frontend/symbol/symbol_table.h>
#include <memory>
#include <optional>
#include <vector>

namespace FE::AST
{
    // A negative extent denotes the first dimension of a tensor parameter.
    inline bool compatibleShape(const std::vector<int>& a, const std::vector<int>& b)
    {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (a[i] >= 0 && b[i] >= 0 && a[i] != b[i]) return false;
        return true;
    }

    using Checker_t = Visitor_t<bool>;

    class ASTChecker : public Checker_t
    {
      private:
        FE::Sym::SymTable                        symTable;
        std::map<FE::Sym::Entry*, VarAttr>       glbSymbols;
        std::map<FE::Sym::Entry*, FuncDeclStmt*> funcDecls;

        std::vector<std::unique_ptr<FuncDeclStmt>> builtins;

        FuncDeclStmt*   currentFunction     = nullptr;
        VarDeclaration* currentDeclaration  = nullptr;
        size_t          loopDepth           = 0;
        int             parameterIndex      = 0;
        bool            recursiveTensorCall = false;
        bool            inferReturnShape    = true;

        bool error(const Node& node, const std::string& message);
        bool check(Node* node) { return !node || apply(*this, *node); }
        bool condition(ExprNode* node);
        bool dimensions(const std::vector<ExprNode*>* exprs, std::vector<int>& shape, bool parameter, bool tensor);
        bool flatten(InitDecl& init, const std::vector<int>& spans, size_t level, int offset,
            std::vector<std::pair<int, ExprNode*>>& values, bool tensor);
        std::optional<VarValue> tensorConstant(ExprNode& expr, int index);
        bool                    tensorBinary(BinaryExpr& node, Type* lhs, Type* rhs);

      public:
        std::vector<std::string> errors;

        ASTChecker() { libFuncRegister(); }

        const std::map<FE::Sym::Entry*, VarAttr>&       getGlbSymbols() const { return glbSymbols; }
        const std::map<FE::Sym::Entry*, FuncDeclStmt*>& getFuncDecls() const { return funcDecls; }

      private:
        // Basic AST nodes
        bool visit(Root& node) override;

        // Declaration nodes
        bool visit(Initializer& node) override;
        bool visit(InitializerList& node) override;
        bool visit(VarDeclarator& node) override;
        bool visit(ParamDeclarator& node) override;
        bool visit(VarDeclaration& node) override;

        // Expression nodes
        bool visit(LeftValExpr& node) override;
        bool visit(LiteralExpr& node) override;
        bool visit(UnaryExpr& node) override;
        bool visit(BinaryExpr& node) override;
        bool visit(CallExpr& node) override;
        bool visit(CommaExpr& node) override;

        // Statement nodes
        bool visit(ExprStmt& node) override;
        bool visit(FuncDeclStmt& node) override;
        bool visit(VarDeclStmt& node) override;
        bool visit(BlockStmt& node) override;
        bool visit(ReturnStmt& node) override;
        bool visit(WhileStmt& node) override;
        bool visit(IfStmt& node) override;
        bool visit(BreakStmt& node) override;
        bool visit(ContinueStmt& node) override;
        bool visit(ForStmt& node) override;

        void libFuncRegister();

      private:
        ExprValue typeInfer(const ExprValue& operand, Operator op);
        ExprValue typeInfer(const ExprValue& lhs, const ExprValue& rhs, Operator op, const Node& node, bool& hasError);
    };
}  // namespace FE::AST

#endif  // __FRONTEND_AST_VISITOR_SEMENTIC_CHECK_AST_CHECKER_H__
