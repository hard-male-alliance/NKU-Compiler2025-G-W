"""Exercise Lab2 through the CLI: immutable AST goldens and structural properties."""

from __future__ import annotations

import argparse
from dataclasses import dataclass, field
import difflib
from pathlib import Path
import random
import re
import subprocess
import tempfile


@dataclass
class Tree:
    label: str
    children: list[Tree] = field(default_factory=list)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def run_parser(compiler: str, work: Path, source: str) -> tuple[subprocess.CompletedProcess[str], str]:
    input_path = work / "source with spaces.sy"
    output_path = work / "AST with spaces.txt"
    input_path.write_text(source, encoding="utf-8")
    result = subprocess.run(
        [compiler, "-parser", "-o", str(output_path), str(input_path)],
        capture_output=True, text=True, timeout=10,
    )
    return result, output_path.read_text(encoding="utf-8")


def valid(compiler: str, work: Path, source: str) -> str:
    result, output = run_parser(compiler, work, source)
    require(result.returncode == 0 and not result.stderr,
            f"Rejected {source!r}, exit {result.returncode}:\n{result.stderr}")
    require(output.startswith("ASTree\n"), f"Missing AST: {output}")
    return output


def tree(output: str) -> Tree:
    root = Tree("ASTree")
    stack = [root]
    for line in output.splitlines()[1:]:
        match = re.fullmatch(r"((?:\|   |    )*)(?:\|-- |`-- )(.*)", line)
        if match is None:
            raise RuntimeError(f"Malformed AST line: {line}")
        depth = len(match[1]) // 4
        label = re.sub(r",? line: \d+", "", match[2].rstrip())
        node = Tree(label)
        stack[depth].children.append(node)
        stack[depth + 1:] = [node]
    return root


def fixture(compiler: str, work: Path, name: str) -> None:
    directory = Path(__file__).resolve().parents[1] / "testcase/parser"
    actual = valid(compiler, work, (directory / f"{name}.sy").read_text(encoding="utf-8"))
    expected = (directory / f"{name}.parser").read_text(encoding="utf-8")
    a = [line.rstrip() for line in actual.splitlines()]
    e = [line.rstrip() for line in expected.splitlines()]
    diff = "\n".join(difflib.unified_diff(e, a, fromfile=f"{name}.parser", tofile="actual"))
    require(a == e, diff)
    print(f"{name}: AST matches the unchanged golden")


def expression(compiler: str, work: Path, source: str) -> Tree:
    ast = tree(valid(compiler, work, f"int main() {{ return {source}; }}"))
    return ast.children[0].children[0].children[0].children[0]


def binary(op: str, lhs: Tree, rhs: Tree) -> Tree:
    return Tree(f"BinaryExpr {op}", [lhs, rhs])


def literal(value: int) -> Tree:
    return Tree(f"literal int: {value}")


def generated(rng: random.Random, depth: int) -> tuple[str, Tree]:
    if depth == 0:
        value = rng.randrange(100)
        return str(value), literal(value)
    left, lhs = generated(rng, depth - 1)
    if rng.randrange(4) == 0:
        op = rng.choice(["+", "-", "!"])
        return f"({op}{left})", Tree(f"UnaryExpr {op}", [lhs])
    right, rhs = generated(rng, depth - 1)
    op = rng.choice(["+", "-", "*", "/", "%", "<", ">", "<=", ">=", "==", "!=", "&&", "||"])
    return f"({left}{op}{right})", binary(op, lhs, rhs)


def expression_properties(compiler: str, work: Path) -> None:
    expected = binary("||", binary("&&", binary("==", binary("<",
        binary("+", literal(1), binary("*", literal(2), literal(3))), literal(8)), literal(1)), literal(0)), literal(9))
    require(expression(compiler, work, "1+2*3<8==1&&0||9") == expected, "Operator precedence changed")
    for op in ["+", "-", "*", "/", "%", "<", ">", "<=", ">=", "==", "!=", "&&", "||"]:
        require(expression(compiler, work, f"8{op}4{op}2") ==
                binary(op, binary(op, literal(8), literal(4)), literal(2)), f"Not left-associative: {op}")
    require(expression(compiler, work, "a=b=3") == binary("=", Tree("LeftValueExpr a"),
            binary("=", Tree("LeftValueExpr b"), literal(3))), "Assignment is not right-associative")
    require(expression(compiler, work, "1,2,3") == Tree("ExprList", [literal(1), literal(2), literal(3)]),
            "Comma expression is not flattened in source order")
    require(expression(compiler, work, "f(1,(2,3),4)") == Tree("Call f", [
        Tree("Arg 0:", [literal(1)]), Tree("Arg 1:", [Tree("ExprList", [literal(2), literal(3)])]),
        Tree("Arg 2:", [literal(4)])]), "Call arguments confused with comma expressions")
    rng = random.Random(2025)
    for _ in range(50):
        source, expected = generated(rng, rng.randrange(1, 5))
        require(expression(compiler, work, source) == expected, f"Generated AST mismatch: {source}")
        spaced = source.replace("(", "( /* comment */\n").replace(")", "\t )")
        require(expression(compiler, work, spaced) == expected, f"Trivia changed structure: {source}")


