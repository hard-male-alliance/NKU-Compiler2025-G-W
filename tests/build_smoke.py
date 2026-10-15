"""Test build integration without claiming the unfinished SysY grammar is complete."""

import argparse
from pathlib import Path
import subprocess
import tempfile


def run(command, **kwargs):
    """Capture a bounded invocation so broken CLI behavior fails the test promptly."""
    return subprocess.run(command, capture_output=True, text=True, timeout=5, **kwargs)


def require(condition, message):
    """Raise an actionable failure independently of Python assertion settings."""
    if not condition:
        raise RuntimeError(message)


def main():
    """Cover error exits, file-based lexing, runtime I/O and release diagnostics."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--runtime", required=True)
    parser.add_argument("--diagnostic", required=True)
    parser.add_argument("--work-dir", required=True)
    args = parser.parse_args()

    # Fatal diagnostics intentionally abort; keep core files out of the workspace.
    try:
        import resource
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    except ImportError:
        pass

    result = run([args.compiler])
    require(result.returncode == 1 and "No input file" in result.stderr,
            f"No-input contract changed: {result}")
    result = run([args.compiler, "--not-an-option"])
    require(result.returncode == 1 and "Unknown option" in result.stderr,
            f"Unknown-option contract changed: {result}")
    result = run([args.compiler, "-o"])
    require(result.returncode == 1 and "requires a filename" in result.stderr,
            f"Missing-output contract changed: {result}")

    with tempfile.TemporaryDirectory(dir=args.work_dir, prefix="cli-") as directory:
        root = Path(directory)
        result = run([args.compiler, str(root / "missing.sy")])
        require(result.returncode == 1 and "Cannot open input file" in result.stderr,
                f"Missing-input contract changed: {result}")
        source = root / "tokens with spaces.sy"
        output = root / "tokens with spaces.out"
        source.write_text("if else while continue break return const abc 123 ()[]{};,\n",
                          encoding="utf-8")
        result = run([args.compiler, "-lexer", "-o", str(output), str(source)])
        require(result.returncode == 0, f"Lexer invocation failed: {result}")
        rows = output.read_text(encoding="utf-8").splitlines()
        expected = ["IF", "ELSE", "WHILE", "CONTINUE", "BREAK", "RETURN", "CONST",
                    "IDENT", "INT_CONST", "LPAREN", "RPAREN", "LBRACKET", "RBRACKET",
                    "LBRACE", "RBRACE", "SEMICOLON", "COMMA"]
        actual = [row.split()[0] for row in rows[1:]]
        require(actual == expected, f"Unexpected lexer tokens: {actual}")
        require(any("abc" in row for row in rows) and any("123" in row for row in rows),
                "Lexer lost identifier or integer attributes")

    result = run([args.runtime], input="41\n")
    require(result.returncode == 0 and result.stdout == "42\n",
            f"Rebuilt runtime I/O failed: {result}")
    require("Timer@" in result.stderr and "TOTAL:" in result.stderr,
            f"Runtime timing state was not exercised: {result}")
    for extra, marker in (([], "TODO:"), (["error"], "ERROR:")):
        result = run([args.diagnostic, *extra])
        require(result.returncode != 0 and marker in result.stderr,
                f"NDEBUG diagnostic fell through: {result}")
    print("CLI, lexer, runtime I/O and NDEBUG fatal diagnostics passed")


if __name__ == "__main__":
    main()
