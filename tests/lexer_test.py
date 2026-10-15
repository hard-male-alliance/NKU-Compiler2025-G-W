"""Check immutable Lab1 goldens and independent SysY2022 lexical boundary cases."""

import argparse
import difflib
import math
import random
from pathlib import Path
import subprocess
import tempfile


def require(condition, message):
    """Fail even when Python assertions have been disabled."""
    if not condition:
        raise RuntimeError(message)


def scan(compiler, work, text):
    """Invoke the real lexer CLI and capture its output separately from diagnostics."""
    source = work / "source with spaces.sy"
    output = work / "tokens with spaces.txt"
    source.write_bytes(text.encode("utf-8") if isinstance(text, str) else text)
    result = subprocess.run([compiler, "-lexer", "-o", str(output), str(source)],
                            capture_output=True, text=True, timeout=5)
    return result, output.read_text(encoding="utf-8")


def rows(table):
    """Read the established fixed-width table without losing escaped string spaces."""
    result = []
    for row in table.splitlines()[1:]:
        result.append((row[:30].strip(), row[30:60].strip(), row[60:90].strip(),
                       int(row[90:98]), int(row[98:106])))
    return result


def valid(compiler, work, text):
    """Require a successful, diagnostic-free lexical analysis."""
    result, table = scan(compiler, work, text)
    require(result.returncode == 0 and not result.stderr,
            f"Valid input rejected: {text!r}\n{result.stderr}")
    return rows(table)


def fixture(compiler, work, name):
    """Compare every printed token/value/location against an unchanged repository golden."""
    directory = Path(__file__).resolve().parents[1] / "testcase/lexer"
    result, actual = scan(compiler, work, (directory / f"{name}.sy").read_bytes())
    require(result.returncode == 0 and not result.stderr, result.stderr)
    expected = (directory / f"{name}.lexer").read_text(encoding="utf-8")
    expected_lines = [row.rstrip() for row in expected.splitlines()]
    actual_lines = [row.rstrip() for row in actual.splitlines()]
    diff = "\n".join(difflib.unified_diff(expected_lines, actual_lines,
                                          fromfile=f"{name}.lexer", tofile="actual"))
    require(expected_lines == actual_lines, diff)
    print(f"{name}: {len(actual_lines) - 1} tokens match the existing golden")


