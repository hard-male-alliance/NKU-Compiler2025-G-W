# Object libraries preserve every translation unit, including registration code.
add_library(sysy_interfaces OBJECT
    frontend/ast/ast_defs.cpp
    frontend/symbol/symbol_entry.cpp
    middleend/ir_defs.cpp
    middleend/pass.cpp
)
target_link_libraries(sysy_interfaces PRIVATE sysy_options)
