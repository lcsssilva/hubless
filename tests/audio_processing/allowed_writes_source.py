#!/usr/bin/env python3
"""Garantia de escritas no PipeWire do módulo audio_processing (ADR-0016, INC-010 R9).

No código C++ de src/modules/audio_processing/ as únicas escritas no PipeWire permitidas são:
  - pw_context_load_module com o módulo "libpipewire-module-filter-chain";
  - pw_node_set_param com o parâmetro SPA_PARAM_Props (controles do EQ e da redução de ruído do microfone);
  - pw_metadata_set_property somente com as chaves "default.configured.audio.sink" e
    "default.configured.audio.source" (ADR-0018);
  - pw_impl_module_destroy (destruição do módulo próprio).
Qualquer outra função de escrita conhecida (pw_metadata_clear, pw_registry_destroy, pw_core_create_object,
pw_device_set_param, pw_node_send_command, pw_port_set_param, pw_link_*...) ou qualquer outra chave "default.*"
literal falha. Comentários são ignorados.

Uso: python3 -I allowed_writes_source.py <raiz-do-repositório>
     python3 -I allowed_writes_source.py --self-test
"""
from __future__ import annotations

import re
import sys
import tempfile
from pathlib import Path

LOAD_MODULE = "libpipewire-module-filter-chain"
METADATA_KEYS = {"default.configured.audio.sink", "default.configured.audio.source"}
FORBIDDEN_CALLS = [
    "pw_metadata_clear",
    "pw_registry_destroy",
    "pw_core_create_object",
    "pw_core_set_param",
    "pw_device_set_param",
    "pw_node_send_command",
    "pw_port_set_param",
    "pw_proxy_destroy_global",
    "pw_core_destroy",
    "pw_properties_save",
]
COMMENT = re.compile(r"//[^\n]*|/\*.*?\*/", re.S)
STRING_CONSTANT = re.compile(r'(\w+)\s*=\s*"((?:[^"\\\n]|\\.)*)"\s*;')
LITERAL = re.compile(r'"((?:[^"\\\n]|\\.)*)"')


def strip_comments(text: str) -> str:
    """Remove comentários preservando literais de texto e a numeração de linhas."""
    out: list[str] = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c == '"':
            j = i + 1
            while j < n and text[j] != '"':
                j += 2 if text[j] == "\\" else 1
            out.append(text[i : j + 1])
            i = j + 1
        elif text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("\n" * text.count("\n", i, j))
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def split_args(text: str) -> list[str]:
    args: list[str] = []
    depth = 0
    current: list[str] = []
    in_string = False
    i = 0
    while i < len(text):
        c = text[i]
        if in_string:
            current.append(c)
            if c == "\\" and i + 1 < len(text):
                current.append(text[i + 1])
                i += 1
            elif c == '"':
                in_string = False
        elif c == '"':
            in_string = True
            current.append(c)
        elif c in "([{":
            depth += 1
            current.append(c)
        elif c in ")]}":
            depth -= 1
            current.append(c)
        elif c == "," and depth == 0:
            args.append("".join(current).strip())
            current = []
        else:
            current.append(c)
        i += 1
    tail = "".join(current).strip()
    if tail:
        args.append(tail)
    return args


def calls(text: str, name: str) -> list[tuple[int, list[str]]]:
    """Todas as chamadas `name(...)`, com a linha e os argumentos de nível superior."""
    found: list[tuple[int, list[str]]] = []
    for match in re.finditer(r"\b" + re.escape(name) + r"\s*\(", text):
        depth = 1
        i = match.end()
        in_string = False
        while i < len(text) and depth > 0:
            c = text[i]
            if in_string:
                if c == "\\":
                    i += 1
                elif c == '"':
                    in_string = False
            elif c == '"':
                in_string = True
            elif c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
            i += 1
        found.append((text.count("\n", 0, match.start()) + 1, split_args(text[match.end() : i - 1])))
    return found


def resolve(argument: str, constants: dict[str, str]) -> str | None:
    """Valor de texto de um argumento: literal entre aspas ou constante conhecida; None se não resolvível."""
    literal = LITERAL.fullmatch(argument)
    if literal:
        return literal.group(1)
    return constants.get(argument)


