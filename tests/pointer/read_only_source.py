#!/usr/bin/env python3
"""Garantia de leitura do módulo pointer (ADR-0011 §6b, INC-007 R13).

Falha se algum arquivo de src/modules/pointer/ contiver um identificador de escrita do ratbagd:
Commit, SetActive, SetDefault, a string "Set" ou setProperty.

Uso: python3 -I read_only_source.py <raiz-do-repositório>
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

FORBIDDEN = re.compile(r'Commit|SetActive|SetDefault|"Set"|setProperty')


def check(root: Path) -> list[str]:
    module = root / "src" / "modules" / "pointer"
    if not module.is_dir():
        return [f"{module}: diretório ausente"]
    found: list[str] = []
    for path in sorted(module.rglob("*")):
        if not path.is_file() or path.suffix not in {".hpp", ".h", ".cpp", ".cc", ".txt", ".cmake"}:
            continue
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
            match = FORBIDDEN.search(line)
            if match:
                found.append(f"{path.relative_to(root)}:{number}: '{match.group(0)}' proibido em src/modules/pointer/")
    return found


def main(argv: list[str]) -> int:
    root = Path(argv[1]).resolve() if len(argv) > 1 else Path(__file__).resolve().parents[2]
    violations = check(root)
    for violation in violations:
        print(violation, file=sys.stderr)
    if violations:
        print(f"FALHA: {len(violations)} ocorrência(s) de escrita no módulo pointer", file=sys.stderr)
        return 1
    print("OK: src/modules/pointer/ não contém Commit/SetActive/SetDefault/\"Set\"/setProperty")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
