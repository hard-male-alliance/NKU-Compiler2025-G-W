#!/bin/sh
# Install the host and Linux cross-compilation tools used by this repository.
# Usage on Ubuntu 24.04: sudo sh scripts/setup-wsl.sh
set -eu

if [ "$(id -u)" -ne 0 ]; then
    echo "Run with sudo sh scripts/setup-wsl.sh (or WSL's root user)." >&2
    exit 1
fi

export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build bison flex libfl-dev \
    clang clang-format llvm python3 \
    gdb gdb-multiarch valgrind qemu-user \
    gcc-aarch64-linux-gnu libc6-dev-arm64-cross \
    gcc-riscv64-linux-gnu libc6-dev-riscv64-cross

# Bare-metal RISC-V is not required: sylib uses Linux libc and QEMU user mode.
echo "WSL compiler environment installed. Next: cmake --preset default"
