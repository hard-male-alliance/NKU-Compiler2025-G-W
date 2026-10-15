# Keep target static initializers linked; ordinary static archives can drop them.
add_library(sysy_backend OBJECT
    common/cfg.cpp
    common/cfg_builder.cpp
    dag/dag_builder.cpp
    mir/m_codegen.cpp
    mir/m_defs.cpp
    ra/linear_scan.cpp
    target/registry.cpp
    targets/aarch64/aarch64_codegen.cpp
    targets/aarch64/aarch64_defs.cpp
    targets/aarch64/aarch64_instr_adapter.cpp
    targets/aarch64/aarch64_target.cpp
    targets/aarch64/dag/aarch64_dag_legalize.cpp
    targets/aarch64/isel/aarch64_dag_isel.cpp
    targets/aarch64/isel/aarch64_ir_isel.cpp
    targets/aarch64/passes/lowering/frame_lowering.cpp
    targets/aarch64/passes/lowering/phi_elimination.cpp
    targets/aarch64/passes/lowering/stack_lowering.cpp
    targets/riscv64/dag/rv64_dag_legalize.cpp
    targets/riscv64/isel/rv64_dag_isel.cpp
    targets/riscv64/isel/rv64_ir_isel.cpp
    targets/riscv64/passes/lowering/frame_lowering.cpp
    targets/riscv64/passes/lowering/phi_elimination.cpp
    targets/riscv64/passes/lowering/stack_lowering.cpp
    targets/riscv64/rv64_codegen.cpp
    targets/riscv64/rv64_defs.cpp
    targets/riscv64/rv64_instr_adapter.cpp
    targets/riscv64/rv64_target.cpp
)
target_link_libraries(sysy_backend PRIVATE sysy_options)
