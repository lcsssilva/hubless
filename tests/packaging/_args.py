"""Separa argv posicional (injetado pelo CMake) dos argumentos do unittest."""
import sys


def pop_args(count):
    args = sys.argv[1 : 1 + count]
    if len(args) != count:
        raise SystemExit(f"esperados {count} argumentos, recebidos {len(args)}")
    del sys.argv[1 : 1 + count]
    return args
