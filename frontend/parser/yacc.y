%skeleton "lalr1.cc"
%require "3.2"

%define api.namespace { FE }
%define api.parser.class { YaccParser }
%define api.token.constructor
%define api.value.type variant
%define parse.assert
%expect 0
%defines

%code requires
{
    #include <memory>
    #include <string>
    #include <utility>
    #include <vector>
    #include <frontend/ast/ast_defs.h>
    #include <frontend/ast/ast.h>
    #include <frontend/ast/stmt.h>
    #include <frontend/ast/expr.h>
    #include <frontend/ast/decl.h>
    #include <frontend/symbol/symbol_entry.h>

    namespace FE
    {
        // Lists own their elements until an AST node takes the complete list.
        template<class T> struct ParserListDeleter
        {
            void operator()(std::vector<T*>* nodes) const
            {
                for (auto* node : *nodes) delete node;
                delete nodes;
            }
        };
        template<class T> using ParserList = std::unique_ptr<std::vector<T*>, ParserListDeleter<T>>;

        class Parser;
        class Scanner;
    }
}

%code top
{
    #include <iostream>

    #include <frontend/parser/parser.h>
    #include <frontend/parser/location.hh>
    #include <frontend/parser/scanner.h>
    #include <frontend/parser/yacc.h>

    using namespace FE;
    using namespace FE::AST;

    static YaccParser::symbol_type yylex(Scanner& scanner, Parser &parser)
    {
        (void)parser;
        return scanner.nextToken(); 
    }

}

%lex-param { FE::Scanner& scanner }
%lex-param { FE::Parser& parser }
%parse-param { FE::Scanner& scanner }
%parse-param { FE::Parser& parser }

%locations

%define parse.error verbose
%define api.token.prefix {TOKEN_}

// 从这开始定义你需要用到的 token
// 对于一些需要 "值" 的 token，可以在前面加上 <type> 来指定值的类型
// 例如，%token <int> INT_CONST 定义了一个名为 INT_CONST
%token <int> INT_CONST
%token <std::string> STR_CONST ERR_TOKEN SLASH_COMMENT

%token <std::string> IDENT 

%token IF ELSE FOR WHILE CONTINUE BREAK SWITCH CASE GOTO DO RETURN CONST
%token SEMICOLON COMMA LPAREN RPAREN LBRACKET RBRACKET LBRACE RBRACE
%token END

// Lab1 token payloads preserve the framework's printed token names and values.
%token INT FLOAT VOID
%token <long long> LL_CONST
%token <float> FLOAT_CONST
%token PLUS MINUS STAR SLASH MOD ASSIGN
%token LT GT LE GE EQ NE NOT AND OR

%nterm <FE::AST::Operator> UNARY_OP
%nterm <FE::AST::Type*> TYPE
%nterm <std::unique_ptr<FE::AST::ExprNode>> LITERAL_EXPR BASIC_EXPR FUNC_CALL_EXPR UNARY_EXPR MULDIV_EXPR ADDSUB_EXPR RELATIONAL_EXPR EQUALITY_EXPR LOGICAL_AND_EXPR LOGICAL_OR_EXPR ASSIGN_EXPR NOCOMMA_EXPR EXPR ARRAY_DIMENSION_EXPR LEFT_VAL_EXPR OPTIONAL_EXPR
%nterm <std::unique_ptr<FE::AST::StmtNode>> EXPR_STMT VAR_DECL_STMT BLOCK_STMT FUNC_DECL_STMT RETURN_STMT WHILE_STMT IF_STMT BREAK_STMT CONTINUE_STMT FOR_STMT STMT BLOCK_ITEM TOP_LEVEL_ITEM FOR_INIT
%nterm <std::unique_ptr<FE::AST::InitDecl>> INITIALIZER
%nterm <std::unique_ptr<FE::AST::VarDeclarator>> VAR_DECLARATOR
%nterm <std::unique_ptr<FE::AST::VarDeclaration>> VAR_DECLARATION
%nterm <std::unique_ptr<FE::AST::ParamDeclarator>> PARAM_DECLARATOR
%nterm <FE::ParserList<FE::AST::InitDecl>> INITIALIZER_LIST
%nterm <FE::ParserList<FE::AST::VarDeclarator>> VAR_DECLARATOR_LIST
%nterm <FE::ParserList<FE::AST::ParamDeclarator>> PARAM_DECLARATOR_LIST PARAM_DECLARATORS
%nterm <FE::ParserList<FE::AST::ExprNode>> EXPR_LIST ARRAY_DIMENSION_EXPR_LIST
%nterm <FE::ParserList<FE::AST::StmtNode>> STMT_LIST TOP_LEVEL_LIST

