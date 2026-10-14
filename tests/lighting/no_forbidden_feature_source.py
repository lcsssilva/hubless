#!/usr/bin/env python3
"""Garantia do módulo lighting (ADR-0013 decisão 7, INC-008 R3).

Falha se algum arquivo de src/modules/lighting/ montar a feature 0x4522 (handshake de semântica desconhecida)
fora de HidppCommandPolicy.cpp, onde ela só pode constar na lista de bloqueio.

Uso: python3 -I no_forbidden_feature_source.py <raiz-do-repositório>
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

FORBIDDEN = re.compile(r"0[xX]4522|0[xX]45\s*,\s*0[xX]22|0[xX]22\s*,\s*0[xX]45")
ALLOWED_FILE = "HidppCommandPolicy.cpp"


def check(root: Path) -> list[str]:
    module = root / "src" / "modules" / "lighting"
    if not module.is_dir():
        return [f"{module}: diretório ausente"]
    found: list[str] = []
    for path in sorted(module.rglob("*")):
        if not path.is_file() or path.suffix not in {".hpp", ".h", ".cpp", ".cc", ".txt", ".cmake"}:
            continue
        if path.name == ALLOWED_FILE:
            continue
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
            match = FORBIDDEN.search(line)
            if match:
                found.append(f"{path.relative_to(root)}:{number}: '{match.group(0)}' proibido fora de {ALLOWED_FILE}")
    return found


def main(argv: list[str]) -> int:
    root = Path(argv[1]).resolve() if len(argv) > 1 else Path(__file__).resolve().parents[2]
    violations = check(root)
    for violation in violations:
        print(violation, file=sys.stderr)
    if violations:
        print(f"FALHA: {len(violations)} ocorrência(s) da feature 0x4522 no módulo lighting", file=sys.stderr)
        return 1
    print(f"OK: src/modules/lighting/ não contém 0x4522 fora de {ALLOWED_FILE}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
