find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)

# Exercise the rebuilt C runtime through an executable, not just archive creation.
add_executable(runtime_smoke runtime_smoke.c)
target_link_libraries(runtime_smoke PRIVATE sysy_runtime)

# The legacy backend uses static registration; verify neither target is discarded.
add_executable(registry_smoke registry_smoke.cpp)
target_link_libraries(registry_smoke PRIVATE sysy_options sysy_interfaces sysy_utils
    sysy_frontend sysy_middleend sysy_backend)
add_dependencies(registry_smoke sysy_parser_generated)
add_test(NAME backend-registration COMMAND registry_smoke)

# Explicit NDEBUG covers fatal TODO/ERROR semantics even in a Debug configuration.
add_executable(diagnostic_smoke diagnostic_smoke.cpp)
target_link_libraries(diagnostic_smoke PRIVATE sysy_options)
target_compile_definitions(diagnostic_smoke PRIVATE NDEBUG)

add_test(NAME build-user-journey
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/build_smoke.py"
        --compiler "$<TARGET_FILE:compiler>"
        --runtime "$<TARGET_FILE:runtime_smoke>"
        --diagnostic "$<TARGET_FILE:diagnostic_smoke>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}"
)
set_tests_properties(backend-registration build-user-journey PROPERTIES TIMEOUT 30)

# Keep the supplied Lab1 expected outputs immutable and compare all columns.
foreach(fixture IN ITEMS simple witharray withfloat)
    add_test(NAME lexer-fixture-${fixture}
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/lexer_test.py"
            --compiler "$<TARGET_FILE:compiler>" --work-dir "${CMAKE_CURRENT_BINARY_DIR}"
            --fixture "${fixture}")
    set_tests_properties(lexer-fixture-${fixture} PROPERTIES TIMEOUT 15)
endforeach()
add_test(NAME lexer-boundaries
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/lexer_test.py"
        --compiler "$<TARGET_FILE:compiler>" --work-dir "${CMAKE_CURRENT_BINARY_DIR}"
        --boundaries)
set_tests_properties(lexer-boundaries PROPERTIES TIMEOUT 45)

add_executable(lexer_state_smoke lexer_state_smoke.cpp)
target_link_libraries(lexer_state_smoke PRIVATE sysy_options sysy_interfaces sysy_utils
    sysy_frontend sysy_middleend sysy_backend)
add_dependencies(lexer_state_smoke sysy_parser_generated)
add_test(NAME lexer-state COMMAND lexer_state_smoke)
set_tests_properties(lexer-state PROPERTIES TIMEOUT 10)

foreach(fixture IN ITEMS simple witharray withfloat)
    add_test(NAME parser-fixture-${fixture}
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/parser_test.py"
            --compiler "$<TARGET_FILE:compiler>" --work-dir "${CMAKE_CURRENT_BINARY_DIR}"
            --fixture "${fixture}")
    set_tests_properties(parser-fixture-${fixture} PROPERTIES TIMEOUT 15)
endforeach()

# Host-only developers need no cross tools; a complete WSL environment gets E2E checks.
find_program(SYSY_TEST_CLANG NAMES clang)
find_program(SYSY_TEST_LLVM_AS NAMES llvm-as)
find_program(SYSY_TEST_QEMU_ARM NAMES qemu-aarch64)
find_program(SYSY_TEST_QEMU_RISCV NAMES qemu-riscv64)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND SYSY_TEST_CLANG AND SYSY_TEST_LLVM_AS
        AND SYSY_TEST_QEMU_ARM AND SYSY_TEST_QEMU_RISCV
        AND TARGET sysy_runtime_aarch64 AND TARGET sysy_runtime_riscv64)
    add_test(NAME toolchain-user-journey
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/toolchain_smoke.py"
            --build-dir "${PROJECT_BINARY_DIR}")
    set_tests_properties(toolchain-user-journey PROPERTIES TIMEOUT 90)
endif()
