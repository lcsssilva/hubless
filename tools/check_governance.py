#!/usr/bin/env python3
"""Verifica a estrutura de governança do repositório (skills, docs, matriz de suporte).

Somente biblioteca padrão. Saída != 0 quando houver erro.
Uso: python3 tools/check_governance.py
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

REQUIRED_SKILLS = [
    "requirements-auditor",
    "architecture-auditor",
    "hardware-compatibility-auditor",
    "protocol-safety-auditor",
    "integration-auditor",
    "strong-testing",
    "mutation-testing",
    "security-auditor",
    "code-quality-auditor",
    "linux-packaging-auditor",
    "device-support-matrix",
    "release-auditor",
    "docs-maintainer",
]

REQUIRED_DOCS = [
    "CLAUDE.md",
    "docs/architecture/OVERVIEW.md",
    "docs/architecture/COMPONENTS.md",
    "docs/architecture/DEVICE_ACCESS_POLICY.md",
    "docs/architecture/DOMAIN_GLOSSARY.md",
    "docs/engineering/ENGINEERING_RULES.md",
    "docs/engineering/DEFINITION_OF_DONE.md",
    "docs/engineering/TEST_STRATEGY.md",
    "docs/integrations/INTEGRATION_CATALOG.md",
    "docs/integrations/OPENRGB.md",
    "docs/integrations/LIBRATBAG.md",
    "docs/integrations/SOLAAR.md",
    "docs/integrations/HEADSETCONTROL.md",
    "docs/integrations/PIPEWIRE.md",
    "docs/integrations/V4L2_UVC.md",
    "docs/integrations/QT6_QML.md",
    "docs/device-support/SUPPORT_MATRIX.md",
    "docs/device-support/G915.md",
    "docs/device-support/G703.md",
    "docs/device-support/G733.md",
    "docs/device-support/BRIO_100.md",
    "docs/decisions/ADR-0001-TECHNICAL-BASELINE.md",
    "docs/quality/SKILL_ACTIVATION.md",
    "docs/quality/RELEASE_CHECKLIST.md",
    "docs/quality/VERIFICATION_LOG.md",
]

ALLOWED_STATES = {
    "CONFIRMADO_EM_HARDWARE",
    "CONFIRMADO_POR_DOCUMENTACAO",
    "IMPLEMENTADO_SEM_TESTE_FISICO",
    "PARCIAL",
    "EM_INVESTIGACAO",
    "NAO_SUPORTADO",
    "BLOQUEADO",
}

STATUS_HEADER = "Situação"
SKILL_NAME_RE = re.compile(r"^[a-z0-9]+(-[a-z0-9]+)*$")
MD_LINK_RE = re.compile(r"\[[^\]]*\]\(([^)#\s]+)(?:#[^)]*)?\)")
EVIDENCE_REF_RE = re.compile(r"evidence/[\w.\-]+\.md")


def parse_frontmatter(text: str) -> dict[str, str] | None:
    lines = text.splitlines()
    if not lines or lines[0].strip() != "---":
        return None
    fields: dict[str, str] = {}
    for line in lines[1:]:
        if line.strip() == "---":
            return fields
        if ":" in line and not line.startswith((" ", "\t")):
            key, value = line.split(":", 1)
            fields[key.strip()] = value.strip()
    return None  # frontmatter sem fechamento


def check_skills(errors: list[str]) -> None:
    skills_dir = ROOT / ".claude" / "skills"
    present = sorted(p.name for p in skills_dir.iterdir() if p.is_dir()) if skills_dir.is_dir() else []
    for name in REQUIRED_SKILLS:
        if name not in present:
            errors.append(f"skill ausente: .claude/skills/{name}/SKILL.md")
    for name in present:
        path = skills_dir / name / "SKILL.md"
        if not path.is_file():
            errors.append(f"{path.relative_to(ROOT)}: arquivo SKILL.md ausente")
            continue
        text = path.read_text(encoding="utf-8")
        fm = parse_frontmatter(text)
        rel = path.relative_to(ROOT)
        if fm is None:
            errors.append(f"{rel}: frontmatter ausente ou não fechado")
            continue
        if fm.get("name") != name:
            errors.append(f"{rel}: 'name' ({fm.get('name')!r}) difere do diretório ({name!r})")
        if not SKILL_NAME_RE.match(fm.get("name", "")) or len(fm.get("name", "")) > 64:
            errors.append(f"{rel}: 'name' inválido (minúsculas, dígitos e hífens; máx. 64)")
        desc = fm.get("description", "")
        if not desc:
            errors.append(f"{rel}: 'description' vazia")
        elif len(desc) > 1024:
            errors.append(f"{rel}: 'description' com {len(desc)} caracteres (máx. 1024)")
        elif "Use " not in desc:
            errors.append(f"{rel}: 'description' não diz quando usar (esperado 'Use ...')")
        body = text.split("---", 2)[2] if text.count("---") >= 2 else ""
        if len(body.strip().splitlines()) < 10:
            errors.append(f"{rel}: corpo muito curto para instruções operacionais")


def check_skill_references(errors: list[str]) -> None:
    """Toda skill citada em CLAUDE.md/SKILL_ACTIVATION.md existe, e toda skill existente é citada."""
    for doc in ("CLAUDE.md", "docs/quality/SKILL_ACTIVATION.md"):
        path = ROOT / doc
        if not path.is_file():
            continue
        text = path.read_text(encoding="utf-8")
        cited = set(re.findall(r"`([a-z]+(?:-[a-z]+)+)`", text)) & (set(REQUIRED_SKILLS) | {
            p.name for p in (ROOT / ".claude" / "skills").glob("*") if p.is_dir()
        })
        for name in REQUIRED_SKILLS:
            if name not in cited:
                errors.append(f"{doc}: skill '{name}' não aparece nas regras de ativação")


def check_required_docs(errors: list[str]) -> None:
    for rel in REQUIRED_DOCS:
        if not (ROOT / rel).is_file():
            errors.append(f"documento obrigatório ausente: {rel}")


def iter_tables(text: str):
    """Gera (header_cells, [row_cells...]) para cada tabela markdown."""
    lines = text.splitlines()
    i = 0
    while i < len(lines) - 1:
        if lines[i].lstrip().startswith("|") and re.match(r"^\s*\|[\s:\-|]+\|\s*$", lines[i + 1]):
            header = [c.strip() for c in lines[i].strip().strip("|").split("|")]
            rows = []
            j = i + 2
            while j < len(lines) and lines[j].lstrip().startswith("|"):
                rows.append((j + 1, [c.strip() for c in lines[j].strip().strip("|").split("|")]))
                j += 1
            yield header, rows
            i = j
        else:
            i += 1


def check_support_matrix(errors: list[str]) -> int:
    path = ROOT / "docs/device-support/SUPPORT_MATRIX.md"
    if not path.is_file():
        return 0
    text = path.read_text(encoding="utf-8")
    checked = 0
    ids: set[str] = set()
    for header, rows in iter_tables(text):
        if STATUS_HEADER not in header:
            continue
        col = header.index(STATUS_HEADER)
        id_col = header.index("ID") if "ID" in header else None
        for lineno, cells in rows:
            if len(cells) != len(header):
                errors.append(f"SUPPORT_MATRIX.md:{lineno}: {len(cells)} colunas, esperado {len(header)}")
                continue
            state = cells[col].strip("` ")
            if state not in ALLOWED_STATES:
                errors.append(f"SUPPORT_MATRIX.md:{lineno}: estado inválido {state!r}")
            if id_col is not None:
                rid = cells[id_col]
                if rid in ids:
                    errors.append(f"SUPPORT_MATRIX.md:{lineno}: ID duplicado {rid!r}")
                ids.add(rid)
            if state == "CONFIRMADO_EM_HARDWARE" and not EVIDENCE_REF_RE.search(" ".join(cells)):
                errors.append(f"SUPPORT_MATRIX.md:{lineno}: CONFIRMADO_EM_HARDWARE sem arquivo em evidence/")
            checked += 1
    for ref in set(EVIDENCE_REF_RE.findall(text)):
        if not (path.parent / ref).is_file():
            errors.append(f"SUPPORT_MATRIX.md: evidência inexistente {ref}")
    if checked == 0:
        errors.append("SUPPORT_MATRIX.md: nenhuma linha com coluna 'Situação' encontrada")
    return checked


def strip_code(text: str) -> str:
    """Remove blocos cercados (```) e código inline, onde colchetes/parênteses não são links."""
    text = re.sub(r"^[ \t]*```.*?^[ \t]*```", "", text, flags=re.MULTILINE | re.DOTALL)
    return re.sub(r"`[^`\n]*`", "", text)


def check_relative_links(errors: list[str]) -> None:
    files = [ROOT / "CLAUDE.md", ROOT / "README.md", *sorted((ROOT / "docs").rglob("*.md")), *sorted((ROOT / ".claude").rglob("*.md"))]
    for path in files:
        if not path.is_file():
            continue
        for target in MD_LINK_RE.findall(strip_code(path.read_text(encoding="utf-8"))):
            if re.match(r"^[a-z]+:", target):  # http:, https:, mailto:
                continue
            if not (path.parent / target).exists():
                errors.append(f"{path.relative_to(ROOT)}: link quebrado -> {target}")


def main() -> int:
    errors: list[str] = []
    check_required_docs(errors)
    check_skills(errors)
    check_skill_references(errors)
    rows = check_support_matrix(errors)
    check_relative_links(errors)
    if errors:
        print(f"FALHOU: {len(errors)} problema(s)")
        for e in errors:
            print(f"  - {e}")
        return 1
    skills = len(list((ROOT / ".claude" / "skills").glob("*/SKILL.md")))
    print(f"OK: {len(REQUIRED_DOCS)} documentos obrigatórios, {skills} skills válidas, {rows} linhas de matriz com estados válidos, links relativos íntegros")
    return 0


if __name__ == "__main__":
    sys.exit(main())