def check_text(label: str, raw: str) -> list[str]:
    text = strip_comments(raw)
    constants = dict(STRING_CONSTANT.findall(text))
    problems: list[str] = []

    for line, args in calls(text, "pw_context_load_module"):
        module = resolve(args[1], constants) if len(args) > 1 else None
        if module != LOAD_MODULE:
            problems.append(f"{label}:{line}: pw_context_load_module só pode carregar '{LOAD_MODULE}'")
    for line, args in calls(text, "pw_node_set_param"):
        if len(args) < 2 or args[1] != "SPA_PARAM_Props":
            problems.append(f"{label}:{line}: pw_node_set_param só pode escrever SPA_PARAM_Props")
    for line, args in calls(text, "pw_metadata_set_property"):
        key = resolve(args[2], constants) if len(args) > 2 else None
        if key not in METADATA_KEYS:
            problems.append(f"{label}:{line}: pw_metadata_set_property só pode usar as chaves {sorted(METADATA_KEYS)}")
    for function in FORBIDDEN_CALLS:
        for line, _ in calls(text, function):
            problems.append(f"{label}:{line}: chamada proibida {function}")
    for number, source_line in enumerate(text.splitlines(), start=1):
        for literal in LITERAL.findall(source_line):
            if literal.startswith("default.") and literal not in METADATA_KEYS:
                problems.append(f"{label}:{number}: chave de metadata '{literal}' não permitida")
    return problems


def check(root: Path) -> list[str]:
    module = root / "src" / "modules" / "audio_processing"
    if not module.is_dir():
        return [f"{module}: diretório ausente"]
    problems: list[str] = []
    for path in sorted(module.rglob("*")):
        if path.is_file() and path.suffix in {".hpp", ".h", ".cpp", ".cc"}:
            problems += check_text(str(path.relative_to(root)), path.read_text(encoding="utf-8"))
    return problems


GOOD = """
constexpr const char* filterChainModule = "libpipewire-module-filter-chain";
constexpr const char* defaultSinkKey = "default.configured.audio.sink";
constexpr const char* defaultSourceKey = "default.configured.audio.source";
// pw_metadata_clear(md) em comentário não conta
void f() {
    pw_context_load_module(context_, filterChainModule, args->c_str(), nullptr);
    pw_context_load_module(context_, "libpipewire-module-filter-chain", nullptr, nullptr);
    pw_node_set_param(proxy, SPA_PARAM_Props, 0, param);
    pw_metadata_set_property(md, 0, defaultSinkKey, "Spa:String:JSON", value.c_str());
    pw_metadata_set_property(md, 0, "default.configured.audio.sink", nullptr, nullptr);
    pw_metadata_set_property(md, 0, defaultSourceKey, "Spa:String:JSON", value.c_str());
    pw_metadata_set_property(md, 0, "default.configured.audio.source", nullptr, nullptr);
    pw_impl_module_destroy(module);
}
"""


def self_test() -> int:
    ok = not check_text("good", GOOD)
    bad_cases = {
        "outro módulo": 'pw_context_load_module(context_, "libpipewire-module-loopback", a, nullptr);',
        "módulo por variável desconhecida": "pw_context_load_module(context_, name, a, nullptr);",
        "set_param Format": "pw_node_set_param(proxy, SPA_PARAM_Format, 0, param);",
        "outra chave": 'pw_metadata_set_property(md, 0, "default.audio.sink", "t", "v");',
        "chave por variável desconhecida": "pw_metadata_set_property(md, 0, key, nullptr, nullptr);",
        "metadata clear": "pw_metadata_clear(md);",
        "registry destroy": "pw_registry_destroy(registry_, id);",
        "create object": 'pw_core_create_object(core_, "link-factory", t, v, p, 0);',
        "device set param": "pw_device_set_param(dev, id, 0, pod);",
        "node send command": "pw_node_send_command(node, cmd);",
        "literal default.*": 'const char* k = "default.audio.source";',
        "outra chave configured": 'pw_metadata_set_property(md, 0, "default.configured.audio.sink.channels", "t", "v");',
        "literal configured proibido": 'const char* k = "default.configured.video.source";',
    }
    for label, code in bad_cases.items():
        if not check_text(label, code):
            print(f"FALHA: caso proibido não detectado: {label}", file=sys.stderr)
            ok = False
    with tempfile.TemporaryDirectory() as tmp:
        module = Path(tmp) / "src" / "modules" / "audio_processing"
        module.mkdir(parents=True)
        (module / "a.cpp").write_text(GOOD, encoding="utf-8")
        ok = ok and not check(Path(tmp))
        (module / "b.cpp").write_text("pw_metadata_clear(md);\n", encoding="utf-8")
        ok = ok and len(check(Path(tmp))) == 1
    print("OK: self-test" if ok else "FALHA: self-test", file=sys.stdout if ok else sys.stderr)
    return 0 if ok else 1


def main(argv: list[str]) -> int:
    if len(argv) > 1 and argv[1] == "--self-test":
        return self_test()
    root = Path(argv[1]).resolve() if len(argv) > 1 else Path(__file__).resolve().parents[2]
    problems = check(root)
    for problem in problems:
        print(problem, file=sys.stderr)
    if problems:
        print(f"FALHA: {len(problems)} escrita(s) fora da lista permitida em src/modules/audio_processing/", file=sys.stderr)
        return 1
    print("OK: src/modules/audio_processing/ só escreve o filter-chain próprio, Props dos filtros e default.configured.audio.sink/source")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
