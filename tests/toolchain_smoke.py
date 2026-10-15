"""Exercise installed LLVM/QEMU tools and existing link/test drivers end to end."""

import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def run(command, **kwargs):
    """Capture a bounded tool invocation with actionable errors."""
    result = subprocess.run(command, capture_output=True, text=True, timeout=15, **kwargs)
    if result.returncode:
        raise RuntimeError(f"{command}: {result.stderr}")
    return result


def main():
    """Link known inputs, bypassing unfinished SysY codegen but using real drivers."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", required=True, type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    build = args.build_dir.resolve()
    # Import the legacy driver without leaving __pycache__ in the source root.
    sys.dont_write_bytecode = True
    sys.path.insert(0, str(root))
    # The existing batch driver must work with the new runtime locations too.
    import test as driver

    driver.RUNTIME_DIR = str(build / "lib")
    environment = dict(os.environ, SYSY_RUNTIME_DIR=str(build / "lib"))
    with tempfile.TemporaryDirectory(dir=build, prefix="toolchains-") as directory:
        work = Path(directory)
        llvm = work / "runtime.ll"
        llvm.write_text(
            "declare i32 @getint()\ndeclare void @putint(i32)\n"
            "declare void @putch(i32)\ndefine i32 @main() {\n"
            "  %v = call i32 @getint()\n  %r = add i32 %v, 1\n"
            "  call void @putint(i32 %r)\n  call void @putch(i32 10)\n"
            "  ret i32 0\n}\n", encoding="utf-8")
        run(["llvm-as", str(llvm), "-o", str(work / "runtime.bc")])
        inputs = {"llvm": llvm}
        for stage, compiler in (("arm", "aarch64-linux-gnu-gcc"),
                                ("riscv", "riscv64-linux-gnu-gcc")):
            assembly = work / f"runtime-{stage}.s"
            run([compiler, "-S", str(root / "tests/runtime_smoke.c"),
                 f"-I{root / 'lib'}", "-o", str(assembly)])
            inputs[stage] = assembly

        for stage, helper, runner in (("llvm", "ll2bin.sh", []),
                                       ("arm", "arm2bin.sh", ["qemu-aarch64"]),
                                       ("riscv", "rv2bin.sh", ["qemu-riscv64"])):
            binary = work / f"{stage}.bin"
            run(["bash", str(root / helper), str(inputs[stage]), str(binary)],
                cwd=root, env=environment)
            result = run([*runner, str(binary)], input="41\n")
            if result.stdout != "42\n":
                raise RuntimeError(f"{helper} produced {result.stdout!r}")

        # Keep the old driver's fixed tmp.o/tmp.bin inside this isolated build folder.
        stdin = work / "input.txt"
        expected = work / "expected.txt"
        stdin.write_text("41\n", encoding="utf-8")
        expected.write_text("42\n0\n", encoding="utf-8")
        os.chdir(work)
        for stage, linker, runner in (
            ("llvm", driver._compile_ir_and_link, driver._run_ir_and_check),
            ("arm", driver._compile_asm_and_link_arm, driver._run_arm_and_check),
            ("riscv", driver._compile_asm_and_link_riscv, driver._run_riscv_and_check),
        ):
            source = str(inputs[stage])
            if not linker(source, source, stage):
                raise RuntimeError(f"test.py {stage} linking failed")
            config = driver.TestConfig(source, source, 0, str(stdin), str(expected),
                                       str(work / f"{stage}.actual"))
            if not runner(config, stage):
                raise RuntimeError(f"test.py {stage} output/exit-code comparison failed")
        os.chdir(root)

        invalid = work / "invalid.s"
        invalid.write_text("not_an_assembly_instruction\n", encoding="utf-8")
        result = subprocess.run(
            ["bash", str(root / "rv2bin.sh"), str(invalid), str(work / "invalid.bin")],
            cwd=root, env=environment, capture_output=True, text=True, timeout=15)
        if result.returncode == 0 or "Successfully" in result.stdout:
            raise RuntimeError("rv2bin.sh hid an assembler failure")
    print("LLVM, both QEMU targets, link helpers and test.py runtime paths passed")


if __name__ == "__main__":
    main()