%start PROGRAM
%precedence THEN
%precedence ELSE

%%

// Publish the root only after the complete input has been accepted.
PROGRAM:
    TOP_LEVEL_LIST END { parser.ast = new Root($1.release()); YYACCEPT; }
    ;

TOP_LEVEL_LIST:
    TOP_LEVEL_ITEM { $$.reset(new std::vector<StmtNode*>()); $$->push_back($1.release()); }
    | TOP_LEVEL_LIST TOP_LEVEL_ITEM { $$ = std::move($1); $$->push_back($2.release()); }
    ;

TOP_LEVEL_ITEM:
    VAR_DECL_STMT { $$ = std::move($1); }
    | FUNC_DECL_STMT { $$ = std::move($1); }
    ;

STMT_LIST:
    %empty { $$.reset(new std::vector<StmtNode*>()); }
    | STMT_LIST BLOCK_ITEM { $$ = std::move($1); if ($2) $$->push_back($2.release()); }
    ;

BLOCK_ITEM:
    VAR_DECL_STMT { $$ = std::move($1); }
    | STMT { $$ = std::move($1); }
    ;

STMT:
    EXPR_STMT { $$ = std::move($1); }
    | BLOCK_STMT { $$ = std::move($1); }
    | RETURN_STMT { $$ = std::move($1); }
    | WHILE_STMT { $$ = std::move($1); }
    | FOR_STMT { $$ = std::move($1); }
    | IF_STMT { $$ = std::move($1); }
    | BREAK_STMT { $$ = std::move($1); }
    | CONTINUE_STMT { $$ = std::move($1); }
    | SEMICOLON { $$ = nullptr; }
    ;

BLOCK_STMT:
    LBRACE STMT_LIST RBRACE { $$.reset(new BlockStmt($2.release(), @1.begin.line, @1.begin.column)); }
    ;

CONTINUE_STMT:
    CONTINUE SEMICOLON { $$.reset(new ContinueStmt(@1.begin.line, @1.begin.column)); }
    ;

BREAK_STMT:
    BREAK SEMICOLON { $$.reset(new BreakStmt(@1.begin.line, @1.begin.column)); }
    ;

RETURN_STMT:
    RETURN OPTIONAL_EXPR SEMICOLON { $$.reset(new ReturnStmt($2.release(), @1.begin.line, @1.begin.column)); }
    ;

EXPR_STMT:
    EXPR SEMICOLON { $$.reset(new ExprStmt($1.release(), @1.begin.line, @1.begin.column)); }
    ;

VAR_DECLARATION:
    TYPE VAR_DECLARATOR_LIST { $$.reset(new VarDeclaration($1, $2.release(), false, @1.begin.line, @1.begin.column)); }
    | CONST TYPE VAR_DECLARATOR_LIST { $$.reset(new VarDeclaration($2, $3.release(), true, @1.begin.line, @1.begin.column)); }
    ;

VAR_DECL_STMT:
    VAR_DECLARATION SEMICOLON { $$.reset(new VarDeclStmt($1.release(), @1.begin.line, @1.begin.column)); }
    ;

FUNC_DECL_STMT:
    TYPE IDENT LPAREN PARAM_DECLARATOR_LIST RPAREN BLOCK_STMT {
        $$.reset(new FuncDeclStmt($1, Entry::getEntry($2), $4.release(), $6.release(), @1.begin.line, @1.begin.column));
    }
    ;

FOR_INIT:
    %empty { $$ = nullptr; }
    | VAR_DECLARATION { $$.reset(new VarDeclStmt($1.release(), @1.begin.line, @1.begin.column)); }
    | EXPR { $$.reset(new ExprStmt($1.release(), @1.begin.line, @1.begin.column)); }
    ;

