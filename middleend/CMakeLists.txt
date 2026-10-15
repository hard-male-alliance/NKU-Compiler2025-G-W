# Explicit sources make adding an optimization an intentional build change.
add_library(sysy_middleend OBJECT
    module/ir_block.cpp
    module/ir_function.cpp
    module/ir_instruction.cpp
    module/ir_module.cpp
    module/ir_operand.cpp
    pass/analysis/analysis_manager.cpp
    pass/analysis/cfg.cpp
    pass/analysis/dominfo.cpp
    pass/unify_return.cpp
    visitor/codegen/ast_codegen.cpp
    visitor/codegen/decl_codegen.cpp
    visitor/codegen/expr_codegen.cpp
    visitor/codegen/stmt_codegen.cpp
    visitor/codegen/type_convert.cpp
    visitor/printer/inst_printer.cpp
    visitor/printer/module_printer.cpp
    visitor/utils/rename_visitor.cpp
)
target_link_libraries(sysy_middleend PRIVATE sysy_options)
