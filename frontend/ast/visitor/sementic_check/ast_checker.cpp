#include <frontend/ast/visitor/sementic_check/ast_checker.h>
#include <debug.h>

namespace FE::AST
{
    bool ASTChecker::error(const Node& node, const std::string& message)
    {
        errors.push_back(node.line_num < 0
                             ? message
                             : std::to_string(node.line_num) + ":" + std::to_string(node.col_num) + ": " + message);
        return false;
    }

    bool ASTChecker::visit(Root& node)
    {
        symTable.reset();
        glbSymbols.clear();
        errors.clear();
        funcDecls.clear();
        for (const auto& builtin : builtins) funcDecls.emplace(builtin->entry, builtin.get());
        bool ok = true;
        for (auto* stmt : *node.getStmts()) ok &= check(stmt);
        if (funcDecls.find(Entry::getEntry("main")) == funcDecls.end()) ok &= error(node, "missing main function");
        return ok;
    }

    void ASTChecker::libFuncRegister()
    {
        auto add = [&](const char* name, Type* result, std::initializer_list<Type*> args) {
            auto params = std::make_unique<std::vector<ParamDeclarator*>>();
            for (auto* type : args)
            {
                auto* param                = new ParamDeclarator(type, nullptr);
                param->attr.val.value.type = type;
                params->push_back(param);
            }
            builtins.emplace_back(std::make_unique<FuncDeclStmt>(result, Entry::getEntry(name), params.release()));
        };

        auto* intPtr   = TypeFactory::getPtrType(intType);
        auto* floatPtr = TypeFactory::getPtrType(floatType);
        add("getint", intType, {});
        add("getch", intType, {});
        add("getfloat", floatType, {});
        add("getarray", intType, {intPtr});
        add("getfarray", intType, {floatPtr});
        add("putint", voidType, {intType});
        add("putch", voidType, {intType});
        add("putfloat", voidType, {floatType});
        add("putarray", voidType, {intType, intPtr});
        add("putfarray", voidType, {intType, floatPtr});
        add("_sysy_starttime", voidType, {intType});
        add("_sysy_stoptime", voidType, {intType});
    }
}  // namespace FE::AST