OPTIONAL_EXPR:
    %empty { $$ = nullptr; }
    | EXPR { $$ = std::move($1); }
    ;

FOR_STMT:
    FOR LPAREN FOR_INIT SEMICOLON OPTIONAL_EXPR SEMICOLON OPTIONAL_EXPR RPAREN STMT {
        $$.reset(new ForStmt($3.release(), $5.release(), $7.release(), $9.release(), @1.begin.line, @1.begin.column));
    }
    ;

IF_STMT:
    IF LPAREN EXPR RPAREN STMT %prec THEN {
        $$.reset(new IfStmt($3.release(), $5.release(), nullptr, @1.begin.line, @1.begin.column));
    }
    | IF LPAREN EXPR RPAREN STMT ELSE STMT {
        $$.reset(new IfStmt($3.release(), $5.release(), $7.release(), @1.begin.line, @1.begin.column));
    }
    ;

WHILE_STMT:
    WHILE LPAREN EXPR RPAREN STMT { $$.reset(new WhileStmt($3.release(), $5.release(), @1.begin.line, @1.begin.column)); }
    ;

PARAM_DECLARATOR:
    TYPE IDENT {
        $$.reset(new ParamDeclarator($1, Entry::getEntry($2), nullptr, @1.begin.line, @1.begin.column));
    }
    | TYPE IDENT LBRACKET RBRACKET {
        auto* dims = new std::vector<ExprNode*>();
        dims->push_back(new LiteralExpr(-1, @3.begin.line, @3.begin.column));
        $$.reset(new ParamDeclarator($1, Entry::getEntry($2), dims, @1.begin.line, @1.begin.column));
    }
    | TYPE IDENT LBRACKET RBRACKET ARRAY_DIMENSION_EXPR_LIST {
        $5->insert($5->begin(), new LiteralExpr(-1, @3.begin.line, @3.begin.column));
        $$.reset(new ParamDeclarator($1, Entry::getEntry($2), $5.release(), @1.begin.line, @1.begin.column));
    }
    // The supplied witharray fixture also uses a sized first parameter dimension.
    | TYPE IDENT ARRAY_DIMENSION_EXPR_LIST {
        $$.reset(new ParamDeclarator($1, Entry::getEntry($2), $3.release(), @1.begin.line, @1.begin.column));
    }
    ;

PARAM_DECLARATOR_LIST:
    %empty { $$.reset(new std::vector<ParamDeclarator*>()); }
    | PARAM_DECLARATORS { $$ = std::move($1); }
    ;

PARAM_DECLARATORS:
    PARAM_DECLARATOR { $$.reset(new std::vector<ParamDeclarator*>()); $$->push_back($1.release()); }
    | PARAM_DECLARATORS COMMA PARAM_DECLARATOR { $$ = std::move($1); $$->push_back($3.release()); }
    ;

VAR_DECLARATOR:
    LEFT_VAL_EXPR { $$.reset(new VarDeclarator($1.release(), nullptr, @1.begin.line, @1.begin.column)); }
    | LEFT_VAL_EXPR ASSIGN INITIALIZER { $$.reset(new VarDeclarator($1.release(), $3.release(), @1.begin.line, @1.begin.column)); }
    ;

VAR_DECLARATOR_LIST:
    VAR_DECLARATOR { $$.reset(new std::vector<VarDeclarator*>()); $$->push_back($1.release()); }
    | VAR_DECLARATOR_LIST COMMA VAR_DECLARATOR { $$ = std::move($1); $$->push_back($3.release()); }
    ;

INITIALIZER:
    NOCOMMA_EXPR { $$.reset(new Initializer($1.release(), @1.begin.line, @1.begin.column)); }
    | LBRACE RBRACE { $$.reset(new InitializerList(new std::vector<InitDecl*>(), @1.begin.line, @1.begin.column)); }
    | LBRACE INITIALIZER_LIST RBRACE { $$.reset(new InitializerList($2.release(), @1.begin.line, @1.begin.column)); }
    ;

