#!/usr/bin/env python3
"""Garantia de escrita onboard confinada (ADR-0017, INC-011 R8b).

Falha se:
  - "writeSector" aparecer em src/modules/pointer/ fora de application/OnboardMemory.hpp (o port),
    application/G703OnboardSettings.cpp (o caso de uso) e infrastructure/HidppOnboardMemory.* (o adaptador);
  - "G703OnboardPolicy" aparecer em src/modules/lighting/ (a política do G703 não vale para o G915).

Uso: python3 -I onboard_write_source.py <raiz-do-repositório>
"""
from __future__ import annotations

import sys
from pathlib import Path

SUFFIXES = {".hpp", ".h", ".cpp", ".cc", ".txt", ".cmake"}
WRITE_ALLOWED = {
    "application/OnboardMemory.hpp",
    "application/G703OnboardSettings.cpp",
    "infrastructure/HidppOnboardMemory.hpp",
    "infrastructure/HidppOnboardMemory.cpp",
}


def scan(module: Path, needle: str, allowed: set[str], root: Path, why: str) -> list[str]:
    if not module.is_dir():
        return [f"{module}: diretório ausente"]
    found: list[str] = []
    for path in sorted(module.rglob("*")):
        if not path.is_file() or path.suffix not in SUFFIXES:
            continue
        if path.relative_to(module).as_posix() in allowed:
            continue
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
            if needle in line:
                found.append(f"{path.relative_to(root)}:{number}: '{needle}' proibido aqui ({why})")
    return found


def check(root: Path) -> list[str]:
    modules = root / "src" / "modules"
    return scan(modules / "pointer", "writeSector", WRITE_ALLOWED, root,
                "escrita onboard só no caso de uso e no adaptador") + \
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
