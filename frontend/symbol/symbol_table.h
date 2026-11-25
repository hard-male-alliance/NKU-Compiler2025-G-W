#ifndef __FRONTEND_SYMBOL_SYMBOL_TABLE_H__
#define __FRONTEND_SYMBOL_SYMBOL_TABLE_H__

#include <frontend/symbol/isymbol_table.h>
#include <map>

namespace FE::Sym
{
    class SymTable : public iSymTable<SymTable>
    {
        friend iSymTable<SymTable>;
        std::vector<std::map<Entry*, FE::AST::VarAttr>> scopes{1};

      public:
        bool containsLocal(Entry* entry) const { return scopes.back().find(entry) != scopes.back().end(); }

      private:
        void reset_impl();

        void              addSymbol_impl(Entry* entry, FE::AST::VarAttr& attr);
        FE::AST::VarAttr* getSymbol_impl(Entry* entry);
        void              enterScope_impl();
        void              exitScope_impl();

        bool isGlobalScope_impl();
        int  getScopeDepth_impl();
    };
}  // namespace FE::Sym

#endif  // __FRONTEND_SYMBOL_SYMBOL_TABLE_H__