INITIALIZER_LIST:
    INITIALIZER { $$.reset(new std::vector<InitDecl*>()); $$->push_back($1.release()); }
    | INITIALIZER_LIST COMMA INITIALIZER { $$ = std::move($1); $$->push_back($3.release()); }
    ;

ASSIGN_EXPR:
    LEFT_VAL_EXPR ASSIGN NOCOMMA_EXPR {
        $$.reset(new BinaryExpr(Operator::ASSIGN, $1.release(), $3.release(), @1.begin.line, @1.begin.column));
    }
    ;

EXPR_LIST:
    NOCOMMA_EXPR { $$.reset(new std::vector<ExprNode*>()); $$->push_back($1.release()); }
    | EXPR_LIST COMMA NOCOMMA_EXPR { $$ = std::move($1); $$->push_back($3.release()); }
    ;

EXPR:
    NOCOMMA_EXPR { $$ = std::move($1); }
    | EXPR COMMA NOCOMMA_EXPR {
        if ($1->isCommaExpr()) {
            auto* expr = static_cast<CommaExpr*>($1.get());
            expr->exprs->push_back($3.release());
            $$ = std::move($1);
        } else {
            auto* exprs = new std::vector<ExprNode*>();
            exprs->push_back($1.release());
            exprs->push_back($3.release());
            $$.reset(new CommaExpr(exprs, @1.begin.line, @1.begin.column));
        }
    }
    ;

NOCOMMA_EXPR:
    LOGICAL_OR_EXPR { $$ = std::move($1); }
    | ASSIGN_EXPR { $$ = std::move($1); }
    ;

LOGICAL_OR_EXPR:
    LOGICAL_AND_EXPR { $$ = std::move($1); }
    | LOGICAL_OR_EXPR OR LOGICAL_AND_EXPR { $$.reset(new BinaryExpr(Operator::OR, $1.release(), $3.release(), @1.begin.line, @1.begin.column)); }
    ;

LOGICAL_AND_EXPR:
    EQUALITY_EXPR { $$ = std::move($1); }
    | LOGICAL_AND_EXPR AND EQUALITY_EXPR { $$.reset(new BinaryExpr(Operator::AND, $1.release(), $3.release(), @1.begin.line, @1.begin.column)); }
    ;

EQUALITY_EXPR:
    RELATIONAL_EXPR { $$ = std::move($1); }
    | EQUALITY_EXPR EQ RELATIONAL_EXPR { $$.reset(new BinaryExpr(Operator::EQ, $1.release(), $3.release(), @1.begin.line, @1.begin.column)); }
    | EQUALITY_EXPR NE RELATIONAL_EXPR { $$.reset(new BinaryExpr(Operator::NEQ, $1.release(), $3.release(), @1.begin.line, @1.begin.column)); }
    ;

RELATIONAL_EXPR:
    ADDSUB_EXPR { $$ = std::move($1); }
    | RELATIONAL_EXPR LT ADDSUB_EXPR { $$.reset(new BinaryExpr(Operator::LT, $1.release(), $3.release(), @1.begin.line, @1.begin.column)); }
    | RELATIONAL_EXPR GT ADDSUB_EXPR { $$.reset(new BinaryExpr(Operator::GT, $1.release(), $3.release(), @1.begin.line, @1.begin.column)); }
    | RELATIONAL_EXPR LE ADDSUB_EXPR { $$.reset(new BinaryExpr(Operator::LE, $1.release(), $3.release(), @1.begin.line, @1.begin.column)); }
    | RELATIONAL_EXPR GE ADDSUB_EXPR { $$.reset(new BinaryExpr(Operator::GE, $1.release(), $3.release(), @1.begin.line, @1.begin.column)); }
    ;

ADDSUB_EXPR:
    MULDIV_EXPR { $$ = std::move($1); }
    | ADDSUB_EXPR PLUS MULDIV_EXPR { $$.reset(new BinaryExpr(Operator::ADD, $1.release(), $3.release(), @1.begin.line, @1.begin.column)); }
    | ADDSUB_EXPR MINUS MULDIV_EXPR { $$.reset(new BinaryExpr(Operator::SUB, $1.release(), $3.release(), @1.begin.line, @1.begin.column)); }
    ;