def statement_properties(compiler: str, work: Path) -> None:
    source = """const int N=2;
float a[2][2]={{1,.5},{0x1p0,3}};
void f(int x[][N+1], float y[2][3]) {
    {} { int v=1; } ;
    if (1) if (2) return; else return 3;
    while (1) { if (0) break; continue; }
    for (;;) ;
    for (int i=0; i<N; i=i+1) { a[i][0]=i; }
    for (a[0][0]=0; 1; a[0][0]=1) break;
    starttime(); stoptime();
}
"""
    ast = tree(valid(compiler, work, source))
    array = ast.children[1].children[0].children[0]
    require(array.children[0] == Tree("Var:", [Tree("LeftValueExpr a", [literal(2), literal(2)])]),
            "Array dimensions lost source order")
    require(array.children[1] == Tree("Init:", [Tree("InitializerList", [
        Tree("InitializerList", [Tree("Initializer", [literal(1)]),
                                 Tree("Initializer", [Tree("literal float: 0.500000")])]),
        Tree("InitializerList", [Tree("Initializer", [Tree("literal float: 1.000000")]),
                                 Tree("Initializer", [literal(3)])]),
    ])]), "Nested initialization shape or source order changed")
    func = ast.children[2]
    require(func.label == "FuncDecl f(int x[][expr], float y[2][3]) -> void", "Bad parameter dimensions")
    require(func.children[0] == Tree("ParamDeclarator: int x", [Tree("literal int: -1"),
            binary("+", Tree("LeftValueExpr N"), literal(1))]), "Symbolic dimensions lost")
    body = func.children[-1]
    require(body.children[0] == Tree("BlockStmt"), "Empty block was erased")
    require(body.children[1].label == "BlockStmt", "Single-item scope was erased")
    outer = body.children[2]
    require([node.label for node in outer.children] == ["Condition:", "Then:"], "Else attached to outer if")
    inner = outer.children[1].children[0]
    require([node.label for node in inner.children] == ["Condition:", "Then:", "Else:"], "Dangling else misbound")
    require(body.children[4] == Tree("ForStmt"), "Empty for clauses were not represented as absent")
    require(body.children[-2].children[0] == Tree("Call _sysy_starttime", [Tree("Arg 0:", [literal(10)])]),
            "starttime did not receive the source line")
    require(body.children[-1].children[0] == Tree("Call _sysy_stoptime", [Tree("Arg 0:", [literal(10)])]),
            "stoptime did not receive the source line")
    require(expression(compiler, work, "-2147483648") == Tree("UnaryExpr -", [Tree("literal long long: 2147483648")]),
            "Large integer payload was truncated")
    require(expression(compiler, work, "0x1p-1") == Tree("literal float: 0.500000"), "Float payload changed")
    valid(compiler, work, "int main(){int a[2][2]={}; return a[0][1];}")


def rejection_properties(compiler: str, work: Path) -> None:
    invalid = ["", "int main( {", "int main(){ return 1 }", "int main(){int a[2;}",
        "int main(){int a={1,};}", "int main(){f(,1);}", "int main(){if(1) else ;}",
        "int main(){(1+2)=3;}", "int main(){return 0;} int broken(",
        "int main(){} @", "int main(){} /* open", "int main(){return 09;}",
        "return 1;", "int main(){int nested(){return 0;}}", "int main(){if(1) int a;}"]
    for source in invalid:
        result, output = run_parser(compiler, work, source)
        require(result.returncode != 0 and bool(result.stderr) and not output,
                f"Invalid input produced success or partial AST: {source!r}\n{result.stderr}\n{output}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--fixture", choices=["simple", "witharray", "withfloat"])
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="parser-", dir=args.work_dir) as directory:
        work = Path(directory)
        if args.fixture:
            fixture(args.compiler, work, args.fixture)
        else:
            expression_properties(args.compiler, work)
            statement_properties(args.compiler, work)
            rejection_properties(args.compiler, work)
            print("Precedence, associativity, 50 generated/trivia pairs, scopes, arrays, timing, and 15 invalid inputs passed")


if __name__ == "__main__":
    main()
