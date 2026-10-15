cmake_minimum_required(VERSION 3.25)
project(NKUCompiler VERSION 0.1.0 LANGUAGES C CXX)

# Keep generated parser files and configuration-specific artifacts out of sources.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Use an out-of-source build: cmake --preset default")
endif()

# Prefer C++23; an explicit fallback never silently weakens the selected contract.
set(CMAKE_CXX_STANDARD 23 CACHE STRING "C++ language standard (23, 20, or 17)")
set_property(CACHE CMAKE_CXX_STANDARD PROPERTY STRINGS 23 20 17)
if(NOT CMAKE_CXX_STANDARD MATCHES "^(23|20|17)$")
    message(FATAL_ERROR "Select CMAKE_CXX_STANDARD=23, 20, or 17")
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SYSY_WARNINGS_AS_ERRORS "Treat project warnings as errors" ON)
option(SYSY_LOCAL_TEST "Enable the original LOCAL_TEST definition" ON)

# All compiler components share one explicit language/include/diagnostic contract.
add_library(sysy_options INTERFACE)
target_compile_features(sysy_options INTERFACE cxx_std_${CMAKE_CXX_STANDARD})
target_include_directories(sysy_options INTERFACE
    "${PROJECT_BINARY_DIR}/generated"
    "${PROJECT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/interfaces"
    "${PROJECT_SOURCE_DIR}/utils"
    "${PROJECT_SOURCE_DIR}/frontend/parser"
)
target_compile_definitions(sysy_options INTERFACE $<$<BOOL:${SYSY_LOCAL_TEST}>:LOCAL_TEST>)
target_compile_options(sysy_options INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic;-Wno-unused-parameter>
    $<$<AND:$<BOOL:${SYSY_WARNINGS_AS_ERRORS}>,$<CXX_COMPILER_ID:GNU,Clang,AppleClang>>:-Werror>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<AND:$<BOOL:${SYSY_WARNINGS_AS_ERRORS}>,$<CXX_COMPILER_ID:MSVC>>:/WX>
)

add_subdirectory(interfaces)
add_subdirectory(utils)
add_subdirectory(frontend)
add_subdirectory(middleend)
add_subdirectory(backend)
add_subdirectory(lib)

add_executable(compiler main.cpp)
target_link_libraries(compiler PRIVATE sysy_options sysy_interfaces sysy_utils
    sysy_frontend sysy_middleend sysy_backend)
add_dependencies(compiler sysy_parser_generated)
set_target_properties(compiler PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")

# Only the compatibility target publishes into bin; normal builds stay isolated.
add_custom_target(legacy-compiler
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${PROJECT_SOURCE_DIR}/bin"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:compiler>" "${PROJECT_SOURCE_DIR}/bin/$<TARGET_FILE_NAME:compiler>"
    DEPENDS compiler sysy_runtimes
    COMMENT "Publishing compiler for the existing test.py and debugger paths"
    VERBATIM
)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