MULDIV_EXPR:
    UNARY_EXPR { $$ = std::move($1); }
    | MULDIV_EXPR STAR UNARY_EXPR { $$.reset(new BinaryExpr(Operator::MUL, $1.release(), $3.release(), @1.begin.line, @1.begin.column)); }
    | MULDIV_EXPR SLASH UNARY_EXPR { $$.reset(new BinaryExpr(Operator::DIV, $1.release(), $3.release(), @1.begin.line, @1.begin.column)); }
    | MULDIV_EXPR MOD UNARY_EXPR { $$.reset(new BinaryExpr(Operator::MOD, $1.release(), $3.release(), @1.begin.line, @1.begin.column)); }
    ;

UNARY_EXPR:
    BASIC_EXPR { $$ = std::move($1); }
    | UNARY_OP UNARY_EXPR { $$.reset(new UnaryExpr($1, $2.release(), @1.begin.line, @1.begin.column)); }
    ;

BASIC_EXPR:
    LITERAL_EXPR { $$ = std::move($1); }
    | LEFT_VAL_EXPR { $$ = std::move($1); }
    | LPAREN EXPR RPAREN { $$ = std::move($2); }
    | FUNC_CALL_EXPR { $$ = std::move($1); }
    ;

FUNC_CALL_EXPR:
    IDENT LPAREN RPAREN {
        std::string funcName = $1;
        if (funcName != "starttime" && funcName != "stoptime")
        {
            Entry* entry = Entry::getEntry(funcName);
            $$.reset(new CallExpr(entry, nullptr, @1.begin.line, @1.begin.column));
        }
        else
        {
            funcName = "_sysy_" + funcName;
            std::vector<ExprNode*>* args = new std::vector<ExprNode*>();
            args->emplace_back(new LiteralExpr(static_cast<int>(@1.begin.line), @1.begin.line, @1.begin.column));
            $$.reset(new CallExpr(Entry::getEntry(funcName), args, @1.begin.line, @1.begin.column));
        }
    }
    | IDENT LPAREN EXPR_LIST RPAREN {
        Entry* entry = Entry::getEntry($1);
        $$.reset(new CallExpr(entry, $3.release(), @1.begin.line, @1.begin.column));
    }
    ;


ARRAY_DIMENSION_EXPR:
    LBRACKET NOCOMMA_EXPR RBRACKET { $$ = std::move($2); }
    ;

ARRAY_DIMENSION_EXPR_LIST:
    ARRAY_DIMENSION_EXPR { $$.reset(new std::vector<ExprNode*>()); $$->push_back($1.release()); }
    | ARRAY_DIMENSION_EXPR_LIST ARRAY_DIMENSION_EXPR { $$ = std::move($1); $$->push_back($2.release()); }
    ;

LEFT_VAL_EXPR:
    IDENT { $$.reset(new LeftValExpr(Entry::getEntry($1), nullptr, @1.begin.line, @1.begin.column)); }
    | IDENT ARRAY_DIMENSION_EXPR_LIST {
        $$.reset(new LeftValExpr(Entry::getEntry($1), $2.release(), @1.begin.line, @1.begin.column));
    }
    ;

LITERAL_EXPR:
    INT_CONST { $$.reset(new LiteralExpr($1, @1.begin.line, @1.begin.column)); }
    | LL_CONST { $$.reset(new LiteralExpr($1, @1.begin.line, @1.begin.column)); }
    | FLOAT_CONST { $$.reset(new LiteralExpr($1, @1.begin.line, @1.begin.column)); }
    ;

TYPE:
    INT { $$ = intType; }
    | FLOAT { $$ = floatType; }
    | VOID { $$ = voidType; }
    ;

UNARY_OP:
    PLUS { $$ = Operator::ADD; }
    | MINUS { $$ = Operator::SUB; }
    | NOT { $$ = Operator::NOT; }
    ;

%%

void FE::YaccParser::error(const FE::location& location, const std::string& message)
{
    std::cerr << "msg: " << message << ", error happened at: " << location << std::endl;
}
