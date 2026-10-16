#!/usr/bin/env python3
"""Compara as telas renderizadas do Hubless com as capturas da V-01 (INC-014 parte 5).

Roda o teste de renderização (`test_rendersEachScreenAtMinimumSize`), recorta cada captura da V-01 para a janela do
G HUB, cobre com preto as áreas de marca/foto (máscara) nas duas imagens e usa `magick compare -metric AE` para contar
os pixels diferentes. Imprime a diferença em % por tela; as imagens de diferença vão para um diretório fora do
repositório. Só biblioteca padrão. Código de saída: 0 ok, 2 se o `magick` não existe, 1 em erro de execução.
A diferença é grosseira de propósito: fonte, ícones e fotos da Logitech não são usados (ADR-0023).
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys

DEFAULT_MAPPING = os.path.join(os.path.dirname(os.path.abspath(__file__)), "compare_ui_v01.json")
RENDER_TEST = "MainQmlBinding::test_rendersEachScreenAtMinimumSize"


def load_mapping(path):
    with open(path, encoding="utf-8") as handle:
        data = json.load(handle)
    for key in ("window_crop", "fuzz", "screens"):
        if key not in data:
            raise ValueError("mapeamento sem a chave '%s'" % key)
    for screen in data["screens"]:
        if "render" not in screen or "capture" not in screen:
            raise ValueError("tela sem 'render' ou 'capture': %r" % screen)
    return data


def mask_args(masks):
    """Argumentos do magick que pintam de preto cada retângulo [x, y, w, h]."""
    args = ["-fill", "black"]
    for x, y, w, h in masks:
        args += ["-draw", "rectangle %d,%d %d,%d" % (x, y, x + w - 1, y + h - 1)]
    return args


def render_command(build_dir):
    return [os.path.join(build_dir, "tests", "ui", "ghub_ui_tests"), RENDER_TEST]


def render_screens(build_dir, render_dir, run=subprocess.run):
    env = dict(os.environ)
    env.update({"QT_QPA_PLATFORMTHEME": "kde", "GHUB_UI_RENDER_DIR": render_dir,
                "QML_XHR_ALLOW_FILE_READ": "1", "QT_QPA_PLATFORM": "offscreen"})
    result = run(render_command(build_dir), env=env, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError("o teste de renderização falhou:\n%s" % (result.stdout or "")[-2000:])


def prepare(magick, source, target, crop, masks, run):
    """Recorta (se houver recorte), aplica a máscara e grava em `target`."""
    command = [magick, source]
    if crop:
        command += ["-crop", crop, "+repage"]
    command += mask_args(masks) + [target]
    result = run(command, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError("falha ao preparar %s: %s" % (source, result.stderr))


def parse_ae(stderr):
    match = re.match(r"\s*([0-9.eE+-]+)", stderr or "")
    if not match:
        raise ValueError("saída inesperada do compare: %r" % stderr)
    return float(match.group(1))


def image_size(magick, path, run):
    result = run([magick, "identify", "-format", "%w %h", path], capture_output=True, text=True)
    width, height = result.stdout.split()
    return int(width), int(height)


def compare_screen(magick, screen, mapping, render_dir, capture_dir, diff_dir, work_dir, run=subprocess.run):
    name = screen["render"]
    rendered = os.path.join(render_dir, name + ".png")
    capture = os.path.join(capture_dir, screen["capture"])
    masks = screen.get("masks", []) + mapping.get("masks", [])
    a = os.path.join(work_dir, name + "-render.png")
    b = os.path.join(work_dir, name + "-capture.png")
    # Cada lado é recortado ao tamanho da janela do G HUB; o render tem a janela do Hubless a partir de (0, 0).
    prepare(magick, rendered, a, mapping.get("render_crop"), masks, run)
    prepare(magick, capture, b, mapping["window_crop"], masks, run)
    width, height = image_size(magick, b, run)
    diff = os.path.join(diff_dir, name + "-diff.png")
    result = run([magick, "compare", "-metric", "AE", "-fuzz", mapping["fuzz"], a, b, diff],
                 capture_output=True, text=True)
    # `compare` sai com 1 quando há diferença e com 2 em erro de verdade.
    if result.returncode not in (0, 1):
        raise RuntimeError("compare falhou em %s: %s" % (name, result.stderr))
    return 100.0 * parse_ae(result.stderr) / (width * height)


def main(argv=None, run=subprocess.run, which=shutil.which):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--build-dir", required=True, help="diretório de build (contém tests/ui/ghub_ui_tests)")
    parser.add_argument("--diff-dir", required=True, help="onde gravar as imagens de diferença (fora do repositório)")
    parser.add_argument("--capture-dir", default=os.path.expanduser("~/ghub-capturas/estilo"))
    parser.add_argument("--mapping", default=DEFAULT_MAPPING)
    parser.add_argument("--render-dir", help="reutiliza renderizações já feitas (não roda o teste)")
    parser.add_argument("--magick", default="magick")
    args = parser.parse_args(argv)

    if which(args.magick) is None:
        print("O comando '%s' (ImageMagick 7) não foi encontrado; instale o ImageMagick para comparar." % args.magick,
              file=sys.stderr)
        return 2
    mapping = load_mapping(args.mapping)
    os.makedirs(args.diff_dir, exist_ok=True)
    work_dir = os.path.join(args.diff_dir, "work")
    os.makedirs(work_dir, exist_ok=True)
    render_dir = args.render_dir or os.path.join(args.diff_dir, "render")
    try:
        if not args.render_dir:
            os.makedirs(render_dir, exist_ok=True)
            render_screens(args.build_dir, render_dir, run)
        for screen in mapping["screens"]:
            percent = compare_screen(args.magick, screen, mapping, render_dir, args.capture_dir, args.diff_dir,
                                     work_dir, run)
            print("%-28s %6.2f %%  (captura %s)" % (screen["render"], percent, screen["capture"]))
    except (RuntimeError, ValueError, OSError) as error:
        print("erro: %s" % error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
