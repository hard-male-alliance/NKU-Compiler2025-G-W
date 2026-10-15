find_package(BISON 3.2 REQUIRED)
find_package(FLEX 2.6 REQUIRED)

# Retain the existing frontend/parser/... include paths without source generation.
set(parser_dir "${PROJECT_BINARY_DIR}/generated/frontend/parser")
file(MAKE_DIRECTORY "${parser_dir}")
# FindBISON's helper does not declare the C++ location header as an output.
# Declare all three outputs so deleting location.hh correctly reruns Bison.
set(parser_outputs "${parser_dir}/yacc.cpp" "${parser_dir}/yacc.h" "${parser_dir}/location.hh")
add_custom_command(OUTPUT ${parser_outputs}
    COMMAND "${BISON_EXECUTABLE}" --language=c++
        "--defines=${parser_dir}/yacc.h" "--output=${parser_dir}/yacc.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/parser/yacc.y"
    DEPENDS parser/yacc.y
    COMMENT "Generating the SysY parser and location header with Bison"
    VERBATIM
)
flex_target(SysYLexer parser/lexer.l "${parser_dir}/lexer.cpp")
add_custom_command(OUTPUT ${FLEX_SysYLexer_OUTPUTS} APPEND DEPENDS "${parser_dir}/yacc.h")
# Bison 3.8's C++ skeleton has an unused error counter under Clang/NDEBUG.
# Scope this workaround to generated code; keep -Werror on handwritten sources.
set_source_files_properties("${parser_dir}/yacc.cpp" PROPERTIES COMPILE_OPTIONS
    "$<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wno-unused-but-set-variable>")
add_custom_target(sysy_parser_generated
    DEPENDS ${parser_outputs} ${FLEX_SysYLexer_OUTPUTS})

add_library(sysy_frontend OBJECT
    ast/ast.cpp
    ast/decl.cpp
    ast/expr.cpp
    ast/stmt.cpp
    ast/visitor/printer/ast_printer.cpp
    ast/visitor/printer/decl_printer.cpp
    ast/visitor/printer/expr_printer.cpp
    ast/visitor/printer/stmt_printer.cpp
    ast/visitor/sementic_check/ast_checker.cpp
    ast/visitor/sementic_check/decl_checker.cpp
    ast/visitor/sementic_check/expr_checker.cpp
    ast/visitor/sementic_check/stmt_checker.cpp
    ast/visitor/sementic_check/type_infer.cpp
    parser/parser.cpp
    symbol/symbol_table.cpp
    ${parser_outputs}
    ${FLEX_SysYLexer_OUTPUTS}
)
target_link_libraries(sysy_frontend PRIVATE sysy_options)
add_dependencies(sysy_frontend sysy_parser_generated)
