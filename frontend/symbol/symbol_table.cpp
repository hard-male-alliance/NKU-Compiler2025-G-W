#include <frontend/symbol/symbol_table.h>
#include <cassert>

namespace FE::Sym
{
    void SymTable::reset_impl() { scopes.assign(1, {}); }
    void SymTable::enterScope_impl() { scopes.emplace_back(); }

    void SymTable::exitScope_impl()
    {
        assert(scopes.size() > 1);
        scopes.pop_back();
    }

    void SymTable::addSymbol_impl(Entry* entry, FE::AST::VarAttr& attr) { scopes.back().insert_or_assign(entry, attr); }

    FE::AST::VarAttr* SymTable::getSymbol_impl(Entry* entry)
    {
        for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope)
        {
            auto it = scope->find(entry);
            if (it != scope->end()) return &it->second;
        }
        return nullptr;
    }

    bool SymTable::isGlobalScope_impl() { return scopes.size() == 1; }
    int  SymTable::getScopeDepth_impl() { return static_cast<int>(scopes.size() - 1); }
}  // namespace FE::Sym