def boundaries(compiler, work):
    """Exercise maximal munch, numeric bases/ranges, comments, strings and positions."""
    keywords = "const int float void if else while break continue return"
    actual = valid(compiler, work, keywords + " _ _a9 integer iffy Float starttime")
    require([row[0] for row in actual] == [word.upper() for word in keywords.split()] + ["IDENT"] * 6,
            f"Keyword/identifier boundary mismatch: {actual}")
    extensions = valid(compiler, work, "for switch case goto do")
    require([row[0] for row in extensions] == ["FOR", "SWITCH", "CASE", "GOTO", "DO"],
            "Pre-existing framework keyword extensions changed")

    actual = valid(compiler, work, "+ - * / % = == != < > <= >= ! && || ; , ( ) [ ] { }")
    require([row[0] for row in actual] == [
        "PLUS", "MINUS", "STAR", "SLASH", "MOD", "ASSIGN", "EQ", "NE", "LT", "GT",
        "LE", "GE", "NOT", "AND", "OR", "SEMICOLON", "COMMA", "LPAREN", "RPAREN",
        "LBRACKET", "RBRACKET", "LBRACE", "RBRACE"], "Operator coverage is incomplete")
    actual = valid(compiler, work, "a<=b&&c!=d||!e==f>=g -2147483648 +1e-3 0x1e+2")
    require([row[0] for row in actual] == [
        "IDENT", "LE", "IDENT", "AND", "IDENT", "NE", "IDENT", "OR", "NOT", "IDENT",
        "EQ", "IDENT", "GE", "IDENT", "MINUS", "LL_CONST", "PLUS", "FLOAT_CONST",
        "INT_CONST", "PLUS", "INT_CONST"], "Maximal munch/sign tokenization changed")

    integers = [("0", 0), ("00", 0), ("077", 63), ("0x2a", 42), ("0XfF", 255),
                ("2147483647", 2147483647), ("2147483648", 2147483648),
                ("0xffffffff", 4294967295), ("9223372036854775807", 9223372036854775807)]
    actual = valid(compiler, work, " ".join(text for text, _ in integers))
    for row, (text, value) in zip(actual, integers):
        kind = "LL_CONST" if value > 2147483647 else "INT_CONST"
        require(row[:3] == (kind, text, str(value)), f"Bad integer conversion: {row}")

    floats = [("1.", 1), (".5", .5), ("01.5", 1.5), ("1e3", 1000), ("1.E-2", .01),
              (".5e+1", 5), ("0x1p0", 1), ("0X.8P+1", 1), ("0x1.p0", 1),
              ("0x1.8p+2", 6), ("0x.8p-1", .25), ("0x1p-149", 2 ** -149),
              ("0x1.fffffep127", float.fromhex("0x1.fffffep127")),
              ("0e999999999", 0), ("0x0p-999999999", 0)]
    actual = valid(compiler, work, " ".join(text for text, _ in floats))
    require(len(actual) == len(floats), "A floating token was split")
    for row, (text, value) in zip(actual, floats):
        require(row[:2] == ("FLOAT_CONST", text), f"Bad floating token: {row}")
        require(math.isclose(float(row[2]), value, rel_tol=5e-6, abs_tol=0),
                f"Bad float conversion: {row}, expected {value}")

    actual = valid(compiler, work, "a/**/b/* outer /* not nested */c//ignored\r\n\t d")
    require([(row[1], row[3], row[4]) for row in actual] == [
        ("a", 1, 0), ("b", 1, 5), ("c", 1, 31), ("d", 2, 5)],
        f"Comment boundaries/positions incorrect: {actual}")
    actual = valid(compiler, work, "\talpha\r\n/*one\r two\n*/\t123\r\tfloat\v\fvoid")
    require([(row[1], row[3], row[4]) for row in actual] == [
        ("alpha", 1, 4), ("123", 4, 4), ("float", 5, 4), ("void", 5, 11)],
        f"Whitespace/CRLF/tab locations incorrect: {actual}")
    actual = valid(compiler, work, "/* comment-only file */ // no final newline")
    require(not actual, "Comments must not emit tokens")

    strings = [r'""', r'"a\"b\\c\n"', r'"// /* */"', r'"\101\x42\t"']
    actual = valid(compiler, work, " ".join(strings))
    require([row[:3] for row in actual] == [("STR_CONST", text, text[1:-1]) for text in strings],
            f"String spelling/payload changed: {actual}")

    # Metamorphic checks: safe whitespace/comment insertion cannot change tokens.
    rng = random.Random(2022)
    spellings = ["int", "name_9", "0", "077", "0x1e", "2147483648", "1.5e-2",
                 "0x.8p1", "+", "/", "<=", "&&", "[", "]", r'"/* // */"']
    separators = [" ", "\t", "\n", " /**/ ", " /* stars **/ ", "\r\n", " // end\n"]
    for _ in range(20):
        tokens = [rng.choice(spellings) for _ in range(40)]
        baseline = valid(compiler, work, " ".join(tokens))
        decorated = valid(compiler, work, "".join(token + rng.choice(separators) for token in tokens))
        require([row[:3] for row in baseline] == [row[:3] for row in decorated],
                "Comment/whitespace insertion changed the token sequence")

    invalid = ["@", "&", "|", ".", "09", "0x", "0xG", "0x1.2", "0x.p2",
               "1e", "1e+", "1e-", "0x1p", "0x1p-", "1.2.3", "123abc", "1.0f",
               "1u", "1LL", "9223372036854775808", "18446744073709551616",
               "1e999", "1e-999", "0x1p128", "0x1p-999", "/* open", '"open',
               '"line\nbreak"', '"trailing\\', r'"bad\q"', r'"bad\x"', b"\x00", b'"a\x00b"']
    for text in invalid:
        result, table = scan(compiler, work, text)
        require(result.returncode == 1 and "error happened at:" in result.stderr,
                f"Bad input silently accepted/crashed: {text!r}\n{result}")
        require("ERR_TOKEN" in table, f"Bad input lacks an explicit error token: {text!r}")
    result, _ = scan(compiler, work, "\n  /* open")
    require("2.3" in result.stderr, f"Open comment must point to its start: {result.stderr}")
    print(f"Lexical boundaries passed, including {len(invalid)} invalid-input cases")


def main():
    """Select a golden or boundary suite and isolate all outputs under the build tree."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--work-dir", required=True)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--fixture", choices=["simple", "witharray", "withfloat"])
    mode.add_argument("--boundaries", action="store_true")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(dir=args.work_dir, prefix="lexer-") as directory:
        if args.fixture:
            fixture(args.compiler, Path(directory), args.fixture)
        else:
            boundaries(args.compiler, Path(directory))


if __name__ == "__main__":
    main()
