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


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--fixture", required=True,
                        choices=["simple", "witharray", "withfloat"])
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="parser-", dir=args.work_dir) as directory:
        fixture(args.compiler, Path(directory), args.fixture)


if __name__ == "__main__":
    main()
