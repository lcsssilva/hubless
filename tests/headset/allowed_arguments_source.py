#!/usr/bin/env python3
"""Garantia de argumentos do módulo headset (ADR-0015, INC-009 R10).

Falha se algum arquivo C++ de src/modules/headset/ contiver um literal de texto com uma opção de linha de
comando do headsetcontrol diferente de -d, -s, -l e -o (ex.: -c, -e, -p, -r, -i, -m, -n, -f, -b, --follow).
Só literais entre aspas são examinados; comentários e identificadores não contam.

Uso: python3 -I allowed_arguments_source.py <raiz-do-repositório>
     python3 -I allowed_arguments_source.py --self-test
"""
from __future__ import annotations

import re
import sys
import tempfile
from pathlib import Path

ALLOWED = {"-d", "-s", "-l", "-o"}
LITERAL = re.compile(r'"((?:[^"\\\n]|\\.)*)"')
OPTION = re.compile(r"--?[A-Za-z][\w-]*")


def forbidden_options(line: str) -> list[str]:
    found: list[str] = []
    for literal in LITERAL.findall(line):
        for token in literal.split():
            if OPTION.fullmatch(token) and token not in ALLOWED:
                found.append(token)
    return found


def check(root: Path) -> list[str]:
    module = root / "src" / "modules" / "headset"
    if not module.is_dir():
        return [f"{module}: diretório ausente"]
    found: list[str] = []
    for path in sorted(module.rglob("*")):
        if not path.is_file() or path.suffix not in {".hpp", ".h", ".cpp", ".cc"}:
            continue
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
            for option in forbidden_options(line):
                found.append(f"{path.relative_to(root)}:{number}: opção '{option}' proibida em src/modules/headset/")
    return found


def self_test() -> int:
    ok = all(not forbidden_options(f'QStringLiteral("{o}")') for o in sorted(ALLOWED))
    forbidden = ["-c", "-e", "-p", "-r", "-i", "-m", "-n", "-f", "-b", "--follow"]
    ok = ok and all(forbidden_options(f'QStringLiteral("{o}")') == [o] for o in forbidden)
    ok = ok and forbidden_options('"-d -c"') == ["-c"]
    ok = ok and not forbidden_options("// headsetcontrol -c  (comentário)")
    ok = ok and not forbidden_options('"0x046d:0x0afe" "sidetone fora de 0-100" "-"')
    with tempfile.TemporaryDirectory() as tmp:
        module = Path(tmp) / "src" / "modules" / "headset"
        module.mkdir(parents=True)
        (module / "x.cpp").write_text('auto a = QStringLiteral("-s");\nauto b = QStringLiteral("-c");\n', encoding="utf-8")
        ok = ok and len(check(Path(tmp))) == 1
    print("OK: self-test" if ok else "FALHA: self-test", file=sys.stderr if not ok else sys.stdout)
    return 0 if ok else 1


def main(argv: list[str]) -> int:
    if len(argv) > 1 and argv[1] == "--self-test":
        return self_test()
    root = Path(argv[1]).resolve() if len(argv) > 1 else Path(__file__).resolve().parents[2]
    violations = check(root)
    for violation in violations:
        print(violation, file=sys.stderr)
    if violations:
        print(f"FALHA: {len(violations)} opção(ões) fora de -d/-s/-l/-o no módulo headset", file=sys.stderr)
        return 1
    print("OK: src/modules/headset/ só usa as opções -d, -s, -l e -o do headsetcontrol")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
