#!/usr/bin/env python3
"""Garantia de escrita onboard confinada (ADR-0017, INC-011 R8b, ADR-0027).

Falha se:
  - "writeSector" aparecer em src/modules/ fora de onboard_memory/application/OnboardMemory.hpp (o port),
    onboard_memory/application/ActiveSectorEditor.cpp (o caso de uso) e onboard_memory/infrastructure/
    HidppOnboardMemory.* (o adaptador);
  - "G703OnboardPolicy" aparecer em src/modules/lighting/ (a política do G703 não vale para o G915).

Uso: python3 -I write_source.py <raiz-do-repositório>
"""
from __future__ import annotations

import sys
from pathlib import Path

SUFFIXES = {".hpp", ".h", ".cpp", ".cc", ".txt", ".cmake"}
WRITE_ALLOWED = {
    "onboard_memory/application/OnboardMemory.hpp",
    "onboard_memory/application/ActiveSectorEditor.cpp",
    "onboard_memory/infrastructure/HidppOnboardMemory.hpp",
    "onboard_memory/infrastructure/HidppOnboardMemory.cpp",
}


def scan(base: Path, needle: str, allowed: set[str], root: Path, why: str) -> list[str]:
    if not base.is_dir():
        return [f"{base}: diretório ausente"]
    found: list[str] = []
    for path in sorted(base.rglob("*")):
        if not path.is_file() or path.suffix not in SUFFIXES:
            continue
        if path.relative_to(base).as_posix() in allowed:
            continue
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
            if needle in line:
                found.append(f"{path.relative_to(root)}:{number}: '{needle}' proibido aqui ({why})")
    return found


def check(root: Path) -> list[str]:
    modules = root / "src" / "modules"
    return scan(modules, "writeSector", WRITE_ALLOWED, root,
                "escrita onboard só no caso de uso e no adaptador do onboard_memory") + \
        scan(modules / "lighting", "G703OnboardPolicy", set(), root, "política do G703 não vale para o lighting")


def main(argv: list[str]) -> int:
    root = Path(argv[1]).resolve() if len(argv) > 1 else Path(__file__).resolve().parents[2]
    violations = check(root)
    for violation in violations:
        print(violation, file=sys.stderr)
    if violations:
        print(f"FALHA: {len(violations)} violação(ões) da confinação da escrita onboard", file=sys.stderr)
        return 1
    print("OK: writeSector confinado ao caso de uso/adaptador e G703OnboardPolicy ausente do lighting")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
