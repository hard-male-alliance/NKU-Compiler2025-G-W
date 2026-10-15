# Rebuild the host runtime without overwriting the checked-in teaching archives.
add_library(sysy_runtime STATIC sylib.c)
# Build available runtimes together so the default testing workflow is ready.
add_custom_target(sysy_runtimes ALL)
add_dependencies(sysy_runtimes sysy_runtime)
target_include_directories(sysy_runtime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(sysy_runtime PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
    OUTPUT_NAME sysy_x86
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

# Linux-targeting compilers provide the libc needed by sylib and QEMU user mode.
# Override these cache paths to use a compatible alternative toolchain.
find_program(SYSY_AARCH64_CC NAMES aarch64-linux-gnu-gcc)
find_program(SYSY_AARCH64_AR NAMES aarch64-linux-gnu-ar)
find_program(SYSY_RISCV_CC NAMES riscv64-linux-gnu-gcc)
find_program(SYSY_RISCV_AR NAMES riscv64-linux-gnu-ar)

# Build a target runtime without changing the host compiler or checked-in archives.
function(add_sysy_cross_runtime name compiler archiver archive)
    set(object "${CMAKE_CURRENT_BINARY_DIR}/${name}.o")
    set(output "${CMAKE_CURRENT_BINARY_DIR}/${archive}")
    add_custom_command(OUTPUT "${output}"
        COMMAND "${compiler}" -std=c11 -I "${CMAKE_CURRENT_SOURCE_DIR}"
            ${ARGN} -c "${CMAKE_CURRENT_SOURCE_DIR}/sylib.c" -o "${object}"
        COMMAND "${archiver}" rcs "${output}" "${object}"
        DEPENDS sylib.c sylib.h
        BYPRODUCTS "${object}"
        COMMENT "Building ${name} with ${compiler}"
        VERBATIM
    )
    add_custom_target(${name} DEPENDS "${output}")
endfunction()

if(SYSY_AARCH64_CC AND SYSY_AARCH64_AR)
    add_sysy_cross_runtime(sysy_runtime_aarch64 "${SYSY_AARCH64_CC}"
        "${SYSY_AARCH64_AR}" libsysy_aarch.a)
    add_dependencies(sysy_runtimes sysy_runtime_aarch64)
endif()
if(SYSY_RISCV_CC AND SYSY_RISCV_AR)
    add_sysy_cross_runtime(sysy_runtime_riscv64 "${SYSY_RISCV_CC}"
        "${SYSY_RISCV_AR}" libsysy_riscv.a -mcmodel=medany)
    add_dependencies(sysy_runtimes sysy_runtime_riscv64)
endif()
