# Compatibility entry points; CMake owns source lists and dependency tracking.
.DEFAULT_GOAL := all
CMAKE ?= cmake
PRESET ?= default
JOBS ?= 4
CMAKE_ARGS ?=
BUILD_DIR := .cache/build/$(PRESET)

# Respect an explicit compiler without inheriting make's built-in g++.
ifneq ($(filter command line environment override,$(origin CXX)),)
CMAKE_ARGS += -DCMAKE_CXX_COMPILER="$(CXX)"
endif

.PHONY: all configure test clean clean-lexer lexer format libarm librv

all: configure
	$(CMAKE) --build --preset $(PRESET) --parallel $(JOBS) --target legacy-compiler

configure:
	$(CMAKE) --preset $(PRESET) $(CMAKE_ARGS)

test: all
	$(CMAKE) --build --preset $(PRESET) --parallel $(JOBS)
	ctest --preset $(PRESET)

# Build-system clean preserves caches and never removes source directories.
clean:
	@if [ -f "$(BUILD_DIR)/CMakeCache.txt" ]; then $(CMAKE) --build "$(BUILD_DIR)" --target clean; fi
	$(CMAKE) -E rm -f bin/compiler

lexer: configure
	$(CMAKE) --build --preset $(PRESET) --target sysy_parser_generated

clean-lexer:
	$(CMAKE) -E rm -f $(addprefix $(BUILD_DIR)/generated/frontend/parser/,lexer.cpp yacc.cpp yacc.h location.hh)

# Restrict formatting to project sources, not generated build trees.
format:
	@find frontend middleend backend interfaces utils lib tests -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.c' -o -name '*.hpp' -o -name '*.hh' \) -exec clang-format -i {} +
	clang-format -i main.cpp

# Preserve explicit legacy runtime publishing; CMake builds stay isolated.
libarm: configure
	$(CMAKE) --build --preset $(PRESET) --target sysy_runtime_aarch64
	$(CMAKE) -E copy_if_different $(BUILD_DIR)/lib/libsysy_aarch.a lib/libsysy_aarch.a

librv: configure
	$(CMAKE) --build --preset $(PRESET) --target sysy_runtime_riscv64
	$(CMAKE) -E copy_if_different $(BUILD_DIR)/lib/libsysy_riscv.a lib/libsysy_riscv.a
