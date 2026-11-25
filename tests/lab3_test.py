"""Compile, link, and run Lab3 programs through the public CLI."""

from __future__ import annotations

import argparse
from pathlib import Path
import random
import signal
import subprocess
import tempfile
from typing import Optional


ROOT = Path(__file__).resolve().parents[1]


def run(argv: list[str], cwd: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(argv, cwd=cwd, text=True, capture_output=True, timeout=30)


def rejected(compiler: str, work: Path, name: str, source: Path, diagnostic: str) -> None:
    output = work / (name + ".ll")
    p = run([compiler, "-llvm", str(source), "-o", str(output)], work)
    assert p.returncode == 1, (name, p.returncode, p.stderr)
    assert diagnostic in p.stderr, (name, p.stderr)
    assert output.read_bytes() == b"", name


def negative(compiler: str, work: Path) -> None:
    for path in sorted((ROOT / "testcase" / "semant").glob("*.sy")):
        rejected(compiler, work, path.stem, path, "Semantic check failed")

    cases = {
        "no_main": ("int f() { return 0; }", "missing main"),
        "break": ("int main() { break; }", "break outside loop"),
        "rank": ("int main() { tensor int a[2][2]; return a[0]; }", "number of subscripts"),
        "extent": ("int main() { tensor int a[0]; return 0; }", "positive integer"),
        "nonconstant_extent": ("int main() { int n=2; tensor int a[n]; return 0; }", "integer constant"),
        "float_extent": ("int main() { tensor int a[2.0]; return 0; }", "integer constant"),
        "overflow_init": ("int main() { tensor int a[2][2]={{1,2,3}}; return 0; }", "too many initializers"),
        "rank_init": ("int main() { tensor int a[1]={{{1}}}; return 0; }", "braces"),
        "scalar_init": ("int main() { tensor int a[1]=1; return 0; }", "initializer"),
        "readonly": ("int main() { const tensor int a[1]={1}; a[0]=0; return 0; }", "not writable"),
        "assign": ("int main() { tensor int a[2], b[3]; a=b; return 0; }", "identical tensor"),
        "precision": ("int main() { tensor int a[2]; a+1.0; return 0; }", "same element type"),
        "hadamard": ("int main() { tensor int a[2], b[3]; a*b; return 0; }", "shapes do not match"),
        "matmul": ("int main() { tensor int a[2][3], b[2][2]; a@b; return 0; }", "MxN"),
        "floatmod": ("int main() { tensor float a[2]; a%1.0; return 0; }", "integer elements"),
        "division": ("int main() { tensor int a[2]; a/(1-1); return 0; }", "zero divisor"),
        "comparison": ("int main() { tensor int a[2]; return a==a; }", "does not accept tensors"),
        "logical": ("int main() { tensor int a[2]; return !a; }", "invalid unary operand"),
        "condition": ("int main() { tensor int a[2]; if(a) return 1; return 0; }", "numeric scalar"),
        "parameter": ("void f(tensor int a[2]) {} int main() { return 0; }", "empty first dimension"),
        "argument": ("void f(tensor int a[][3]) {} int main() { tensor int a[2][4]; f(a); return 0; }",
                     "argument 1 has incompatible"),
        "array_argument": ("void f(tensor int a[]) {} int main() { int a[2]; f(a); return 0; }",
                           "argument 1 has incompatible"),
        "return_shape": ("tensor int f(int x) { tensor int a[2], b[3]; if(x)return a; return b; }"
                         "int main() { return 0; }", "incompatible tensor return shape"),
    }
    for name, (source, expected) in cases.items():
        path = work / (name + ".sy")
        path.write_text(source)
        rejected(compiler, work, name, path, expected)


def executable(compiler: str, clang: str, runtime: Path, work: Path, name: str,
               source: str, opt: int) -> Path:
    path = work / (name + ".sy")
    ir = path.with_suffix(".ll")
    exe = work / name
    path.write_text(source)
    p = run([compiler, "-llvm", str(path), "-o", str(ir), f"-O{opt}"], work)
    assert p.returncode == 0, (name, p.stderr)
    p = run([clang, "-Wno-override-module", f"-O{opt}", str(ir), str(runtime), "-o", str(exe)], work)
    assert p.returncode == 0, (name, p.stderr)
    return exe


def tensor_journey(compiler: str, clang: str, runtime: Path, work: Path) -> None:
    source = """
const int K = 3;
const tensor int weights[3][2] = {{7,8},{9,10},{11,12}};
const tensor int identity[2][2] = {{1}, {0,1}};
tensor int globalProd[2][2] = identity @ identity + 2;
const tensor int globals[2][2] = identity + 3;
int touched = 0;

tensor int product(tensor int a[][K], tensor int b[][2]) {
    return a @ b;
}
tensor int recurse(int n, tensor int a[][2]) {
    if (n) return recurse(n-1, a) + 1;
    return a;
}
int read(tensor int a[][2]) { return a[0][0] + a[1][1]; }
int effect(int value) { touched=touched+1; return value; }
int never(tensor int a[][3]) {
    int zero = 0;
    if (zero && read(product(a, weights))) return 90;
    return touched;
}
void grow(tensor int a[]) {
    for (int i=0; i<600; i=i+1) a=a+1;
}
void touch(tensor int a[]) { a[0] = 99; }
int tensor(int tensor) { return tensor; }
int main() {
    tensor int a[2][3] = {{1,2,3}, {4,5,6}};
    tensor int r[2][2] = product(a, weights);
    r = recurse(2, r);
    putint(read(r) + globalProd[0][0]); putch(10);
    tensor int v[3] = {2,4,6}, w[3] = {8,12,16};
    tensor int q[3] = -(w / v) + (w % 5) + (effect(2) * +v);
    putint(q[0]*100+q[1]*10+q[2]); putch(10);
    putint(never(a)); putch(10);
    tensor float fa[2][3] = {0.5, 1.0, 1.5, 2.0};
    tensor float fb[3][1] = {2.0, 4.0, 6.0};
    tensor float fc[2][1] = fa @ fb;
    fc = 10.0 - fc * 0.5;
    putint(fc[0][0]*10 + fc[1][0]); putch(10);
    int dims[globals[1][1]] = {0,1,2,3};
    putint(dims[3]); putch(10);
    tensor int large[4096] = {};
    grow(large);
    touch(+large);
    putint(large[0] + large[4095]); putch(10);
    tensor float finf[1] = {1.0};
    tensor float zeros[1] = {-0.0};
    finf = finf / zeros;
    putint(finf[0] < 0.0); putch(10);
    return tensor(0);
}
"""
    for opt in (0, 1):
        exe = executable(compiler, clang, runtime, work, f"tensor journey {opt}", source, opt)
        p = run([str(exe)], work)
        assert (p.returncode, p.stdout) == (0, "219\n381\n1\n38\n3\n1200\n1\n"), p


def incompatible_dynamic_extents(compiler: str, clang: str, runtime: Path, work: Path) -> None:
    source = """
void add(tensor int a[], tensor int b[]) { a = a + b; }
int main() {
    tensor int a[2] = {}, b[3] = {};
    add(a,b);
    return 0;
}
"""
    exe = executable(compiler, clang, runtime, work, "runtime shape mismatch", source, 0)
    p = run([str(exe)], work)
    assert p.returncode in (-signal.SIGILL, -signal.SIGTRAP), p


def generated_products(compiler: str, clang: str, runtime: Path, work: Path) -> None:
    rng = random.Random(314159)
    expected: list[str] = []
    parts = ["tensor int mul(tensor int a[][3], tensor int b[][2]) { return a@b; }", "int main() {"]
    for case in range(24):
        rows = rng.randrange(1, 8)
        a = [rng.randrange(-8, 9) for _ in range(rows * 3)]
        b = [rng.randrange(-8, 9) for _ in range(6)]
        lhs, rhs = ",".join(map(str, a)), ",".join(map(str, b))
        parts.append(f"{{ tensor int a[{rows}][3]={{{lhs}}}, b[3][2]={{{rhs}}};")
        parts.append(f"tensor int c[{rows}][2] = mul(a,b);")
        for i in range(rows):
            for j in range(2):
                expected.append(str(sum(a[i * 3 + k] * b[k * 2 + j] for k in range(3))))
                parts.append(f"putint(c[{i}][{j}]); putch(10);")
        parts.append("}")
    parts.append("return 0; }")
    exe = executable(compiler, clang, runtime, work, "generated products", "\n".join(parts), 0)
    p = run([str(exe)], work)
    assert p.returncode == 0 and p.stdout.splitlines() == expected, p


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    parser.add_argument("--clang", type=Path)
    parser.add_argument("--runtime", type=Path)
    args = parser.parse_args()
    clang: Optional[Path] = args.clang
    runtime: Optional[Path] = args.runtime
    compiler = str(args.compiler.resolve())
    args.work_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="lab3-", dir=args.work_dir.resolve()) as tmp:
        work = Path(tmp)
        if clang is not None and runtime is not None:
            tensor_journey(compiler, str(clang.resolve()), runtime.resolve(), work)
            generated_products(compiler, str(clang.resolve()), runtime.resolve(), work)
            incompatible_dynamic_extents(compiler, str(clang.resolve()), runtime.resolve(), work)
        else:
            negative(compiler, work)


if __name__ == "__main__":
    main()
