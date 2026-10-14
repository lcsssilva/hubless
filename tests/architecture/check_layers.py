#!/usr/bin/env python3
"""Teste de arquitetura: falha quando um #include cruza fronteira proibida (ADR-0002, OVERVIEW.md §2).

Uso: python3 check_layers.py <raiz-do-repositório>
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

INCLUDE_RE = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]')
MODULE_INCLUDE_RE = re.compile(r"^modules/([a-z_]+)/([a-z_]+)/")
SOURCE_SUFFIXES = {".hpp", ".h", ".cpp", ".cc", ".cxx", ".ipp"}

# Cabeçalhos de framework/I/O proibidos em domain, application e shared_kernel.
FORBIDDEN_IN_CORE = re.compile(
    r"^(Q[A-Za-z]|qt|hidapi|linux/|pipewire|spa/|dbus|libudev|sys/|filesystem$|fstream$)"
)

# Camadas do próprio módulo que cada camada pode incluir.
ALLOWED_OWN_LAYERS = {
    "domain": {"domain"},
    "application": {"domain", "application", "api"},
    "api": {"domain", "application", "api"},
    "infrastructure": {"domain", "application", "api", "infrastructure"},
    "presentation": {"domain", "application", "api", "presentation"},
}
CORE_LAYERS = {"domain", "application"}


def classify(rel: Path) -> tuple[str | None, str | None]:
    """Retorna (módulo, camada) para arquivos em src/modules; ('', 'shared_kernel') para o kernel."""
    parts = rel.parts
    if len(parts) >= 4 and parts[0] == "src" and parts[1] == "modules":
        return parts[2], parts[3]
    if len(parts) >= 2 and parts[0] == "src" and parts[1] == "shared_kernel":
        return "", "shared_kernel"
    return None, None


def check_file(root: Path, path: Path) -> list[str]:
    rel = path.relative_to(root)
    module, layer = classify(rel)
    if layer is None:
        return []
    found: list[str] = []
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        match = INCLUDE_RE.match(line)
        if not match:
            continue
        target = match.group(2)
        where = f"{rel}:{number}: {target}"
        if layer in CORE_LAYERS | {"shared_kernel"} and FORBIDDEN_IN_CORE.match(target):
            found.append(f"{where} — framework/I-O proibido em {layer}")
            continue
        module_match = MODULE_INCLUDE_RE.match(target)
        if not module_match:
            continue
        target_module, target_layer = module_match.groups()
        if layer == "shared_kernel":
            found.append(f"{where} — shared_kernel não pode depender de módulos")
        elif layer == "domain" and target_module != module:
            found.append(f"{where} — domain não pode incluir nada de outro módulo (nem a api/)")
        elif target_module != module and target_layer != "api":
            found.append(f"{where} — módulo '{module}' só pode incluir modules/{target_module}/api/")
        elif target_module == module and target_layer not in ALLOWED_OWN_LAYERS.get(layer, set()):
            found.append(f"{where} — camada '{layer}' não pode depender de '{target_layer}'")
    return found


def check(root: Path) -> list[str]:
    src = root / "src"
    if not src.is_dir():
        return [f"{src}: diretório src/ ausente"]
    violations: list[str] = []
    for path in sorted(src.rglob("*")):
        if path.is_file() and path.suffix in SOURCE_SUFFIXES:
            violations.extend(check_file(root, path))
    return violations


def main(argv: list[str]) -> int:
    root = Path(argv[1]).resolve() if len(argv) > 1 else Path(__file__).resolve().parents[2]
    violations = check(root)
    for violation in violations:
        print(violation, file=sys.stderr)
    if violations:
        print(f"FALHA: {len(violations)} violação(ões) de fronteira", file=sys.stderr)
        return 1
    print("OK: nenhuma violação de fronteira")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
